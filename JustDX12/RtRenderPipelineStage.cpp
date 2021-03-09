#include "RtRenderPipelineStage.h"
#include "ModelLoading/ModelLoader.h"
#include "ResourceDecay.h"

RtRenderPipelineStage::RtRenderPipelineStage(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice, RtRenderPipelineStageDesc rtDesc, RenderPipelineDesc renderDesc, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect)
	: RenderPipelineStage(d3dDevice, renderDesc, viewport, scissorRect) {
	rtStageDesc = rtDesc;
	ModelLoader::registerRtUser(this);
}

void RtRenderPipelineStage::DeferRebuildRtData(std::vector<std::shared_ptr<Model>> RtModels) {
	enqueue(new RebuildRtDataTask(this, RtModels));
}

void RtRenderPipelineStage::setup(PipeLineStageDesc stageDesc) {
	RenderPipelineStage::setup(stageDesc);
	for (int i = 0; i < DESCRIPTOR_USAGE_MAX; i++) {
		auto iter = rootParameterDescs[i].begin();
		while (iter != rootParameterDescs[i].end()) {
			if (iter->slot == rtStageDesc.rtTlasSlot) {
				iter = rootParameterDescs[i].erase(iter);
			}
			else if (iter->slot == rtStageDesc.rtIndexBufferSlot) {
				iter = rootParameterDescs[i].erase(iter);
			}
			else if (iter->slot == rtStageDesc.rtVertexBufferSlot) {
				iter = rootParameterDescs[i].erase(iter);
			}
			else if (iter->slot == rtStageDesc.rtTexturesSlot) {
				iter = rootParameterDescs[i].erase(iter);
			}
			else {
				iter++;
			}
		}
	}
	for (int i = 0; i < DESCRIPTOR_USAGE_MAX; i++) {
		auto iter = meshRootParameterDescs[i].begin();
		while (iter != meshRootParameterDescs[i].end()) {
			if (iter->slot == rtStageDesc.rtTlasSlot) {
				iter = meshRootParameterDescs[i].erase(iter);
			}
			else if (iter->slot == rtStageDesc.rtIndexBufferSlot) {
				iter = meshRootParameterDescs[i].erase(iter);
			}
			else if (iter->slot == rtStageDesc.rtVertexBufferSlot) {
				iter = meshRootParameterDescs[i].erase(iter);
			}
			else if (iter->slot == rtStageDesc.rtTexturesSlot) {
				iter = meshRootParameterDescs[i].erase(iter);
			}
			else {
				iter++;
			}
		}
	}
}

void RtRenderPipelineStage::RebuildRtData(std::vector<std::shared_ptr<Model>> RtModels) {
	ResourceDecay::FreeDescriptorsAferDelay(&descriptorManager, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, rtDescriptors.indexRange.cpuHandle, rtDescriptors.indexRange.numDescriptors);
	ResourceDecay::FreeDescriptorsAferDelay(&descriptorManager, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, rtDescriptors.vertRange.cpuHandle, rtDescriptors.vertRange.numDescriptors);
	ResourceDecay::FreeDescriptorsAferDelay(&descriptorManager, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, rtDescriptors.texRange.cpuHandle, rtDescriptors.texRange.numDescriptors);
	std::vector<DescriptorJob> texJobVec;
	std::vector<DescriptorJob> indexJobVec;
	std::vector<DescriptorJob> vertexJobVec;
	UINT index = 0;
	for (auto& model : RtModels) {
		for (auto& mesh : model->meshes) {
			std::vector<DescriptorJob> meshJobs = buildMeshTexturesDescriptorJobs(&mesh);
			texJobVec.insert(texJobVec.end(), meshJobs.begin(), meshJobs.end());
		}
		DescriptorJob bufferJob;
		bufferJob.autoDesc = false;
		bufferJob.view.srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		bufferJob.view.srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		bufferJob.view.srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		bufferJob.view.srvDesc.Buffer.FirstElement = 0;
		bufferJob.view.srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		// Since we're using triangle list format, it's more efficient to read 3 values at once.
		bufferJob.view.srvDesc.Buffer.NumElements = model->indexCount / 3;
		bufferJob.view.srvDesc.Buffer.StructureByteStride = 12;
		bufferJob.directBinding = true;
		bufferJob.directBindingTarget = model->indexBuffer.get();
		bufferJob.type = DESCRIPTOR_TYPE_SRV;
		bufferJob.usage = DESCRIPTOR_USAGE_SYSTEM_DEFINED;
		indexJobVec.push_back(bufferJob);

		bufferJob.view.srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		bufferJob.view.srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		bufferJob.view.srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		bufferJob.view.srvDesc.Buffer.FirstElement = 0;
		bufferJob.view.srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		bufferJob.view.srvDesc.Buffer.NumElements = model->vertexCount;
		bufferJob.view.srvDesc.Buffer.StructureByteStride = model->vertexByteStride;
		bufferJob.directBindingTarget = model->vertexBuffer.get();
		vertexJobVec.push_back(bufferJob);
	}
	// Need all textures in continuous descriptor table.
	DX12Descriptor firstDesc = descriptorManager.makeDescriptors(indexJobVec, &resourceManager, &constantBufferManager, false)[0];
	rtDescriptors.indexRange.cpuHandle = firstDesc.cpuHandle;
	rtDescriptors.indexRange.gpuHandle = firstDesc.gpuHandle;
	rtDescriptors.indexRange.numDescriptors = indexJobVec.size();

	firstDesc = descriptorManager.makeDescriptors(vertexJobVec, &resourceManager, &constantBufferManager, false)[0];
	rtDescriptors.vertRange.cpuHandle = firstDesc.cpuHandle;
	rtDescriptors.vertRange.gpuHandle = firstDesc.gpuHandle;
	rtDescriptors.vertRange.numDescriptors = vertexJobVec.size();

	firstDesc = descriptorManager.makeDescriptors(texJobVec, &resourceManager, &constantBufferManager, false)[0];
	rtDescriptors.texRange.cpuHandle = firstDesc.cpuHandle;
	rtDescriptors.texRange.gpuHandle = firstDesc.gpuHandle;
	rtDescriptors.texRange.numDescriptors = texJobVec.size();
}

void RtRenderPipelineStage::drawRenderObjects() {
	mCommandList->SetGraphicsRootShaderResourceView(rtStageDesc.rtTlasSlot, (*rtStageDesc.tlasPtr)->GetGPUVirtualAddress());
	mCommandList->SetGraphicsRootDescriptorTable(rtStageDesc.rtIndexBufferSlot, rtDescriptors.indexRange.gpuHandle);
	mCommandList->SetGraphicsRootDescriptorTable(rtStageDesc.rtVertexBufferSlot, rtDescriptors.vertRange.gpuHandle);
	mCommandList->SetGraphicsRootDescriptorTable(rtStageDesc.rtTexturesSlot, rtDescriptors.texRange.gpuHandle);
	RenderPipelineStage::drawRenderObjects();
}

void RtRenderPipelineStage::RebuildRtDataTask::execute() {
	stage->RebuildRtData(RtModels);
}

RtRenderPipelineStage::RebuildRtDataTask::RebuildRtDataTask(RtRenderPipelineStage* stage, std::vector<std::shared_ptr<Model>> RtModels) {
	this->stage = stage;
	this->RtModels = RtModels;
}