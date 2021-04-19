#pragma once
#include "PipelineStage/RenderPipelineStage.h"

#include "ModelListener.h"

class ModelRenderPipelineStage : public RenderPipelineStage, public ModelListener {
public:
	ModelRenderPipelineStage(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice, RenderPipelineDesc renderDesc, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect);
	~ModelRenderPipelineStage();

protected:
	virtual void buildPSO() override;
	// Inherited via ModelListener
	virtual void processModel(std::weak_ptr<Model> model) override;

	virtual void draw() override;
	virtual void drawModels();
	void drawOcclusionQuery();

	void setupOcclusionBoundingBoxes();
};