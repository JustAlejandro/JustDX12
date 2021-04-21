#pragma once
#include "ScreenRenderPipelineStage.h"

// Describes where the shader/rootsig expect the RT data to be in a DXR 1.1 setup.
struct RtRenderPipelineStageDesc {
	int rtTlasSlot = -1;
	ID3D12Resource** tlasPtr = nullptr;
	// Additional information, able to supply the Index/Vertex/Texture data of all Models in the RT
	// structure, for effects such as reflections, or shadows with masking
	int rtIndexBufferSlot = -1;
	int rtVertexBufferSlot = -1;
	int rtTexturesSlot = -1;
	int rtTransformCbvSlot = -1;
};

// RenderPipelineStage that specifically uses RT data in it's shaders
class RtRenderPipelineStage : public ScreenRenderPipelineStage {
public:
	RtRenderPipelineStage(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice, RtRenderPipelineStageDesc rtDesc, RenderPipelineDesc renderDesc, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect);

	void setup(PipeLineStageDesc stageDesc) override;

	// Enqueues an update operation onto the CPU thread, used by the ModelLoader to let this Stage know there's been a change.
	void deferRebuildRtData(std::vector<std::shared_ptr<BasicModel>> RtModels);

private:

	void rebuildRtData(std::vector<std::shared_ptr<BasicModel>> RtModels);

	void draw() override;

	class RebuildRtDataTask : public Task {
	public:
		virtual void execute();
		RebuildRtDataTask(RtRenderPipelineStage* stage, std::vector<std::shared_ptr<BasicModel>> RtModels);
		virtual ~RebuildRtDataTask() override = default;
	protected:
		RtRenderPipelineStage* stage;
		std::vector<std::shared_ptr<BasicModel>> RtModels;
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
		DescriptorRange transformRange;
	};

	RtData rtDescriptors;
	RtRenderPipelineStageDesc rtStageDesc;
};

