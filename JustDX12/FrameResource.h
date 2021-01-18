#pragma once

#include <Windows.h>
#include <d3dx12.h>
#include <DirectXMath.h>
#include <memory>
#include "DX12Helper.h"
#include "ConstantBufferTypes.h"

class FrameResource {
public:
	FrameResource(ID3D12Device5* device, D3D12_COMMAND_LIST_TYPE cmdListType = D3D12_COMMAND_LIST_TYPE_DIRECT);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	UINT64 Fence = 0;
};

