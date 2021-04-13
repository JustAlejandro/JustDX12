#include "Tasks\DX12TaskQueueThread.h"
#include "DX12Helper.h"
#include "Settings.h"

DX12TaskQueueThread::DX12TaskQueueThread(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice, D3D12_COMMAND_LIST_TYPE cmdListType) : md3dDevice(d3dDevice) {

	running = true;
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

	worker = std::thread(&DX12TaskQueueThread::threadMain, this);
}

DX12TaskQueueThread::~DX12TaskQueueThread() {
	running = false;
	taskCv.notify_one();
	worker.join();
	while (!taskQueue.empty()) {
		delete taskQueue.front();
		taskQueue.pop();
	}
}

void DX12TaskQueueThread::enqueue(Task* t) {
	std::lock_guard<std::mutex> lk(taskQueueMutex);
	taskQueue.push(t);
	taskCv.notify_one();
}

HANDLE DX12TaskQueueThread::deferSetCpuEvent() {
	HANDLE ev = CreateEvent(
		NULL,
		FALSE,
		FALSE,
		NULL);
	enqueue(new SetCpuEventTask(ev));
	return ev;
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

void DX12TaskQueueThread::threadMain() {
	try {
		while (true) {
			std::unique_lock<std::mutex> lk(taskQueueMutex);
			if (taskQueue.empty()) {
				taskCv.wait(lk, [this]() { return !taskQueue.empty() || !running; });
			}
			if (!running) {
				return;
			}
			Task* toExecute = taskQueue.front();
			taskQueue.pop();
			lk.unlock();

			toExecute->execute();
			delete toExecute;
		}
	}
	catch (const HrException& hrEx) {
		MessageBoxA(nullptr, hrEx.what(), "HR Exception", MB_OK);
		throw hrEx;
	}
	catch (const std::string& ex) {
		MessageBoxA(nullptr, ex.c_str(), "String Exception", MB_OK);
		throw ex;
	}
}