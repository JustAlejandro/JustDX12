#include "PipelineStage\ComputePipelineStage.h"

#include "IndexedName.h"

ComputePipelineStage::ComputePipelineStage(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice, ComputePipelineDesc computeDesc)
	: PipelineStage(d3dDevice, D3D12_COMMAND_LIST_TYPE_COMPUTE), computeStageDesc(computeDesc) {
}

void ComputePipelineStage::setup(PipeLineStageDesc stageDesc) {
	PipelineStage::setup(stageDesc);
	buildDescriptors(stageDesc.descriptorJobs);
	buildPSO();
}

void ComputePipelineStage::execute() {
	resetCommandList();

	PIXBeginEvent(mCommandList.Get(), PIX_COLOR(255, 0, 0), stageDesc.name.c_str());

	bindDescriptorHeaps();
	setResourceStates();

	mCommandList->SetComputeRootSignature(rootSignature.Get());

	bindDescriptorsToRoot(DESCRIPTOR_USAGE_ALL);
	bindDescriptorsToRoot();

	mCommandList->Dispatch(computeStageDesc.groupCount[0], computeStageDesc.groupCount[1], computeStageDesc.groupCount[2]);

	PIXEndEvent(mCommandList.Get());

	ThrowIfFailed(mCommandList->Close());
}

void ComputePipelineStage::buildPSO() {
	D3D12_COMPUTE_PIPELINE_STATE_DESC computePSO;
	ZeroMemory(&computePSO, sizeof(D3D12_COMPUTE_PIPELINE_STATE_DESC));

	computePSO.pRootSignature = rootSignature.Get();
	computePSO.CS = {
		reinterpret_cast<BYTE*>(shadersByType[SHADER_TYPE_CS]->GetBufferPointer()),
		shadersByType[SHADER_TYPE_CS]->GetBufferSize() };
	computePSO.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	ThrowIfFailed(md3dDevice->CreateComputePipelineState(&computePSO, IID_PPV_ARGS(&PSO)));
}

void ComputePipelineStage::bindDescriptorsToRoot(DESCRIPTOR_USAGE usage, int usageIndex, std::vector<RootParamDesc> curRootParamDescs[DESCRIPTOR_USAGE_MAX]) {
	if (curRootParamDescs == nullptr) {
		curRootParamDescs = rootParameterDescs;
	}

	for (int i = 0; i < curRootParamDescs[usage].size(); i++) {
		DESCRIPTOR_TYPE descriptorType = getDescriptorTypeFromRootParameterDesc(curRootParamDescs[usage][i]);

		if (curRootParamDescs[usage][i].type == ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
			DX12Descriptor* descriptor = descriptorManager.getDescriptor(IndexedName(curRootParamDescs[usage][i].name, usageIndex), descriptorType);

			switch (descriptorType) {
			case DESCRIPTOR_TYPE_NONE:
				throw "Not sure what this is";
			case DESCRIPTOR_TYPE_SRV:
				mCommandList->SetComputeRootDescriptorTable(curRootParamDescs[usage][i].slot,
					descriptor->gpuHandle);
				break;
			case DESCRIPTOR_TYPE_UAV:
				mCommandList->SetComputeRootDescriptorTable(curRootParamDescs[usage][i].slot,
					descriptor->gpuHandle);
				break;
			case DESCRIPTOR_TYPE_CBV:
				mCommandList->SetComputeRootDescriptorTable(curRootParamDescs[usage][i].slot,
					descriptor->gpuHandle);
				break;
			default:
				throw "Don't know what to do here.";
				break;
			}
		}
		else if (descriptorType == DESCRIPTOR_TYPE_CBV) {
			mCommandList->SetComputeRootConstantBufferView(curRootParamDescs[usage][i].slot, constantBufferManager.getConstantBuffer(IndexedName(curRootParamDescs[usage][i].name, usageIndex))->get(gFrameIndex)->GetGPUVirtualAddress());
		}
		else {
			D3D12_GPU_VIRTUAL_ADDRESS resource = resourceManager.getResource(curRootParamDescs[usage][i].name)->get()->GetGPUVirtualAddress();
			switch (descriptorType) {
			case DESCRIPTOR_TYPE_NONE:
				OutputDebugStringA("Not sure what this is");
				break;
			case DESCRIPTOR_TYPE_SRV:
				mCommandList->SetComputeRootShaderResourceView(curRootParamDescs[usage][i].slot, resource);
				break;
			case DESCRIPTOR_TYPE_UAV:
				mCommandList->SetComputeRootUnorderedAccessView(curRootParamDescs[usage][i].slot, resource);
				break;
			default:
				throw "Don't know what to do here.";
				break;
			}
		}
	}
}
