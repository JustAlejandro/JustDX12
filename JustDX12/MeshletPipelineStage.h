#pragma once
#include "PipelineStage\RenderPipelineStage.h"

struct MeshletPipelineDesc {
	RenderPipelineDesc renderPipelineDesc;
};

class MeshletPipelineStage : public RenderPipelineStage {
public:
	MeshletPipelineStage(Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice, MeshletPipelineDesc renderDesc, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect);
	void Execute() override;
protected:
	MeshletPipelineDesc renderStageDesc;
	void BuildPSO() override;
};

