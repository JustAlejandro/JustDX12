#pragma once
#include "PipelineStage/RenderPipelineStage.h"

struct RtRenderPipelineStageDesc {
	int rtTlasSlot = -1;
	ID3D12Resource** tlasPtr = nullptr;
	int rtIndexBufferSlot = -1;
	int rtVertexBufferSlot = -1;
	int rtTexturesSlot = -1;
};

class RtRenderPipelineStage : public RenderPipelineStage {
public:
	RtRenderPipelineStage(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice, RtRenderPipelineStageDesc rtDesc, RenderPipelineDesc renderDesc, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect);

	void DeferRebuildRtData(std::vector<std::shared_ptr<Model>> RtModels);

	void setup(PipeLineStageDesc stageDesc) override;
private:
	void RebuildRtData(std::vector<std::shared_ptr<Model>> RtModels);

	void drawRenderObjects() override;

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
		struct DescriptorRange {
			CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle;
			CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle;
			UINT numDescriptors = 0;
		};
		DescriptorRange indexRange;
		DescriptorRange vertRange;
		DescriptorRange texRange;
	};

	RtData rtDescriptors;
	RtRenderPipelineStageDesc rtStageDesc;
};

