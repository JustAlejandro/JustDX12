#pragma once
#include "PipelineStage\PipelineStage.h"

class RenderPipelineStage : public PipelineStage {
public:
	RenderPipelineStage(Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect);
	void Execute() override;
protected:
	void BuildPSO() override;
	D3D12_VIEWPORT viewport;
	D3D12_RECT scissorRect;
};

