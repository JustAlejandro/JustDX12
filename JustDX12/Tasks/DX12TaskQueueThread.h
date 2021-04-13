#pragma once
#include "Tasks\Task.h"
#include <queue>
#include <thread>
#include <condition_variable>
#define NOMINMAX
#include <wrl.h>
#include <d3d12.h>
#include "FrameResource.h"
#include "TaskQueueThread.h"

// Continuous thread that has DX12 backing to execute it's own commands.
class DX12TaskQueueThread : public TaskQueueThread {
protected:
	std::vector<std::unique_ptr<FrameResource>> frameResourceArray;

public:
	DX12TaskQueueThread(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice, D3D12_COMMAND_LIST_TYPE cmdListType = D3D12_COMMAND_LIST_TYPE_DIRECT);
	~DX12TaskQueueThread();

	Microsoft::WRL::ComPtr<ID3D12Device5> md3dDevice;

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> mCommandList;

	void waitOnFence();
	int getFenceValue();
	void setFence(int destVal);
	Microsoft::WRL::ComPtr<ID3D12Fence> getFence();

private:
	Microsoft::WRL::ComPtr<ID3D12Fence> mFence = nullptr;
	int fenceValue;

};

