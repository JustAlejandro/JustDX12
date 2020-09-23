#pragma once
#include "PipelineStage\PipelineStage.h"

class ComputePipelineStage : public PipelineStage {
public:
	ComputePipelineStage(Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice);
	void Execute() override;
private:
	void BuildPSO() override;
	void bindDescriptorsToRoot() override;
};

