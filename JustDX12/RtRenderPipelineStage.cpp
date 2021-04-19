#include "RtRenderPipelineStage.h"
#include "ModelLoading/ModelLoader.h"
#include "ResourceDecay.h"

RtRenderPipelineStage::RtRenderPipelineStage(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice, RtRenderPipelineStageDesc rtDesc, RenderPipelineDesc renderDesc, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect)
	: ScreenRenderPipelineStage(d3dDevice, renderDesc, viewport, scissorRect) {
	rtStageDesc = rtDesc;
	ModelLoader::registerRtUser(this);
}

void RtRenderPipelineStage::deferRebuildRtData(std::vector<std::shared_ptr<Model>> RtModels) {
	enqueue(new RebuildRtDataTask(this, RtModels));
}

void RtRenderPipelineStage::setup(PipeLineStageDesc stageDesc) {
	RenderPipelineStage::setup(stageDesc);
	// Have to remove the root parameter descs that map to RT data
	// speeds up binding and removes clutter
	// TODO: find a cleaner way to do this.
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

void RtRenderPipelineStage::rebuildRtData(std::vector<std::shared_ptr<Model>> RtModels) {
	ResourceDecay::freeDescriptorsAferDelay(&descriptorManager, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, rtDescriptors.indexRange.cpuHandle, rtDescriptors.indexRange.numDescriptors);
	ResourceDecay::freeDescriptorsAferDelay(&descriptorManager, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, rtDescriptors.vertRange.cpuHandle, rtDescriptors.vertRange.numDescriptors);
	ResourceDecay::freeDescriptorsAferDelay(&descriptorManager, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, rtDescriptors.texRange.cpuHandle, rtDescriptors.texRange.numDescriptors);
	ResourceDecay::freeDescriptorsAferDelay(&descriptorManager, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, rtDescriptors.transformRange.cpuHandle, rtDescriptors.transformRange.numDescriptors);
	std::vector<DescriptorJob> texJobVec;
	std::vector<DescriptorJob> indexJobVec;
	std::vector<DescriptorJob> vertexJobVec;
	std::vector<DescriptorJob> transformJobVec;
	UINT index = 0;
	for (auto& model : RtModels) {
		for (auto& mesh : model->meshes) {
			for (UINT i = 0; i < mesh.meshTransform.getInstanceCount(); i++) {
				std::vector<DescriptorJob> meshJobs = buildMeshTexturesDescriptorJobs(&mesh);
				texJobVec.insert(texJobVec.end(), meshJobs.begin(), meshJobs.end());

				DescriptorJob bufferJob;
				bufferJob.autoDesc = false;
				bufferJob.view.srvDesc.Format = DXGI_FORMAT_UNKNOWN;
				bufferJob.view.srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				bufferJob.view.srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				bufferJob.view.srvDesc.Buffer.FirstElement = mesh.startIndexLocation / 3;
				bufferJob.view.srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
				// Since we're using triangle list format, it's more efficient to read 3 values at once.
				bufferJob.view.srvDesc.Buffer.NumElements = mesh.indexCount / 3;
				bufferJob.view.srvDesc.Buffer.StructureByteStride = 12;
				bufferJob.directBinding = true;
				bufferJob.directBindingTarget = model->indexBuffer.get();
				bufferJob.type = DESCRIPTOR_TYPE_SRV;
				bufferJob.usage = DESCRIPTOR_USAGE_SYSTEM_DEFINED;
				indexJobVec.push_back(bufferJob);

				bufferJob.view.srvDesc.Format = DXGI_FORMAT_UNKNOWN;
				bufferJob.view.srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				bufferJob.view.srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				bufferJob.view.srvDesc.Buffer.FirstElement = mesh.baseVertexLocation;
				bufferJob.view.srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
				bufferJob.view.srvDesc.Buffer.NumElements = mesh.vertexCount;
				bufferJob.view.srvDesc.Buffer.StructureByteStride = model->vertexByteStride;
				bufferJob.directBindingTarget = model->vertexBuffer.get();
				vertexJobVec.push_back(bufferJob);

				// Each instance needs a CBV since we have a transform associated with each mesh
				// For now, this is fine, but should either refactor to not be super wasteful, or refactor to remove submesh instancing.
				bufferJob.view.srvDesc.Format = DXGI_FORMAT_UNKNOWN;
				bufferJob.view.srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				bufferJob.view.srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				bufferJob.view.srvDesc.Buffer.FirstElement = i;
				bufferJob.view.srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
				bufferJob.view.srvDesc.Buffer.NumElements = 1;
				bufferJob.view.srvDesc.Buffer.StructureByteStride = sizeof(DirectX::XMFLOAT4X4);
				bufferJob.directBindingTarget = mesh.meshTransform.getResourceForFrame(gFrameIndex);
				transformJobVec.push_back(bufferJob);
			}
		}
		
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

	firstDesc = descriptorManager.makeDescriptors(transformJobVec, &resourceManager, &constantBufferManager, false)[0];
	rtDescriptors.transformRange.cpuHandle = firstDesc.cpuHandle;
	rtDescriptors.transformRange.gpuHandle = firstDesc.gpuHandle;
	rtDescriptors.transformRange.numDescriptors = transformJobVec.size();
}

void RtRenderPipelineStage::draw() {
	mCommandList->SetGraphicsRootShaderResourceView(rtStageDesc.rtTlasSlot, (*rtStageDesc.tlasPtr)->GetGPUVirtualAddress());
	mCommandList->SetGraphicsRootDescriptorTable(rtStageDesc.rtIndexBufferSlot, rtDescriptors.indexRange.gpuHandle);
	mCommandList->SetGraphicsRootDescriptorTable(rtStageDesc.rtVertexBufferSlot, rtDescriptors.vertRange.gpuHandle);
	mCommandList->SetGraphicsRootDescriptorTable(rtStageDesc.rtTexturesSlot, rtDescriptors.texRange.gpuHandle);
	mCommandList->SetGraphicsRootDescriptorTable(rtStageDesc.rtTransformCbvSlot, rtDescriptors.transformRange.gpuHandle);
	ScreenRenderPipelineStage::draw();
}

void RtRenderPipelineStage::RebuildRtDataTask::execute() {
	stage->rebuildRtData(RtModels);
}

RtRenderPipelineStage::RebuildRtDataTask::RebuildRtDataTask(RtRenderPipelineStage* stage, std::vector<std::shared_ptr<Model>> RtModels) {
	this->stage = stage;
	this->RtModels = RtModels;
}