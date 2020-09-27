#pragma once

#include <Windows.h>
#include <d3dx12.h>
#include <DirectXMath.h>
#include <memory>
#include "DX12Helper.h"
#include "UploadBuffer.h"
#include "ConstantBufferTypes.h"

class FrameResource {
public:
	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	std::unique_ptr<UploadBuffer<PerPassConstants>> passCB = nullptr;
	std::unique_ptr<UploadBuffer<PerObjectConstants>> objectCB = nullptr;

	UINT64 Fence = 0;
};

