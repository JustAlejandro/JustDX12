#pragma once
#include "PipelineStage\PipelineStage.h"

// Just contains data needed to launch correct number of thread groups, group size/layout determined by shader
struct ComputePipelineDesc {
	UINT groupCount[3];
};

// PipelineStage that handles building a command list specifically for compute operations
// Technically a thread that always running, so multiple command lists can be built at once across threads
class ComputePipelineStage : public PipelineStage {
public:
	ComputePipelineStage(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice, ComputePipelineDesc computeDesc);

	void setup(PipeLineStageDesc stageDesc) override;

	void execute() override;

private:
	void buildPSO() override;
	void bindDescriptorsToRoot(DESCRIPTOR_USAGE usage = DESCRIPTOR_USAGE_PER_PASS, int usageIndex = 0, std::vector<RootParamDesc> curRootParamDescs[DESCRIPTOR_USAGE_MAX] = nullptr) override;

	ComputePipelineDesc computeStageDesc;
};

