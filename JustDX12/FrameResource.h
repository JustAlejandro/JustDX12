#pragma once

#include <memory>
#include <Windows.h>
#include <d3dx12.h>
#include <DirectXMath.h>

#include "ConstantBufferTypes.h"

#include "DX12Helper.h"

// Simple wrapper over the Command Allocator, so the allocator associated
// with a command list isn't reused before the command list is completed
// TODO: see if this class can't be easily removed, since most of it's original purpose has been pulled out
class FrameResource {
public:
	FrameResource(ID3D12Device5* device, D3D12_COMMAND_LIST_TYPE cmdListType = D3D12_COMMAND_LIST_TYPE_DIRECT);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	UINT64 Fence = 0;
};

