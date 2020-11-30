#include "PipelineStage\RenderPipelineStage.h"
#include "DescriptorClasses\DescriptorManager.h"
#include "ModelLoading/ModelLoader.h"
#include "RenderPipelineStageTask.h"
#include <string>
#include "Settings.h"
#include "MeshletModel.h"
#include <d3dx12.h>
#include <DirectXCollision.h>

RenderPipelineStage::RenderPipelineStage(Microsoft::WRL::ComPtr<ID3D12Device2> d3dDevice, RenderPipelineDesc renderDesc, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect)
	: PipelineStage(d3dDevice), renderStageDesc(renderDesc) {
	this->viewport = viewport;
	this->scissorRect = scissorRect;
	frustrumCull = true;
}

void RenderPipelineStage::setup(PipeLineStageDesc stageDesc) {
	if (renderStageDesc.usesMeshlets) {
		BuildRootSignature(meshRootSignature, renderStageDesc.meshletRootSignature, meshRootParameterDescs);
	}

	PipelineStage::setup(stageDesc);
}

void RenderPipelineStage::Execute() {
	resetCommandList();

	if (!allRenderObjectsSetup) {
		setupRenderObjects();
		mCommandList->Close();
		return;
	}

	//PIXBeginEvent(mCommandList.GetAddressOf(), PIX_COLOR(0.0, 1.0, 0.0), "Forward Pass");

	bindDescriptorHeaps();
	setResourceStates();

	mCommandList->RSSetViewports(1, &viewport);
	mCommandList->RSSetScissorRects(1, &scissorRect);

	mCommandList->SetGraphicsRootSignature(rootSignature.Get());

	bindDescriptorsToRoot(DESCRIPTOR_USAGE_ALL);
	bindDescriptorsToRoot(DESCRIPTOR_USAGE_PER_PASS);

	bindRenderTarget();

	drawRenderObjects();

	if (renderStageDesc.usesMeshlets) {
		drawMeshletRenderObjects();
	}

	if (renderStageDesc.supportsCulling && occlusionCull) {
		drawOcclusionQuery();
	}

	//PIXEndEvent(mCommandList.GetAddressOf());

	mCommandList->Close();
}

void RenderPipelineStage::LoadModel(ModelLoader* loader, std::string fileName, std::string dirName) {
	renderObjects.push_back(loader->loadModel(fileName, dirName));
}

void RenderPipelineStage::LoadMeshletModel(ModelLoader* loader, std::string fileName, std::string dirName) {
	meshletRenderObjects.push_back(loader->loadMeshletModel(fileName, dirName));
}

RenderPipelineStage::~RenderPipelineStage() {
	for (Model* m : renderObjects) {
		delete m;
	}
	for (MeshletModel* mm : meshletRenderObjects) {
		delete mm;
	}
}

void RenderPipelineStage::BuildPSO() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPSO;
	ZeroMemory(&graphicsPSO, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	graphicsPSO.InputLayout = { inputLayout.data(),(UINT)inputLayout.size() };
	graphicsPSO.pRootSignature = rootSignature.Get();
	graphicsPSO.VS.pShaderBytecode = shadersByType[SHADER_TYPE_VS]->GetBufferPointer();
	graphicsPSO.VS.BytecodeLength = shadersByType[SHADER_TYPE_VS]->GetBufferSize();
	graphicsPSO.PS.pShaderBytecode = shadersByType[SHADER_TYPE_PS]->GetBufferPointer();
	graphicsPSO.PS.BytecodeLength = shadersByType[SHADER_TYPE_PS]->GetBufferSize();
	graphicsPSO.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	graphicsPSO.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	graphicsPSO.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	graphicsPSO.SampleMask = UINT_MAX;
	graphicsPSO.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	graphicsPSO.NumRenderTargets = renderTargetDescs.size();
	for (int i = 0; i < renderTargetDescs.size(); i++) {
		graphicsPSO.RTVFormats[i] = COLOR_TEXTURE_FORMAT;
	}
	graphicsPSO.SampleDesc.Count = 1;
	graphicsPSO.SampleDesc.Quality = 0;
	graphicsPSO.DSVFormat = DEPTH_TEXTURE_DSV_FORMAT;
	if (FAILED(md3dDevice->CreateGraphicsPipelineState(&graphicsPSO, IID_PPV_ARGS(&PSO)))) {
		OutputDebugStringA("PSO Setup Failed");
		throw "PSO FAIL";
	}

	if (renderStageDesc.usesMeshlets) {
		D3DX12_MESH_SHADER_PIPELINE_STATE_DESC meshPSODesc = {};
		meshPSODesc.pRootSignature = meshRootSignature.Get();
		meshPSODesc.AS.pShaderBytecode = shadersByType[SHADER_TYPE_AS]->GetBufferPointer();
		meshPSODesc.AS.BytecodeLength = shadersByType[SHADER_TYPE_AS]->GetBufferSize();
		meshPSODesc.MS.pShaderBytecode = shadersByType[SHADER_TYPE_MS]->GetBufferPointer();
		meshPSODesc.MS.BytecodeLength = shadersByType[SHADER_TYPE_MS]->GetBufferSize();
		meshPSODesc.PS.pShaderBytecode = shaders["Mesh Pixel Shader"]->GetBufferPointer();
		meshPSODesc.PS.BytecodeLength = shaders["Mesh Pixel Shader"]->GetBufferSize();
		meshPSODesc.NumRenderTargets = renderTargetDescs.size();
		for (int i = 0; i < renderTargetDescs.size(); i++) {
			meshPSODesc.RTVFormats[i] = COLOR_TEXTURE_FORMAT;
		}
		meshPSODesc.SampleDesc.Count = 1;
		meshPSODesc.SampleDesc.Quality = 0;
		meshPSODesc.DSVFormat = DEPTH_TEXTURE_DSV_FORMAT;
		meshPSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		meshPSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		meshPSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		meshPSODesc.SampleMask = UINT_MAX;
		meshPSODesc.SampleDesc = DefaultSampleDesc();

		auto psoStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(meshPSODesc);

		D3D12_PIPELINE_STATE_STREAM_DESC streamDesc;
		streamDesc.pPipelineStateSubobjectStream = &psoStream;
		streamDesc.SizeInBytes = sizeof(psoStream);

		ThrowIfFailed(md3dDevice->CreatePipelineState(&streamDesc, IID_PPV_ARGS(&meshletPSO)));
	}

	if (renderStageDesc.supportsCulling) {
		// Setting up second PSO for predication occlusion.
		D3D12_GRAPHICS_PIPELINE_STATE_DESC occlusionPSODesc = graphicsPSO;
		for (int i = 0; i < renderTargetDescs.size(); i++) {
			occlusionPSODesc.BlendState.RenderTarget[i].RenderTargetWriteMask = 0;
		}
		occlusionPSODesc.InputLayout = { occlusionInputLayout.data(),(UINT)occlusionInputLayout.size() };
		occlusionPSODesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		occlusionPSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		occlusionPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;

		// Need a different set of shaders
		occlusionVS = compileShader(L"..\\Shaders\\BoundingBox.hlsl", {}, L"VS", getCompileTargetFromType(SHADER_TYPE_VS));
		occlusionGS = compileShader(L"..\\Shaders\\BoundingBox.hlsl", {}, L"GS", getCompileTargetFromType(SHADER_TYPE_GS));
		occlusionPS = compileShader(L"..\\Shaders\\BoundingBox.hlsl", {}, L"PS", getCompileTargetFromType(SHADER_TYPE_PS));
		occlusionPSODesc.VS.pShaderBytecode = occlusionVS->GetBufferPointer();
		occlusionPSODesc.VS.BytecodeLength = occlusionVS->GetBufferSize();
		occlusionPSODesc.GS.pShaderBytecode = occlusionGS->GetBufferPointer();
		occlusionPSODesc.GS.BytecodeLength = occlusionGS->GetBufferSize();
		occlusionPSODesc.PS = { 0 };
		occlusionPSODesc.PS.pShaderBytecode = occlusionPS->GetBufferPointer();
		occlusionPSODesc.PS.BytecodeLength = occlusionPS->GetBufferSize();
		if (FAILED(md3dDevice->CreateGraphicsPipelineState(&occlusionPSODesc, IID_PPV_ARGS(&occlusionPSO)))) {
			OutputDebugStringA("Occlusion PSO Setup Failed\n");
			throw "PSO FAIL";
		}
	}
}

void RenderPipelineStage::BuildQueryHeap() {
	if (renderStageDesc.supportsCulling) {
		md3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer((renderObjects.size() + meshletRenderObjects.size()) * 8),
			D3D12_RESOURCE_STATE_PREDICATION,
			nullptr,
			IID_PPV_ARGS(&occlusionQueryResultBuffer));
		D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
		queryHeapDesc.Count = renderObjects.size() + meshletRenderObjects.size();
		queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_OCCLUSION;
		md3dDevice->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&occlusionQueryHeap));
	}
}

void RenderPipelineStage::bindDescriptorsToRoot(DESCRIPTOR_USAGE usage, int usageIndex, std::vector<RootParamDesc> curRootParamDescs[DESCRIPTOR_USAGE_MAX]) {
	if (curRootParamDescs == nullptr) {
		curRootParamDescs = rootParameterDescs;
	}

	for (int i = 0; i < curRootParamDescs[usage].size(); i++) {
		DESCRIPTOR_TYPE descriptorType = getDescriptorTypeFromRootParameterDesc(curRootParamDescs[usage][i]);
		if (curRootParamDescs[usage][i].type == ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
			DX12Descriptor* descriptor = descriptorManager.getDescriptor(curRootParamDescs[usage][i].name + std::to_string(usageIndex), descriptorType);
			if (descriptor == nullptr) {
				// For now just ignoring because if a texture doesn't exist we'll just assume it'll be fine.
				// Different PSOs for different texturing would fix this.
				continue;
			}
			switch (descriptorType) {
			case DESCRIPTOR_TYPE_NONE:
				OutputDebugStringA("Not sure what this is");
			case DESCRIPTOR_TYPE_SRV:
				mCommandList->SetGraphicsRootDescriptorTable(curRootParamDescs[usage][i].slot,
					descriptor->gpuHandle);
				break;
			case DESCRIPTOR_TYPE_UAV:
				mCommandList->SetGraphicsRootDescriptorTable(curRootParamDescs[usage][i].slot,
					descriptor->gpuHandle);
				break;
			case DESCRIPTOR_TYPE_CBV:
				mCommandList->SetGraphicsRootDescriptorTable(curRootParamDescs[usage][i].slot,
					descriptor->gpuHandle);
				break;
			default:
				throw "Don't know what to do here.";
				break;
			}
		}
		else if (descriptorType == DESCRIPTOR_TYPE_CBV) {
			mCommandList->SetGraphicsRootConstantBufferView(curRootParamDescs[usage][i].slot, constantBufferManager.getConstantBuffer(curRootParamDescs[usage][i].name + std::to_string(usageIndex))->get(frameIndex)->GetGPUVirtualAddress());
		}
		else {
			DX12Resource* resource = resourceManager.getResource(curRootParamDescs[usage][i].name);
			if (resource == nullptr) {
				continue;
			}
			D3D12_GPU_VIRTUAL_ADDRESS resPtr = resource->get()->GetGPUVirtualAddress();
			switch (descriptorType) {
			case DESCRIPTOR_TYPE_NONE:
				OutputDebugStringA("Not sure what this is");
			case DESCRIPTOR_TYPE_SRV:
				mCommandList->SetGraphicsRootShaderResourceView(curRootParamDescs[usage][i].slot, resPtr);
				break;
			case DESCRIPTOR_TYPE_UAV:
				mCommandList->SetGraphicsRootUnorderedAccessView(curRootParamDescs[usage][i].slot, resPtr);
				break;
			default:
				throw "Don't know what to do here.";
				break;
			}
		}
	}
}

void RenderPipelineStage::bindRenderTarget() {
	mCommandList->ClearDepthStencilView(
		descriptorManager.getAllDescriptorsOfType(DESCRIPTOR_TYPE_DSV)->at(0)->cpuHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1.0f, 0, 0, nullptr);

#ifdef DEBUG
	float clear[4] = { 0.0f,0.0f,0.0f,0.0f };
	mCommandList->ClearRenderTargetView(descriptorManager.getDescriptor(renderTargetDescs[0].descriptorName + "0", DESCRIPTOR_TYPE_RTV)->cpuHandle,
		clear, 0, nullptr);
#endif // DEBUG

	mCommandList->OMSetRenderTargets(renderTargetDescs.size(),
		&descriptorManager.getDescriptor(renderTargetDescs[0].descriptorName + "0", DESCRIPTOR_TYPE_RTV)->cpuHandle,
		true,
		&descriptorManager.getAllDescriptorsOfType(DESCRIPTOR_TYPE_DSV)->at(0)->cpuHandle);
}

void RenderPipelineStage::drawRenderObjects() {
	//PIXScopedEvent(mCommandList.Get(), PIX_COLOR(0.0, 1.0, 0.0), "Draw Calls");
	int meshIndex = 0;
	int modelIndex = 0;
	if (VRS) {
		resourceManager.getResource("VRS")->changeState(mCommandList, D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE);
		mCommandList->RSSetShadingRateImage(resourceManager.getResource("VRS")->get());
	}
	for (int i = 0; i < renderObjects.size(); i++) {
		Model* model = renderObjects[i];

		if (frustrumCull && (frustrum.Contains(model->boundingBox) == DirectX::ContainmentType::DISJOINT)) {
			meshIndex += model->meshes.size();
			modelIndex++;
			continue;
		}

		bindDescriptorsToRoot(DESCRIPTOR_USAGE_PER_OBJECT, i);

		mCommandList->IASetVertexBuffers(0, 1, &model->vertexBufferView());
		mCommandList->IASetIndexBuffer(&model->indexBufferView());
		mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		if (renderStageDesc.supportsCulling && occlusionCull && frustrum.Contains(model->boundingBox) != (DirectX::ContainmentType::INTERSECTS)) {
			DirectX::ContainmentType containType =  frustrum.Contains(model->boundingBox);
			if (model->boundingBox.Contains(DirectX::XMLoadFloat3(&eyePos)) != DirectX::ContainmentType::CONTAINS) {
				mCommandList->SetPredication(occlusionQueryResultBuffer.Get(), (UINT64)modelIndex * 8, D3D12_PREDICATION_OP_EQUAL_ZERO);
			}
			else {
				mCommandList->SetPredication(nullptr, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);
			}
		}
		else {
			mCommandList->SetPredication(nullptr, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);
		}

		for (Mesh& m : model->meshes) {
			DirectX::ContainmentType frustrumCullResult = frustrum.Contains(m.boundingBox);
			if (frustrumCull && (frustrumCullResult == DirectX::ContainmentType::DISJOINT)) {
				meshIndex++;
				continue;
			}

			if (VRS) {
				D3D12_SHADING_RATE_COMBINER combiners[2] = { D3D12_SHADING_RATE_COMBINER_OVERRIDE, D3D12_SHADING_RATE_COMBINER_MIN };
				mCommandList->RSSetShadingRate(getShadingRateFromDistance(eyePos, m.boundingBox), combiners);
			}

			bindDescriptorsToRoot(DESCRIPTOR_USAGE_PER_MESH, meshIndex);

			mCommandList->DrawIndexedInstanced(m.indexCount,
				1, m.startIndexLocation, m.baseVertexLocation, 0);

			meshIndex++;
		}
		modelIndex++;
	}
}

void RenderPipelineStage::drawMeshletRenderObjects() {
	mCommandList->SetPredication(nullptr, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);
	mCommandList->SetPipelineState(meshletPSO.Get());
	mCommandList->SetGraphicsRootSignature(meshRootSignature.Get());
	bindDescriptorsToRoot(DESCRIPTOR_USAGE_ALL, 0, meshRootParameterDescs);
	bindDescriptorsToRoot(DESCRIPTOR_USAGE_PER_PASS, 0, meshRootParameterDescs);
	if (VRS) {
		resourceManager.getResource("VRS")->changeState(mCommandList, D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE);
		mCommandList->RSSetShadingRateImage(resourceManager.getResource("VRS")->get());
	}
	int modelIndex = 0;
	for (auto& model : meshletRenderObjects) {
		DirectX::ContainmentType containType = frustrum.Contains(model->GetBoundingBox());
		if (containType == DirectX::ContainmentType::DISJOINT) {
			modelIndex++;
			continue;
		}
		if (renderStageDesc.supportsCulling && occlusionCull && containType != (DirectX::ContainmentType::INTERSECTS)) {
			if (model->GetBoundingBox().Contains(DirectX::XMLoadFloat3(&eyePos)) != DirectX::ContainmentType::CONTAINS) {
				mCommandList->SetPredication(occlusionQueryResultBuffer.Get(), (UINT64)(modelIndex + renderObjects.size()) * 8, D3D12_PREDICATION_OP_EQUAL_ZERO);
			}
			else {
				mCommandList->SetPredication(nullptr, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);
			}
		}
		else {
			mCommandList->SetPredication(nullptr, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);
		}
		bindDescriptorsToRoot(DESCRIPTOR_USAGE_PER_MESH, modelIndex, meshRootParameterDescs);
		for (auto& mesh : *model) {
			mCommandList->SetGraphicsRootConstantBufferView(0, mesh.MeshInfoResource->GetGPUVirtualAddress());
			mCommandList->SetGraphicsRootShaderResourceView(1, mesh.VertexResources[0]->GetGPUVirtualAddress());
			mCommandList->SetGraphicsRootShaderResourceView(2, mesh.MeshletResource->GetGPUVirtualAddress());
			mCommandList->SetGraphicsRootShaderResourceView(3, mesh.UniqueVertexIndexResource->GetGPUVirtualAddress());
			mCommandList->SetGraphicsRootShaderResourceView(4, mesh.PrimitiveIndexResource->GetGPUVirtualAddress());
			mCommandList->SetGraphicsRootShaderResourceView(5, mesh.CullDataResource->GetGPUVirtualAddress());
			
			mCommandList->DispatchMesh(DivRoundUp((UINT32)mesh.Meshlets.size(), 32), 1, 1);
		}
		modelIndex++;
	}
}

void RenderPipelineStage::drawOcclusionQuery() {
	//PIXScopedEvent(mCommandList.Get(), PIX_COLOR(0.0, 1.0, 0.0), "Draw Calls");
	// Have to rebind if we're using meshlets.
	mCommandList->SetPredication(nullptr, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);
	mCommandList->SetPipelineState(occlusionPSO.Get());
	mCommandList->SetGraphicsRootSignature(rootSignature.Get());
	bindDescriptorsToRoot(DESCRIPTOR_USAGE_ALL);
	bindDescriptorsToRoot(DESCRIPTOR_USAGE_PER_PASS);
	mCommandList->SetGraphicsRootSignature(rootSignature.Get());
	mCommandList->IASetVertexBuffers(0, 1, &occlusionBoundingBoxBufferView);
	for (int i = 0; i < renderObjects.size() + meshletRenderObjects.size(); i++) {
		bindDescriptorsToRoot(DESCRIPTOR_USAGE_PER_OBJECT, i);

		mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		mCommandList->BeginQuery(occlusionQueryHeap.Get(), D3D12_QUERY_TYPE_BINARY_OCCLUSION, i);
		mCommandList->DrawInstanced(1, 1, i, 0);
		mCommandList->EndQuery(occlusionQueryHeap.Get(), D3D12_QUERY_TYPE_BINARY_OCCLUSION, i);
	}
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(occlusionQueryResultBuffer.Get(), D3D12_RESOURCE_STATE_PREDICATION, D3D12_RESOURCE_STATE_COPY_DEST));
	mCommandList->ResolveQueryData(occlusionQueryHeap.Get(), D3D12_QUERY_TYPE_BINARY_OCCLUSION, 0, renderObjects.size() + meshletRenderObjects.size(), occlusionQueryResultBuffer.Get(), 0);
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(occlusionQueryResultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PREDICATION));
}

void RenderPipelineStage::buildMeshTexturesDescriptors(Mesh* m, int usageIndex) {
	for (const auto& texMap : renderStageDesc.textureToDescriptor) {
		MODEL_FORMAT textureType = texMap.first;
		DX12Resource* texture = nullptr;
		if ((m->typeFlags & textureType) != 0) {
			texture = m->textures.at(textureType);
		}
		else {
			texture = resourceManager.getResource(renderStageDesc.defaultTextures.at(textureType));
		}
		DescriptorJob job(texMap.second, texture, DESCRIPTOR_TYPE_SRV, true, usageIndex, DESCRIPTOR_USAGE_PER_MESH);
		addDescriptorJob(job);
	}
	m->texturesBound = true;
}

void RenderPipelineStage::buildMeshletTexturesDescriptors(MeshletModel* m, int usageIndex) {
	for (const auto& texMap : renderStageDesc.meshletTextureToDescriptor) {
		MODEL_FORMAT textureType = texMap.first;
		DX12Resource* texture = nullptr;
		auto textureIter = m->textures.find(textureType);
		if (textureIter != m->textures.end()) {
			texture = textureIter->second;
		}
		else {
			texture = resourceManager.getResource(renderStageDesc.defaultTextures.at(textureType));
		}
		DescriptorJob job(texMap.second, texture, DESCRIPTOR_TYPE_SRV, true, usageIndex, DESCRIPTOR_USAGE_PER_MESH);
		addDescriptorJob(job);
	}
	m->texturesBound = true;
}

void RenderPipelineStage::BuildInputLayout() {
	PipelineStage::BuildInputLayout();

	occlusionInputLayout = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0 , 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA}
	};
}

void RenderPipelineStage::setupRenderObjects() {
	int index = 0;
	for (auto& renderObj : renderObjects) {
		if (!renderObj->loaded) {
			return;
		}
		for (auto& mesh : renderObj->meshes) {
			if (!mesh.allTexturesLoaded()) {
				return;
			}
			if (!mesh.texturesBound) {
				buildMeshTexturesDescriptors(&mesh, index);
				return;
			}
			index++;
		}
	}
	index = 0;
	for (auto& meshletRenderObj : meshletRenderObjects) {
		if (!meshletRenderObj->loaded) {
			return;
		}
		if (!meshletRenderObj->allTexturesLoaded()) {
			return;
		}
		if (!meshletRenderObj->texturesBound) {
			buildMeshletTexturesDescriptors(meshletRenderObj, index);
			return;
		}
		index++;
	}

	setupOcclusionBoundingBoxes();

	BuildDescriptors(stageDesc.descriptorJobs);
	BuildQueryHeap();
	allRenderObjectsSetup = true;
}

void RenderPipelineStage::setupOcclusionBoundingBoxes() {
	std::vector<CompactBoundingBox> boundingBoxes;
	for (const auto& model : renderObjects) {
		boundingBoxes.emplace_back(model->boundingBox.Center, model->boundingBox.Extents);
	}
	for (const auto& meshletModel : meshletRenderObjects) {
		boundingBoxes.emplace_back(meshletModel->GetBoundingBox().Center, meshletModel->GetBoundingBox().Extents);
	}

	UINT byteSize = (UINT)boundingBoxes.size() * sizeof(CompactBoundingBox);
	D3DCreateBlob(byteSize, &occlusionBoundingBoxBufferCPU);
	CopyMemory(occlusionBoundingBoxBufferCPU->GetBufferPointer(), boundingBoxes.data(), byteSize);

	occlusionBoundingBoxBufferGPU = CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
		boundingBoxes.data(), byteSize, occlusionBoundingBoxBufferGPUUploader);

	occlusionBoundingBoxBufferView.BufferLocation = occlusionBoundingBoxBufferGPU->GetGPUVirtualAddress();
	occlusionBoundingBoxBufferView.StrideInBytes = sizeof(CompactBoundingBox);
	occlusionBoundingBoxBufferView.SizeInBytes = byteSize;
}

void RenderPipelineStage::addDescriptorJob(DescriptorJob j) {
	stageDesc.descriptorJobs.push_back(j);
}