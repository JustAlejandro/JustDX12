#pragma once
#include "PipelineStage\PipelineStage.h"

class ComputePipelineStage : public PipelineStage {
public:
	ComputePipelineStage(Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice);
	void Execute() override;
	void setup(PipeLineStageDesc stageDesc) override;
private:
	void BuildPSO() override;
	void bindDescriptorsToRoot(DESCRIPTOR_USAGE usage = DESCRIPTOR_USAGE_PER_PASS, int usageIndex = 0) override;
};

