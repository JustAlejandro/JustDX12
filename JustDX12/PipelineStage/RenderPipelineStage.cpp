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
	std::vector<RootParamDesc> meshParams;
	meshParams.push_back({ "MeshInfo", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER,
		0, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, DESCRIPTOR_USAGE_PER_MESHLET });
	meshParams.push_back({ "Vertices", ROOT_PARAMETER_TYPE_SRV,
		1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, DESCRIPTOR_USAGE_PER_MESHLET });
	meshParams.push_back({ "Meshlets", ROOT_PARAMETER_TYPE_SRV,
		2, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, DESCRIPTOR_USAGE_PER_MESHLET });
	meshParams.push_back({ "UniqueVertexIndices", ROOT_PARAMETER_TYPE_SRV,
		3, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, DESCRIPTOR_USAGE_PER_MESHLET });
	meshParams.push_back({ "PrimitiveIndices", ROOT_PARAMETER_TYPE_SRV,
		4, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, DESCRIPTOR_USAGE_PER_MESHLET });
	meshParams.push_back({ "MeshletCullData", ROOT_PARAMETER_TYPE_SRV,
		5, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, DESCRIPTOR_USAGE_PER_MESHLET });

	for (RootParamDesc param : stageDesc.rootSigDesc) {
		// Require 8 params for the mesh shader, so we push the root sig back.
		param.slot += 6;
		meshParams.push_back(param);
	}
	PipelineStage::setup(stageDesc);
}

void RenderPipelineStage::Execute() {
	if (!allRenderObjectsSetup) {
		setupRenderObjects();
		return;
	}

	resetCommandList();

	//PIXBeginEvent(mCommandList.GetAddressOf(), PIX_COLOR(0.0, 1.0, 0.0), "Forward Pass");

	bindDescriptorHeaps();
	setResourceStates();

	mCommandList->RSSetViewports(1, &viewport);
	mCommandList->RSSetScissorRects(1, &scissorRect);

	//mCommandList->RSSetShadingRate(D3D12_SHADING_RATE_4X4, nullptr);

	mCommandList->SetGraphicsRootSignature(rootSignature.Get());

	bindDescriptorsToRoot(DESCRIPTOR_USAGE_ALL);
	bindDescriptorsToRoot(DESCRIPTOR_USAGE_PER_PASS);

	bindRenderTarget();

	drawRenderObjects();
	if (renderStageDesc.supportsCulling && occlusionCull) {
		drawOcclusionQuery();
	}

	//PIXEndEvent(mCommandList.GetAddressOf());

	mCommandList->Close();

	ID3D12CommandList* cmdList[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdList), cmdList);
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

	if (shadersByType.find(SHADER_TYPE_MS) != shadersByType.end()) {
		D3DX12_MESH_SHADER_PIPELINE_STATE_DESC meshPSODesc = {};
		meshPSODesc.pRootSignature = meshRootSignature.Get();
		meshPSODesc.MS.pShaderBytecode = shadersByType[SHADER_TYPE_MS]->GetBufferPointer();
		meshPSODesc.MS.BytecodeLength = shadersByType[SHADER_TYPE_MS]->GetBufferSize();
		meshPSODesc.PS = graphicsPSO.PS;
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
		occlusionPSODesc.InputLayout = { inputLayout.data(),(UINT)inputLayout.size() };
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
			&CD3DX12_RESOURCE_DESC::Buffer(renderObjects.size() * 8),
			D3D12_RESOURCE_STATE_PREDICATION,
			nullptr,
			IID_PPV_ARGS(&occlusionQueryResultBuffer));
		D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
		queryHeapDesc.Count = renderObjects.size();
		queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_OCCLUSION;
		md3dDevice->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&occlusionQueryHeap));
	}
}

void RenderPipelineStage::bindDescriptorsToRoot(DESCRIPTOR_USAGE usage, int usageIndex, std::vector<RootParamDesc> curRootParamDescs[DESCRIPTOR_USAGE_MAX]) {
	if (curRootParamDescs == nullptr) {
		curRootParamDescs = rootParameterDescs;
	}

	for (int i = 0; i < rootParameterDescs[usage].size(); i++) {
		DESCRIPTOR_TYPE descriptorType = getDescriptorTypeFromRootParameterDesc(rootParameterDescs[usage][i]);
		if (rootParameterDescs[usage][i].type == ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
			DX12Descriptor* descriptor = descriptorManager.getDescriptor(rootParameterDescs[usage][i].name + std::to_string(usageIndex), descriptorType);
			if (descriptor == nullptr) {
				// For now just ignoring because if a texture doesn't exist we'll just assume it'll be fine.
				// Different PSOs for different texturing would fix this.
				continue;
			}
			switch (descriptorType) {
			case DESCRIPTOR_TYPE_NONE:
				OutputDebugStringA("Not sure what this is");
			case DESCRIPTOR_TYPE_SRV:
				mCommandList->SetGraphicsRootDescriptorTable(rootParameterDescs[usage][i].slot,
					descriptor->gpuHandle);
				break;
			case DESCRIPTOR_TYPE_UAV:
				mCommandList->SetGraphicsRootDescriptorTable(rootParameterDescs[usage][i].slot,
					descriptor->gpuHandle);
				break;
			case DESCRIPTOR_TYPE_CBV:
				mCommandList->SetGraphicsRootDescriptorTable(rootParameterDescs[usage][i].slot,
					descriptor->gpuHandle);
				break;
			default:
				throw "Don't know what to do here.";
				break;
			}
		}
		else {
			D3D12_GPU_VIRTUAL_ADDRESS resource = resourceManager.getResource(rootParameterDescs[usage][i].name)->get()->GetGPUVirtualAddress();
			switch (descriptorType) {
			case DESCRIPTOR_TYPE_NONE:
				OutputDebugStringA("Not sure what this is");
			case DESCRIPTOR_TYPE_SRV:
				mCommandList->SetGraphicsRootShaderResourceView(rootParameterDescs[usage][i].slot, resource);
				break;
			case DESCRIPTOR_TYPE_UAV:
				mCommandList->SetGraphicsRootUnorderedAccessView(rootParameterDescs[usage][i].slot, resource);
				break;
			case DESCRIPTOR_TYPE_CBV:
				mCommandList->SetGraphicsRootConstantBufferView(rootParameterDescs[usage][i].slot, resource);
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

void RenderPipelineStage::drawOcclusionQuery() {
	//PIXScopedEvent(mCommandList.Get(), PIX_COLOR(0.0, 1.0, 0.0), "Draw Calls");
	mCommandList->SetPipelineState(occlusionPSO.Get());
	mCommandList->SetPredication(nullptr, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);
	for (int i = 0; i < renderObjects.size(); i++) {
		Model* model = renderObjects[i];

		bindDescriptorsToRoot(DESCRIPTOR_USAGE_PER_OBJECT, i);

		mCommandList->IASetVertexBuffers(0, 1, &model->vertexBufferView());
		mCommandList->IASetIndexBuffer(&model->indexBufferView());
		mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		mCommandList->BeginQuery(occlusionQueryHeap.Get(), D3D12_QUERY_TYPE_BINARY_OCCLUSION, i);
		mCommandList->DrawInstanced(1, 1, model->boundingBoxVertexLocation, 0);
		mCommandList->EndQuery(occlusionQueryHeap.Get(), D3D12_QUERY_TYPE_BINARY_OCCLUSION, i);
	}
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(occlusionQueryResultBuffer.Get(), D3D12_RESOURCE_STATE_PREDICATION, D3D12_RESOURCE_STATE_COPY_DEST));
	mCommandList->ResolveQueryData(occlusionQueryHeap.Get(), D3D12_QUERY_TYPE_BINARY_OCCLUSION, 0, renderObjects.size(), occlusionQueryResultBuffer.Get(), 0);
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(occlusionQueryResultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PREDICATION));
}

void RenderPipelineStage::importMeshTextures(Mesh* m, int usageIndex) {
	for (auto textureArray : m->textures) {
		for (auto texture : textureArray.second) {
			// Assuming only 1 texture per type...
			resourceManager.importResource(textureArray.first + std::to_string(usageIndex), texture);
		}
	}
}

void RenderPipelineStage::buildMeshTexturesDescriptors(Mesh* m, int usageIndex) {
	std::string diffuse = "default_diff";
	DX12Resource* diffuseTex = resourceManager.getResource(diffuse);
	std::string specular = "default_spec";
	DX12Resource* specularTex = resourceManager.getResource(specular);
	std::string normal = "default_normal";
	DX12Resource* normalTex = resourceManager.getResource(normal);

	if ((m->typeFlags & MODEL_FORMAT_DIFFUSE_TEX) != 0) {
		// Can't bind a texture if it doesn't exist.
		diffuse = "texture_diffuse" + std::to_string(usageIndex);
		diffuseTex = m->textures.at("texture_diffuse")[0];
	}
	if ((m->typeFlags & MODEL_FORMAT_SPECULAR) != 0) {
		specular = "texture_specular" + std::to_string(usageIndex);
		specularTex = m->textures.at("texture_specular")[0];
	}
	if ((m->typeFlags & MODEL_FORMAT_NORMAL_TEX) != 0) {
		normal = "texture_normal" + std::to_string(usageIndex);
		normalTex = m->textures.at("texture_normal")[0];
	}

	DescriptorJob j;
	j.name = "texture_diffuse";
	j.target = diffuse;// "texture_diffuse" + std::to_string(usageIndex);
	j.type = DESCRIPTOR_TYPE_SRV;
	j.usage = DESCRIPTOR_USAGE_PER_MESH;
	j.usageIndex = usageIndex;
	j.srvDesc.Format = diffuseTex->getFormat();
	j.srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	j.srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	j.srvDesc.Texture2D.MipLevels = -1;
	j.srvDesc.Texture2D.MostDetailedMip = 0;
	j.srvDesc.Texture2D.PlaneSlice = 0;
	j.srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	// Here's where we'd put more textures, but for now, this will be it.
	addDescriptorJob(j);
	j.name = "texture_specular";
	j.target = specular;
	j.srvDesc.Format = specularTex->getFormat();
	addDescriptorJob(j);
	j.name = "texture_normal";
	j.target = normal;
	j.srvDesc.Format = normalTex->getFormat();
	addDescriptorJob(j);

	// Flag them all as bound.
	m->texturesBound = true;
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
				importMeshTextures(&mesh, index);
				buildMeshTexturesDescriptors(&mesh, index);
				return;
			}
			index++;
		}
	}
	BuildDescriptors(stageDesc.descriptorJobs);
	BuildQueryHeap();
	allRenderObjectsSetup = true;
}

void RenderPipelineStage::addDescriptorJob(DescriptorJob j) {
	stageDesc.descriptorJobs.push_back(j);
}