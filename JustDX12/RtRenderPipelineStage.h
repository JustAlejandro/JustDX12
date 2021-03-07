#pragma once
#include "PipelineStage/RenderPipelineStage.h"

class RtRenderPipelineStage : public RenderPipelineStage {
public:
	void DeferRebuildRtData(std::vector<std::shared_ptr<Model>> RtModels);
private:
	void RebuildRtData(std::vector<std::shared_ptr<Model>> RtModels);

	class RebuildRtDataTask : public Task {
	public:
		virtual void execute();
		RebuildRtDataTask(RtRenderPipelineStage* stage, std::vector<std::shared_ptr<Model>> RtModels);
		virtual ~RebuildRtDataTask() override = default;
	protected:
		RtRenderPipelineStage* stage;
		std::vector<std::shared_ptr<Model>> RtModels;
	};

	struct RtData {
		// Used for binding to lookup texture values from shaders.
		CD3DX12_CPU_DESCRIPTOR_HANDLE TexRangeCpuHandle;
		CD3DX12_GPU_DESCRIPTOR_HANDLE TexRangeGpuHandle;
		CD3DX12_CPU_DESCRIPTOR_HANDLE VertRangeCpuHandle;
		CD3DX12_GPU_DESCRIPTOR_HANDLE VertRangeGpuHandle;
		CD3DX12_CPU_DESCRIPTOR_HANDLE IndexRangeCpuHandle;
		CD3DX12_GPU_DESCRIPTOR_HANDLE IndexRangeGpuHandle;
		// These will be used to free the descriptor ranges after they are no longer in use.
		UINT heapStartIndex = 0;
		UINT heapEndIndex = 0;
	};

	RtData rtDescriptors;
};

