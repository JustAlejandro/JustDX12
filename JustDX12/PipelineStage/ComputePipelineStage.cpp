#include "PipelineStage\ComputePipelineStage.h"

ComputePipelineStage::ComputePipelineStage(Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice)
	: PipelineStage(d3dDevice) {
}

void ComputePipelineStage::Execute() {
	resetCommandList();

	//PIXBeginEvent(mCommandList.Get(), PIX_COLOR(1.0, 0.0, 0.0), "SSAO");

	bindDescriptorHeaps();
	setResourceStates();

	mCommandList->SetComputeRootSignature(rootSignature.Get());

	bindDescriptorsToRoot();

	UINT numGroupsX = (UINT)ceilf(SCREEN_WIDTH / 8.0f);
	UINT numGroupsY = (UINT)ceilf(SCREEN_HEIGHT / 8.0f);
	mCommandList->Dispatch(numGroupsX, numGroupsY, 1);

	resourceManager.getResource("SSAOOutTexture")->changeState(mCommandList, D3D12_RESOURCE_STATE_COMMON);

	//PIXEndEvent(mCommandList.Get());

	mCommandList->Close();

	ID3D12CommandList* cmdList[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdList), cmdList);
}

void ComputePipelineStage::setup(PipeLineStageDesc stageDesc) {
	PipelineStage::setup(stageDesc);
	BuildDescriptors(stageDesc.descriptorJobs);
}

void ComputePipelineStage::BuildPSO() {
	D3D12_COMPUTE_PIPELINE_STATE_DESC computePSO;
	ZeroMemory(&computePSO, sizeof(D3D12_COMPUTE_PIPELINE_STATE_DESC));

	computePSO.pRootSignature = rootSignature.Get();
	computePSO.CS = {
		reinterpret_cast<BYTE*>(shadersByType[SHADER_TYPE_CS]->GetBufferPointer()),
		shadersByType[SHADER_TYPE_CS]->GetBufferSize() };
	computePSO.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	if (md3dDevice->CreateComputePipelineState(&computePSO, IID_PPV_ARGS(&PSO)) < 0) {
		OutputDebugStringA("Compute PSO Setup Failed");
		throw "Compute PSO setup fail";
	}
}

void ComputePipelineStage::bindDescriptorsToRoot(DESCRIPTOR_USAGE usage, int usageIndex) {
	for (int i = 0; i < rootParameterDescs[usage].size(); i++) {
		DESCRIPTOR_TYPE descriptorType = getDescriptorTypeFromRootParameterDesc(rootParameterDescs[usage][i]);
		DX12Descriptor* descriptor = descriptorManager.getDescriptor(rootParameterDescs[usage][i].name + std::to_string(usageIndex), descriptorType);

		switch (descriptorType) {
		case DESCRIPTOR_TYPE_NONE:
			throw "Not sure what this is";
		case DESCRIPTOR_TYPE_SRV:
			mCommandList->SetComputeRootDescriptorTable(rootParameterDescs[usage][i].slot,
				descriptor->gpuHandle);
			break;
		case DESCRIPTOR_TYPE_UAV:
			mCommandList->SetComputeRootDescriptorTable(rootParameterDescs[usage][i].slot,
				descriptor->gpuHandle);
			break;
		case DESCRIPTOR_TYPE_CBV:
			mCommandList->SetComputeRootDescriptorTable(rootParameterDescs[usage][i].slot,
				descriptor->gpuHandle);
			break;
		default:
			throw "Don't know what to do here.";
			break;
		}
	}
}
