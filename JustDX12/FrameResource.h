#pragma once

#include <Windows.h>
#include <d3dx12.h>
#include <DirectXMath.h>
#include <memory>
#include "DX12Helper.h"
#include "UploadBuffer.h"

struct PerObjectConstants {
	DirectX::XMFLOAT4X4 World = Identity();
};

struct PerPassConstants {
	DirectX::XMFLOAT4X4 view = Identity();
	DirectX::XMFLOAT4X4 invView = Identity();
	DirectX::XMFLOAT4X4 proj = Identity();
	DirectX::XMFLOAT4X4 invProj = Identity();
	DirectX::XMFLOAT4X4 viewProj = Identity();
	DirectX::XMFLOAT4X4 invViewProj = Identity();
	DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	float cbPadding = 0.0f;
	DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
	DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float TotalTime = 0.0f;
	float DeltaTime = 0.0f;
};

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

