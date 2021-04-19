#include "PipelineStage\RenderPipelineStage.h"
#include "DescriptorClasses\DescriptorManager.h"
#include "ModelLoading/ModelLoader.h"
#include <string>
#include "Settings.h"
#include "MeshletModel.h"
#include <d3dx12.h>
#include <DirectXCollision.h>
#include "ResourceDecay.h"

RenderPipelineStage::RenderPipelineStage(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice, RenderPipelineDesc renderDesc, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect)
	: PipelineStage(d3dDevice), renderStageDesc(renderDesc) {
	this->viewport = viewport;
	this->scissorRect = scissorRect;
	frustrumCull = true;
}

RenderPipelineStage::~RenderPipelineStage() {
}

void RenderPipelineStage::setup(PipeLineStageDesc stageDesc) {
	if (renderStageDesc.usesMeshlets) {
		buildRootSignature(meshRootSignature, renderStageDesc.meshletRootSignature, meshRootParameterDescs);
	}

	PipelineStage::setup(stageDesc);
	buildDescriptors(stageDesc.descriptorJobs);
	buildPSO();
	for (int i = 0; i < DESCRIPTOR_USAGE_MAX; i++) {
		auto iter = rootParameterDescs[i].begin();
		while (iter != rootParameterDescs[i].end()) {
			if (iter->slot == renderStageDesc.perObjTransformCBSlot) {
				iter = rootParameterDescs[i].erase(iter);
			}
			else if (iter->slot == renderStageDesc.perMeshTransformCBSlot) {
				iter = rootParameterDescs[i].erase(iter);
			}
			else if (iter->slot == renderStageDesc.perMeshTextureSlot) {
				iter = rootParameterDescs[i].erase(iter);
			}
			else {
				iter++;
			}
		}
	}
	for (int i = 0; i < DESCRIPTOR_USAGE_MAX; i++) {
		auto iter = meshRootParameterDescs[i].begin();
		while (iter != meshRootParameterDescs[i].end()) {
			if (iter->slot == renderStageDesc.perObjTransformCBMeshletSlot) {
				iter = meshRootParameterDescs[i].erase(iter);
			}
			else if (iter->slot == renderStageDesc.perObjTextureMeshletSlot) {
				iter = meshRootParameterDescs[i].erase(iter);
			}
			else {
				iter++;
			}
		}
	}
}

void RenderPipelineStage::execute() {
	resetCommandList();

	PIXBeginEvent(mCommandList.Get(), PIX_COLOR(0, 255, 0), stageDesc.name.c_str());

	performTransitionsIn();

	bindDescriptorHeaps();
	setResourceStates();

	mCommandList->RSSetViewports(1, &viewport);
	mCommandList->RSSetScissorRects(1, &scissorRect);

	mCommandList->SetGraphicsRootSignature(rootSignature.Get());

	bindDescriptorsToRoot(DESCRIPTOR_USAGE_ALL);
	bindDescriptorsToRoot(DESCRIPTOR_USAGE_PER_PASS);

	bindRenderTarget();

	draw();

	performTransitionsOut();

	PIXEndEvent(mCommandList.Get());

	ThrowIfFailed(mCommandList->Close());
}

void RenderPipelineStage::loadMeshletModel(std::string fileName, std::string dirName, bool usesRT) {
	meshletRenderObjects.push_back(ModelLoader::loadMeshletModel(fileName, dirName, usesRT));
}

void RenderPipelineStage::updateMeshletTransform(UINT modelIndex, DirectX::XMFLOAT4X4 transform) {
	meshletRenderObjects[modelIndex]->transform.setTransform(0, transform);
}

void RenderPipelineStage::buildPSO() {
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
	graphicsPSO.DepthStencilState.DepthEnable = renderStageDesc.usesDepthTex;
	graphicsPSO.SampleMask = UINT_MAX;
	graphicsPSO.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	graphicsPSO.NumRenderTargets = (UINT)renderStageDesc.renderTargets.size();
	for (int i = 0; i < renderStageDesc.renderTargets.size(); i++) {
		graphicsPSO.RTVFormats[i] = descriptorManager.getDescriptor(IndexedName(renderStageDesc.renderTargets[i].descriptorName, 0), DESCRIPTOR_TYPE_RTV)->resourceTarget->getFormat();
	}
	graphicsPSO.SampleDesc.Count = 1;
	graphicsPSO.SampleDesc.Quality = 0;
	graphicsPSO.DSVFormat = renderStageDesc.usesDepthTex ? DEPTH_TEXTURE_DSV_FORMAT : DXGI_FORMAT_UNKNOWN;

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
		meshPSODesc.NumRenderTargets = (UINT)renderStageDesc.renderTargets.size();
		for (UINT i = 0; i < (UINT)renderStageDesc.renderTargets.size(); i++) {
			meshPSODesc.RTVFormats[i] = descriptorManager.getDescriptor(IndexedName(renderStageDesc.renderTargets[i].descriptorName, 0), DESCRIPTOR_TYPE_RTV)->resourceTarget->getFormat();
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
}

void RenderPipelineStage::buildQueryHeap() {
	if (renderStageDesc.supportsCulling) {
		// Due to the occlusion query being used from the previous frame it has a lifetime of 1 more than other resources.
		ResourceDecay::destroyAfterSpecificDelay(occlusionQueryResultBuffer, CPU_FRAME_COUNT + 1);
		ResourceDecay::destroyAfterSpecificDelay(occlusionQueryHeap, CPU_FRAME_COUNT + 1);
		occlusionQueryResultBuffer.Reset();
		occlusionQueryHeap.Reset();

		auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer((renderObjects.size()) * 8);
		md3dDevice->CreateCommittedResource(&gDefaultHeapDesc,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_PREDICATION,
			nullptr,
			IID_PPV_ARGS(&occlusionQueryResultBuffer));
		D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
		queryHeapDesc.NodeMask = 0;
		queryHeapDesc.Count = (UINT)renderObjects.size();
		queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_OCCLUSION;
		md3dDevice->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&occlusionQueryHeap));

		SetName(occlusionQueryResultBuffer.Get(), L"Occlusion Query Result");
		SetName(occlusionQueryHeap.Get(), L"Occlusion Query Heap");
	}
}

std::vector<DescriptorJob> RenderPipelineStage::buildMeshTexturesDescriptorJobs(Mesh* m) {
	std::vector<DescriptorJob> descriptorJobs;
	for (const auto& texMap : renderStageDesc.textureToDescriptor) {
		MODEL_FORMAT textureType = texMap.first;
		DX12Resource* texture = nullptr;
		if ((m->typeFlags & textureType) != 0) {
			texture = m->textures.at(textureType).get();
		}
		else {
			texture = resourceManager.getResource(renderStageDesc.defaultTextures.at(textureType));
		}
		DescriptorJob job(texMap.second, texture, DESCRIPTOR_TYPE_SRV, true, 0, DESCRIPTOR_USAGE_SYSTEM_DEFINED);
		descriptorJobs.push_back(job);
	}
	return descriptorJobs;
}

void RenderPipelineStage::buildMeshletTexturesDescriptors(MeshletModel* m, int usageIndex) {
	std::vector<DX12Descriptor> meshletDescriptors;
	std::vector<DescriptorJob> descriptorJobs;
	for (const auto& texMap : renderStageDesc.meshletTextureToDescriptor) {
		MODEL_FORMAT textureType = texMap.first;
		DX12Resource* texture = nullptr;
		auto textureIter = m->textures.find(textureType);
		if (textureIter != m->textures.end()) {
			texture = textureIter->second.get();
		}
		else {
			texture = resourceManager.getResource(renderStageDesc.defaultTextures.at(textureType));
		}
		DescriptorJob job(texMap.second, texture, DESCRIPTOR_TYPE_SRV, true, usageIndex, DESCRIPTOR_USAGE_SYSTEM_DEFINED);
		descriptorJobs.push_back(job);
	}
	meshletDescriptors = descriptorManager.makeDescriptors(descriptorJobs, &resourceManager, &constantBufferManager, false);
	// TODO: make a similar binding as with the regular models
	m->texturesBound = true;
}

void RenderPipelineStage::buildInputLayout() {
	PipelineStage::buildInputLayout();

	occlusionInputLayout = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0 , 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA}
	};
}

bool RenderPipelineStage::setupRenderObjects() {
	bool newObjectsLoadedOrDeleted = false;

	int index = 0;
	for (auto& meshletRenderObj : meshletRenderObjects) {
		if (!meshletRenderObj->loaded) {
			continue;
		}
		if (!meshletRenderObj->allTexturesLoaded()) {
			continue;
		}
		if (!meshletRenderObj->texturesBound) {
			buildMeshletTexturesDescriptors(meshletRenderObj, index);
			continue;
		}
		index++;
	}

	return newObjectsLoadedOrDeleted;
}

void RenderPipelineStage::bindDescriptorsToRoot(DESCRIPTOR_USAGE usage, int usageIndex, std::vector<RootParamDesc> curRootParamDescs[DESCRIPTOR_USAGE_MAX]) {
	if (curRootParamDescs == nullptr) {
		curRootParamDescs = rootParameterDescs;
	}

	for (int i = 0; i < curRootParamDescs[usage].size(); i++) {
		DESCRIPTOR_TYPE descriptorType = getDescriptorTypeFromRootParameterDesc(curRootParamDescs[usage][i]);
		if (curRootParamDescs[usage][i].type == ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
			DX12Descriptor* descriptor = descriptorManager.getDescriptor(IndexedName(curRootParamDescs[usage][i].name, usageIndex), descriptorType);
			if (descriptor == nullptr) {
				// For now just ignoring because if a texture doesn't exist we'll just assume it'll be fine.
				// Different PSOs for different texturing would fix this.
				continue;
			}
			switch (descriptorType) {
			case DESCRIPTOR_TYPE_NONE:
				OutputDebugStringA("Not sure what this is");
				break;
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
			mCommandList->SetGraphicsRootConstantBufferView(curRootParamDescs[usage][i].slot, constantBufferManager.getConstantBuffer(IndexedName(curRootParamDescs[usage][i].name, usageIndex))->get(gFrameIndex)->GetGPUVirtualAddress());
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
				break;
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

	D3D12_CPU_DESCRIPTOR_HANDLE* depthHandle = nullptr;
	if (renderStageDesc.usesDepthTex) {
		depthHandle = &descriptorManager.getAllDescriptorsOfType(DESCRIPTOR_TYPE_DSV)->at(0)->cpuHandle;

		mCommandList->ClearDepthStencilView(
			*depthHandle,
			D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
			1.0f, 0, 0, nullptr);
	}

	float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	for (auto& target : renderStageDesc.renderTargets) {
		mCommandList->ClearRenderTargetView(descriptorManager.getDescriptor(IndexedName(target.descriptorName, 0), DESCRIPTOR_TYPE_RTV)->cpuHandle,
			clearColor,
			0,
			nullptr);
	}

	mCommandList->OMSetRenderTargets((UINT)renderStageDesc.renderTargets.size(),
		&descriptorManager.getDescriptor(IndexedName(renderStageDesc.renderTargets[0].descriptorName, 0), DESCRIPTOR_TYPE_RTV)->cpuHandle,
		true,
		depthHandle);
}

bool RenderPipelineStage::performsTransitions() {
	return true;
}

void RenderPipelineStage::performTransitionsIn() {
	if (transitionsIn.size() > 0) {
		mCommandList->ResourceBarrier((UINT)transitionsIn.size(), transitionsIn.data());
	}
}

void RenderPipelineStage::performTransitionsOut() {
	if (transitionsOut.size() > 0) {
		mCommandList->ResourceBarrier((UINT)transitionsOut.size(), transitionsOut.data());
	}
}

void RenderPipelineStage::addTransitionIn(DX12Resource* res, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter) {
	transitionsIn.push_back(CD3DX12_RESOURCE_BARRIER::Transition(res->get(), stateBefore, stateAfter));
}

void RenderPipelineStage::addTransitionOut(DX12Resource* res, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter) {
	transitionsOut.push_back(CD3DX12_RESOURCE_BARRIER::Transition(res->get(), stateBefore, stateAfter));
}

void RenderPipelineStage::draw() {
	PIXScopedEvent(mCommandList.Get(), PIX_COLOR(0, 255, 0), "Draw Calls");
	int modelIndex = 0;
	if (renderStageDesc.supportsVRS && VRS && (vrsSupport.VariableShadingRateTier == D3D12_VARIABLE_SHADING_RATE_TIER_2)) {
		D3D12_SHADING_RATE_COMBINER combiners[2] = { D3D12_SHADING_RATE_COMBINER_OVERRIDE, D3D12_SHADING_RATE_COMBINER_OVERRIDE };
		mCommandList->RSSetShadingRate(D3D12_SHADING_RATE_1X1, combiners);
		mCommandList->RSSetShadingRateImage(resourceManager.getResource(renderStageDesc.VrsTextureName)->get());
	}
	for (int i = 0; i < renderObjects.size(); i++) {
		std::shared_ptr<Model> model = renderObjects[i].lock();
		if (!model) {
			renderObjects.erase(renderObjects.begin() + i);
			i--;
			modelIndex++;
			continue;
		}

		bindDescriptorsToRoot(DESCRIPTOR_USAGE_PER_OBJECT, i);
		model->transform.bindTransformToRoot(renderStageDesc.perObjTransformCBSlot, gFrameIndex, mCommandList.Get());

		auto vertexBufferView = model->getVertexBufferView();
		auto indexBufferView = model->getIndexBufferView();
		mCommandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		mCommandList->IASetIndexBuffer(&indexBufferView);
		mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		for (Mesh& m : model->meshes) {
			if (VRS && (vrsSupport.VariableShadingRateTier == D3D12_VARIABLE_SHADING_RATE_TIER_1)) {
				D3D12_SHADING_RATE_COMBINER combiners[2] = { D3D12_SHADING_RATE_COMBINER_OVERRIDE, D3D12_SHADING_RATE_COMBINER_OVERRIDE };
				mCommandList->RSSetShadingRate(getShadingRateFromDistance(eyePos, m.boundingBox), combiners);
			}

			m.meshTransform.bindTransformToRoot(renderStageDesc.perMeshTransformCBSlot, gFrameIndex, mCommandList.Get());
			if (renderStageDesc.perMeshTextureSlot > -1) {
				mCommandList->SetGraphicsRootDescriptorTable(renderStageDesc.perMeshTextureSlot, m.getDescriptorsForStage(this)[0].gpuHandle);
			}

			mCommandList->DrawIndexedInstanced(m.indexCount,
				model->transform.getInstanceCount() * m.meshTransform.getInstanceCount(), m.startIndexLocation, m.baseVertexLocation, 0);

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
	if (renderStageDesc.supportsVRS && VRS && (vrsSupport.VariableShadingRateTier == D3D12_VARIABLE_SHADING_RATE_TIER_2)) {
		mCommandList->RSSetShadingRateImage(resourceManager.getResource(renderStageDesc.VrsTextureName)->get());
	}
	int modelIndex = 0;
	for (auto& model : meshletRenderObjects) {
		DirectX::BoundingBox modelBB;
		auto transform = model->transform.getTransform(modelIndex);
		model->GetBoundingBox().Transform(modelBB, DirectX::XMLoadFloat4x4(&transform));
		DirectX::ContainmentType containType = frustrum.Contains(modelBB);
		if (containType == DirectX::ContainmentType::DISJOINT) {
			modelIndex++;
			continue;
		}
		if (renderStageDesc.supportsCulling && occlusionCull && containType != (DirectX::ContainmentType::INTERSECTS)) {
			if (modelBB.Contains(DirectX::XMLoadFloat3(&eyePos)) != DirectX::ContainmentType::CONTAINS) {
				mCommandList->SetPredication(occlusionQueryResultBuffer.Get(), (UINT64)(modelIndex + renderObjects.size()) * 8, D3D12_PREDICATION_OP_EQUAL_ZERO);
			}
			else {
				mCommandList->SetPredication(nullptr, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);
			}
		}
		else {
			mCommandList->SetPredication(nullptr, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);
		}
		bindDescriptorsToRoot(DESCRIPTOR_USAGE_PER_OBJECT, modelIndex, meshRootParameterDescs);

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

std::vector<std::pair<D3D12_RESOURCE_STATES, DX12Resource*>> RenderPipelineStage::getRequiredResourceStates() {
	auto requiredStates = PipelineStage::getRequiredResourceStates();
	if (renderStageDesc.supportsVRS) {
		requiredStates.push_back(std::make_pair(D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE, resourceManager.getResource(renderStageDesc.VrsTextureName)));
	}
	return requiredStates;
}