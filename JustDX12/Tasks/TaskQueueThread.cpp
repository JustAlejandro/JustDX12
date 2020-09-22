#include "Tasks\TaskQueueThread.h"

TaskQueueThread::TaskQueueThread(Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice, D3D12_COMMAND_LIST_TYPE cmdListType) : md3dDevice(d3dDevice) {
	running = true;
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = cmdListType;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue));

	md3dDevice->CreateCommandAllocator(
		cmdListType,
		IID_PPV_ARGS(mDirectCmdListAlloc.GetAddressOf()));

	md3dDevice->CreateCommandList(
		0,
		cmdListType,
		mDirectCmdListAlloc.Get(),
		nullptr,
		IID_PPV_ARGS(mCommandList.GetAddressOf()));

	md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&mFence));

	mCommandList->Close();

	worker = std::thread(&TaskQueueThread::threadMain, this);
}

TaskQueueThread::~TaskQueueThread() {
	running = false;
	taskCv.notify_one();
	worker.join();
}

void TaskQueueThread::enqueue(Task* t) {
	std::lock_guard<std::mutex> lk(taskQueueMutex);
	taskQueue.push(t);
}

void TaskQueueThread::waitOnFence(Microsoft::WRL::ComPtr<ID3D12Fence> fence, int destVal) {
	if (fence->GetCompletedValue() < destVal) {
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);

		fence->SetEventOnCompletion(destVal, eventHandle);

		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

void TaskQueueThread::waitOnFence() {
	fenceValue++;
	setFence(fenceValue);
	waitOnFence(mFence, fenceValue);
}

void TaskQueueThread::setFence(int destVal) {
	mCommandQueue->Signal(mFence.Get(), fenceValue);
}

Microsoft::WRL::ComPtr<ID3D12Fence> TaskQueueThread::getFence() {
	return mFence;
}

void TaskQueueThread::threadMain() {
	mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr);
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
