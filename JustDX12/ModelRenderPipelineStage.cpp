#include "ModelRenderPipelineStage.h"

#include "ModelLoading/Model.h"
#include <ResourceDecay.h>

ModelRenderPipelineStage::ModelRenderPipelineStage(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice, RenderPipelineDesc renderDesc, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect)
	: RenderPipelineStage(d3dDevice, renderDesc, viewport, scissorRect) {

}

ModelRenderPipelineStage::~ModelRenderPipelineStage() {
}

void ModelRenderPipelineStage::buildPSO() {
	RenderPipelineStage::buildPSO();

	if (renderStageDesc.supportsCulling) {
		// Setting up second PSO for predication occlusion.
		D3D12_GRAPHICS_PIPELINE_STATE_DESC occlusionPSODesc;
		ZeroMemory(&occlusionPSODesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
		occlusionPSODesc.pRootSignature = rootSignature.Get();
		occlusionPSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		occlusionPSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		occlusionPSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		occlusionPSODesc.DepthStencilState.DepthEnable = renderStageDesc.usesDepthTex;
		occlusionPSODesc.SampleMask = UINT_MAX;
		occlusionPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		occlusionPSODesc.NumRenderTargets = (UINT)renderStageDesc.renderTargets.size();
		for (int i = 0; i < renderStageDesc.renderTargets.size(); i++) {
			occlusionPSODesc.RTVFormats[i] = descriptorManager.getDescriptor(IndexedName(renderStageDesc.renderTargets[i].descriptorName, 0), DESCRIPTOR_TYPE_RTV)->resourceTarget->getFormat();
			occlusionPSODesc.BlendState.RenderTarget[i].RenderTargetWriteMask = 0;
		}
		occlusionPSODesc.SampleDesc.Count = 1;
		occlusionPSODesc.SampleDesc.Quality = 0;
		occlusionPSODesc.DSVFormat = renderStageDesc.usesDepthTex ? DEPTH_TEXTURE_DSV_FORMAT : DXGI_FORMAT_UNKNOWN;
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

void ModelRenderPipelineStage::draw() {
	drawModels();

	bool modelAmountChanged = processNewModels();

	if (false && modelAmountChanged && renderStageDesc.supportsCulling) {
		setupOcclusionBoundingBoxes();
		buildQueryHeap();
	}

	if (false && renderStageDesc.supportsCulling && occlusionCull) {
		drawOcclusionQuery();
	}
}

void ModelRenderPipelineStage::drawModels() {
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

		std::vector<DirectX::BoundingBox> boundingBoxes;
		for (UINT i = 0; i < model->transform.getInstanceCount(); i++) {
			DirectX::BoundingBox instanceBB;
			model->boundingBox.Transform(instanceBB, TransposeLoad(model->transform.getTransform(i)));
			boundingBoxes.push_back(instanceBB);
		}
		if (frustrumCull && std::all_of(boundingBoxes.begin(), boundingBoxes.end(), [this](DirectX::BoundingBox b) { return frustrum.Contains(b) == DirectX::ContainmentType::DISJOINT; })) {
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

		if (false && renderStageDesc.supportsCulling && occlusionCull && std::all_of(boundingBoxes.begin(), boundingBoxes.end(), [this](DirectX::BoundingBox b) { return frustrum.Contains(b) != DirectX::ContainmentType::INTERSECTS; })) {
			if (std::all_of(boundingBoxes.begin(), boundingBoxes.end(), [this](DirectX::BoundingBox b) { return frustrum.Contains(DirectX::XMLoadFloat3(&eyePos)) != DirectX::ContainmentType::CONTAINS; })) {
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
			std::vector<DirectX::BoundingBox> meshBoundingBoxes;
			for (UINT i = 0; i < model->transform.getInstanceCount(); i++) {
				DirectX::BoundingBox instanceMeshBB;
				DirectX::BoundingBox subInstanceMeshBB;
				for (UINT j = 0; j < m.meshTransform.getInstanceCount(); j++) {
					m.boundingBox.Transform(instanceMeshBB, TransposeLoad(m.meshTransform.getTransform(j)));
					instanceMeshBB.Transform(subInstanceMeshBB, TransposeLoad(model->transform.getTransform(i)));
					meshBoundingBoxes.push_back(subInstanceMeshBB);
				}
			}
			if (frustrumCull && std::all_of(meshBoundingBoxes.begin(), meshBoundingBoxes.end(), [this](DirectX::BoundingBox b) {return frustrum.Contains(b) == DirectX::ContainmentType::DISJOINT; })) {
				continue;
			}

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

void ModelRenderPipelineStage::drawOcclusionQuery() {
	PIXScopedEvent(mCommandList.Get(), PIX_COLOR(0, 255, 0), "Draw Calls");
	// Have to rebind if we're using meshlets.
	mCommandList->SetPredication(nullptr, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);
	mCommandList->SetPipelineState(occlusionPSO.Get());
	mCommandList->SetGraphicsRootSignature(rootSignature.Get());
	bindDescriptorsToRoot(DESCRIPTOR_USAGE_ALL);
	bindDescriptorsToRoot(DESCRIPTOR_USAGE_PER_PASS);
	mCommandList->SetGraphicsRootSignature(rootSignature.Get());
	mCommandList->IASetVertexBuffers(0, 1, &occlusionBoundingBoxBufferView);
	for (int i = 0; i < renderObjects.size(); i++) {
		auto mPtr = renderObjects[i].lock();
		if (!mPtr) {
			continue;
		}
		bindDescriptorsToRoot(DESCRIPTOR_USAGE_PER_OBJECT, i);
		mPtr->transform.bindTransformToRoot(renderStageDesc.perObjTransformCBSlot, gFrameIndex, mCommandList.Get());
		mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		mCommandList->BeginQuery(occlusionQueryHeap.Get(), D3D12_QUERY_TYPE_BINARY_OCCLUSION, i);
		mCommandList->DrawInstanced(1, mPtr->transform.getInstanceCount(), i, 0);
		mCommandList->EndQuery(occlusionQueryHeap.Get(), D3D12_QUERY_TYPE_BINARY_OCCLUSION, i);
	}
	auto copyTransition = CD3DX12_RESOURCE_BARRIER::Transition(occlusionQueryResultBuffer.Get(), D3D12_RESOURCE_STATE_PREDICATION, D3D12_RESOURCE_STATE_COPY_DEST);
	mCommandList->ResourceBarrier(1, &copyTransition);
	mCommandList->ResolveQueryData(occlusionQueryHeap.Get(), D3D12_QUERY_TYPE_BINARY_OCCLUSION, 0, (UINT)(renderObjects.size()), occlusionQueryResultBuffer.Get(), 0);
	auto predTransition = CD3DX12_RESOURCE_BARRIER::Transition(occlusionQueryResultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PREDICATION);
	mCommandList->ResourceBarrier(1, &predTransition);
}

void ModelRenderPipelineStage::setupOcclusionBoundingBoxes() {
	std::vector<CompactBoundingBox> boundingBoxes;
	for (const auto& model : renderObjects) {
		if (auto modelPtr = model.lock()) {
			boundingBoxes.emplace_back(modelPtr->boundingBox.Center, modelPtr->boundingBox.Extents);
		}
		else {
			// if the model was actually unloaded, this bogus 'padding' is necessary to keep indexing consistent when drawing BBs
			boundingBoxes.emplace_back(CompactBoundingBox(DirectX::XMFLOAT3(), DirectX::XMFLOAT3()));
		}
	}

	UINT byteSize = (UINT)boundingBoxes.size() * sizeof(CompactBoundingBox);

	ResourceDecay::destroyAfterSpecificDelay(occlusionBoundingBoxBufferGPU, CPU_FRAME_COUNT + 1);
	occlusionBoundingBoxBufferGPU.Reset();
	ResourceDecay::destroyAfterSpecificDelay(occlusionBoundingBoxBufferGPUUploader, CPU_FRAME_COUNT + 1);
	occlusionBoundingBoxBufferGPUUploader.Reset();

	occlusionBoundingBoxBufferGPU = CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
		boundingBoxes.data(), byteSize, occlusionBoundingBoxBufferGPUUploader);

	SetName(occlusionBoundingBoxBufferGPU.Get(), L"Occlusion BB Buffer GPU");
	SetName(occlusionBoundingBoxBufferGPUUploader.Get(), L"Occlusion BB Buffer GPU Uploader");

	auto transToVertexBuffer = CD3DX12_RESOURCE_BARRIER::Transition(occlusionBoundingBoxBufferGPU.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	mCommandList->ResourceBarrier(1, &transToVertexBuffer);

	occlusionBoundingBoxBufferView.BufferLocation = occlusionBoundingBoxBufferGPU->GetGPUVirtualAddress();
	occlusionBoundingBoxBufferView.StrideInBytes = sizeof(CompactBoundingBox);
	occlusionBoundingBoxBufferView.SizeInBytes = byteSize;
}
