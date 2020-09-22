#include "SSAO.h"
#include <math.h>

SSAO::SSAO(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format, UINT descriptorSize) :
	descriptorManager(device), resourceManager(device) {
	this->mFormat = format;
	this->device = device;
	this->mHeight = height;
	this->mWidth = width;
	this->descriptorSize = descriptorSize;
	BuildResources();
}

ID3D12Resource* SSAO::output() {
	return outputRes.Get();
}

void SSAO::BuildDescriptors(ID3D12Resource* input) {
	std::vector<DescriptorJob> jobs{ {}, {}, {} };
	jobs[0].name = "SSAO";
	jobs[0].type = DESCRIPTOR_TYPE_SRV;
	jobs[0].target = out;
	jobs[0].srvDesc = DEFAULT_SRV_DESC();
	jobs[1].name = "SSAO";
	jobs[1].target = out;
	jobs[1].type = DESCRIPTOR_TYPE_UAV;
	jobs[1].uavDesc = DEFAULT_UAV_DESC();
	jobs[2].name = "SSAO Input";
	jobs[2].type = DESCRIPTOR_TYPE_SRV;
	jobs[2].srvDesc = DEFAULT_SRV_DESC();
	jobs[2].target = resourceManager.makeFromExisting("SSAO Input", DESCRIPTOR_TYPE_SRV, input, D3D12_RESOURCE_STATE_COMMON);
	descriptorManager.makeDescriptorHeap(jobs, true);
}

void SSAO::BuildResources() {
	out = resourceManager.makeResource("SSAO", DESCRIPTOR_TYPE_UAV | DESCRIPTOR_TYPE_SRV);
	outputRes = out->get();
	outputRes->SetName(L"SSAO Out Tex");
}

void SSAO::Execute(ID3D12GraphicsCommandList* cmdList, ID3D12RootSignature* rootSig,
	ID3D12PipelineState* pso, ID3D12Resource* input, CD3DX12_GPU_DESCRIPTOR_HANDLE inputDes) {
	cmdList->SetComputeRootSignature(rootSig);

	int rayAmount = 50;
	float maxRange = 10.0f;
	float minRange = 1.0f;
	int TAA = 0;
	ID3D12DescriptorHeap* heaps[1] = {descriptorManager.getDescriptor("SSAO Input", DESCRIPTOR_TYPE_SRV)->descriptorHeap.Get()};
	cmdList->SetDescriptorHeaps(1, heaps);

	cmdList->SetComputeRoot32BitConstants(0, 1, &rayAmount, 0);
	cmdList->SetComputeRoot32BitConstants(0, 1, &maxRange, 1);
	cmdList->SetComputeRoot32BitConstants(0, 1, &minRange, 2);
	cmdList->SetComputeRoot32BitConstants(0, 1, &TAA, 3);

	out->changeState(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	cmdList->SetPipelineState(pso);

	//cmdList->SetComputeRootDescriptorTable(1, inGpuSRV);
	cmdList->SetComputeRootDescriptorTable(1, descriptorManager.getDescriptor("SSAO Input", DESCRIPTOR_TYPE_SRV)->gpuHandle);
	//cmdList->SetComputeRootDescriptorTable(2, outGpuUAV);
	cmdList->SetComputeRootDescriptorTable(2, descriptorManager.getDescriptor("SSAO", DESCRIPTOR_TYPE_UAV)->gpuHandle);

	UINT numGroups = (UINT)ceilf(mWidth / 256.0f);
	cmdList->Dispatch(numGroups, mHeight, 1);

	out->changeState(cmdList, D3D12_RESOURCE_STATE_COMMON);
}
