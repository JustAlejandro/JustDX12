#include "ModelRenderPipelineStage.h"

ModelRenderPipelineStage::ModelRenderPipelineStage(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice, RenderPipelineDesc renderDesc, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect)
	: RenderPipelineStage(d3dDevice, renderDesc, viewport, scissorRect) {

}

ModelRenderPipelineStage::~ModelRenderPipelineStage() {
}

void ModelRenderPipelineStage::draw() {
	RenderPipelineStage::draw();

	if (renderStageDesc.usesMeshlets) {
		drawMeshletRenderObjects();
	}

	bool modelAmountChanged = processNewModels();// setupRenderObjects();

	if (modelAmountChanged && renderStageDesc.supportsCulling) {
		setupOcclusionBoundingBoxes();
		buildQueryHeap();
	}

	if (renderStageDesc.supportsCulling && occlusionCull) {
		drawOcclusionQuery();
	}
}
