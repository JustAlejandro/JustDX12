#include "MeshletRenderPipelineStage.h"
#include "MeshletModel.h"

MeshletRenderPipelineStage::MeshletRenderPipelineStage(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice, RenderPipelineDesc renderDesc, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect) 
	: RenderPipelineStage(d3dDevice, renderDesc, viewport, scissorRect) {
}

void MeshletRenderPipelineStage::buildPSO() {
	D3DX12_MESH_SHADER_PIPELINE_STATE_DESC meshPSODesc = {};
	meshPSODesc.pRootSignature = rootSignature.Get();
	meshPSODesc.AS.pShaderBytecode = shadersByType[SHADER_TYPE_AS]->GetBufferPointer();
	meshPSODesc.AS.BytecodeLength = shadersByType[SHADER_TYPE_AS]->GetBufferSize();
	meshPSODesc.MS.pShaderBytecode = shadersByType[SHADER_TYPE_MS]->GetBufferPointer();
	meshPSODesc.MS.BytecodeLength = shadersByType[SHADER_TYPE_MS]->GetBufferSize();
	meshPSODesc.PS.pShaderBytecode = shadersByType[SHADER_TYPE_PS]->GetBufferPointer();
	meshPSODesc.PS.BytecodeLength = shadersByType[SHADER_TYPE_PS]->GetBufferSize();
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

	ThrowIfFailed(md3dDevice->CreatePipelineState(&streamDesc, IID_PPV_ARGS(&PSO)));
}

void MeshletRenderPipelineStage::buildMeshletTexturesDescriptors(MeshletModel* m, int usageIndex) {
	std::vector<DX12Descriptor> meshletDescriptors;
	std::vector<DescriptorJob> descriptorJobs;
	for (const auto& texMap : renderStageDesc.textureToDescriptor) {
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
	meshletTexDescs.push_back(meshletDescriptors[0]);
	// TODO: make a similar binding as with the regular models
	m->texturesBound = true;
}

void MeshletRenderPipelineStage::processModel(std::weak_ptr<Model> model) {
	if (auto ptr = model.lock()) {
		// Runtime polymorphism is bad, but it keeps the modelLoader broadcast simple... So for now I'll just deal with it
		// this only gets run once per object per time loaded anyway.
		if (auto basicModel = dynamic_pointer_cast<MeshletModel>(ptr)) {
			buildMeshletTexturesDescriptors(basicModel.get(), meshletIndex++);
			meshletRenderObjects.push_back(basicModel);
		}
	}
}

void MeshletRenderPipelineStage::draw() {
	processNewModels();

	mCommandList->SetPredication(nullptr, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);
	mCommandList->SetPipelineState(PSO.Get());
	mCommandList->SetGraphicsRootSignature(rootSignature.Get());
	bindDescriptorsToRoot(DESCRIPTOR_USAGE_ALL, 0);
	bindDescriptorsToRoot(DESCRIPTOR_USAGE_PER_PASS, 0);
	if (renderStageDesc.supportsVRS && VRS && (vrsSupport.VariableShadingRateTier == D3D12_VARIABLE_SHADING_RATE_TIER_2)) {
		mCommandList->RSSetShadingRateImage(resourceManager.getResource(renderStageDesc.VrsTextureName)->get());
	}
	int modelIndex = 0;
	for (auto& pWeakModel : meshletRenderObjects) {
		auto model = pWeakModel.lock();
		if (!model.get()) {
			// binding is index based for just now. So unfortunately this is needed.
			modelIndex++;
			continue;
		}
		DirectX::BoundingBox modelBB;
		auto transform = model->getTransform(modelIndex);
		model->GetBoundingBox().Transform(modelBB, DirectX::XMLoadFloat4x4(&transform));
		DirectX::ContainmentType containType = frustrum.Contains(modelBB);
		if (containType == DirectX::ContainmentType::DISJOINT) {
			modelIndex++;
			continue;
		}
		bindDescriptorsToRoot(DESCRIPTOR_USAGE_PER_OBJECT, modelIndex);
		if (renderStageDesc.perMeshTextureSlot > -1) {
			mCommandList->SetGraphicsRootDescriptorTable(renderStageDesc.perMeshTextureSlot, meshletTexDescs[modelIndex].gpuHandle);
		}

		model->bindTransformToRoot(renderStageDesc.perObjTransformCBSlot, gFrameIndex, mCommandList.Get());

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