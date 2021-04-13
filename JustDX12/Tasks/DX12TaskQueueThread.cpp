#include "Tasks\DX12TaskQueueThread.h"
#include "DX12Helper.h"
#include "Settings.h"

DX12TaskQueueThread::DX12TaskQueueThread(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice, D3D12_COMMAND_LIST_TYPE cmdListType) : md3dDevice(d3dDevice) {
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = cmdListType;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue));

	for (int i = 0; i < CPU_FRAME_COUNT; i++) {
		frameResourceArray.push_back(std::make_unique<FrameResource>(md3dDevice.Get(), cmdListType));
	}
	mDirectCmdListAlloc = frameResourceArray[0].get()->CmdListAlloc;

	md3dDevice->CreateCommandList(
		0,
		cmdListType,
		mDirectCmdListAlloc.Get(),
		nullptr,
		IID_PPV_ARGS(mCommandList.GetAddressOf()));

	md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&mFence));

	mCommandList->Close();
}

DX12TaskQueueThread::~DX12TaskQueueThread() {
}

void DX12TaskQueueThread::waitOnFence() {
	fenceValue++;
	setFence(fenceValue);
	WaitOnFenceForever(mFence, fenceValue);
}

int DX12TaskQueueThread::getFenceValue() {
	return fenceValue;
}

void DX12TaskQueueThread::setFence(int destVal) {
	fenceValue = destVal;
	mCommandQueue->Signal(mFence.Get(), fenceValue);
}

Microsoft::WRL::ComPtr<ID3D12Fence> DX12TaskQueueThread::getFence() {
	return mFence;
}
