#pragma once
#include "Tasks\Task.h"
#include <queue>
#include <thread>
#include <condition_variable>
#define NOMINMAX
#include <wrl.h>
#include <d3d12.h>

class TaskQueueThread {
protected:
	TaskQueueThread(Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice, D3D12_COMMAND_LIST_TYPE cmdListType = D3D12_COMMAND_LIST_TYPE_DIRECT);
	~TaskQueueThread();
	void enqueue(Task* t);

public:
	Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice;

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;

	void waitOnFence();
	int getFenceValue();
	void setFence(int destVal);
	Microsoft::WRL::ComPtr<ID3D12Fence> getFence();
private:
	bool running;
	std::mutex taskQueueMutex;
	std::queue<Task*> taskQueue;
	std::thread worker;
	std::condition_variable taskCv;

	Microsoft::WRL::ComPtr<ID3D12Fence> mFence = nullptr;
	int fenceValue;

	void threadMain();
};

