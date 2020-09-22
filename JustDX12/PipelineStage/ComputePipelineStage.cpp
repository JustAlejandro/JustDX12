#include "PipelineStage\ComputePipelineStage.h"

ComputePipelineStage::ComputePipelineStage(Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice, PipeLineStageDesc stageDesc)
	: PipelineStage(d3dDevice) {
}

void ComputePipelineStage::Execute() {
	resetCommandList();
	bindDescriptorHeaps();
	setResourceStates();

	mCommandList->SetComputeRootSignature(rootSignature.Get());


}

void ComputePipelineStage::BuildPSO() {
	D3D12_COMPUTE_PIPELINE_STATE_DESC computePSO;
	ZeroMemory(&computePSO, sizeof(D3D12_COMPUTE_PIPELINE_STATE_DESC));

	computePSO.pRootSignature = rootSignature.Get();
	computePSO.CS = {
		reinterpret_cast<BYTE*>(shadersByType[SHADER_TYPE_CS]->GetBufferPointer()),
		shadersByType[SHADER_TYPE_CS]->GetBufferSize() };
	computePSO.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	if (device->CreateComputePipelineState(&computePSO, IID_PPV_ARGS(&PSO)) < 0) {
		OutputDebugStringA("Compute PSO Setup Failed");
		throw "Compute PSO setup fail";
	}
}
