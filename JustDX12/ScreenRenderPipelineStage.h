#pragma once
#include "PipelineStage/RenderPipelineStage.h"

class ScreenRenderPipelineStage : public RenderPipelineStage {
public:
	ScreenRenderPipelineStage(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice, RenderPipelineDesc renderDesc, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect);

protected:
	void buildInputLayout() override;

	virtual void draw() override;
};

