#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <d3dx12.h>
using namespace Microsoft::WRL;

class DX12Resource;
class DX12ConstantBuffer;
class DX12Texture;

enum DESCRIPTOR_USAGE {
	DESCRIPTOR_USAGE_PER_PASS = 0,
	DESCRIPTOR_USAGE_PER_OBJECT = 1,
	DESCRIPTOR_USAGE_PER_MESH = 2
};

struct DX12Descriptor {
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle;
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle;
	union {
		DX12Resource* resourceTarget;
		DX12ConstantBuffer* constantBufferTarget;
		DX12Texture* textureTarget;
	};
	int usageIndex = 0;
	DESCRIPTOR_USAGE usage = DESCRIPTOR_USAGE_PER_PASS;
	ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;
};
