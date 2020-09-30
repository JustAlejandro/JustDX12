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
	//mCommandList->Dispatch(numGroups, SCREEN_HEIGHT, 1);
	mCommandList->Dispatch(numGroupsX, numGroupsY, 1);

	resourceManager.getResource("SSAOOutTexture")->changeState(mCommandList, D3D12_RESOURCE_STATE_COMMON);

	//PIXEndEvent(mCommandList.Get());

	mCommandList->Close();

	ID3D12CommandList* cmdList[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdList), cmdList);
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

void ComputePipelineStage::bindDescriptorsToRoot() {
	for (int i = 0; i < rootParameterDescs.size(); i++) {
		DESCRIPTOR_TYPE descriptorType = getDescriptorTypeFromRootParameterType(rootParameterDescs[i].type);
		switch (descriptorType) {
		case DESCRIPTOR_TYPE_NONE:
			throw "Not sure what this is";
		case DESCRIPTOR_TYPE_SRV:
			mCommandList->SetComputeRootDescriptorTable(i, 
				descriptorManager.getDescriptor(rootParameterDescs[i].name, descriptorType)->gpuHandle);
			break;
		case DESCRIPTOR_TYPE_UAV:
			mCommandList->SetComputeRootDescriptorTable(i,
				descriptorManager.getDescriptor(rootParameterDescs[i].name, descriptorType)->gpuHandle);
			break;
		case DESCRIPTOR_TYPE_CBV:
			mCommandList->SetComputeRootDescriptorTable(i,
				descriptorManager.getDescriptor(rootParameterDescs[i].name, descriptorType)->gpuHandle);
			break;
		default:
			throw "Don't know what to do here.";
			break;
		}
	}
}
