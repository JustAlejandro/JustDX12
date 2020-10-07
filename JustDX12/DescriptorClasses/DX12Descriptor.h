#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <d3dx12.h>
using namespace Microsoft::WRL;

class DX12Resource;
class DX12ConstantBuffer;
class DX12Texture;

enum DESCRIPTOR_USAGE {
	DESCRIPTOR_USAGE_ALL = 0,
	DESCRIPTOR_USAGE_PER_PASS = 1,
	DESCRIPTOR_USAGE_PER_OBJECT = 2,
	DESCRIPTOR_USAGE_PER_MESH = 3,
	DESCRIPTOR_USAGE_MAX = 4
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
	DESCRIPTOR_USAGE usage = DESCRIPTOR_USAGE_ALL;
	ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;
};

struct DX12DescriptorHeap {
	UINT size;
	D3D12_DESCRIPTOR_HEAP_TYPE type;
	UINT offset;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
	CD3DX12_CPU_DESCRIPTOR_HANDLE startCPUHandle;
	CD3DX12_GPU_DESCRIPTOR_HANDLE startGPUHandle;
	//Offset handle should point to the current 'end' of the heap
	// this is where we'd place new descriptors.
	CD3DX12_CPU_DESCRIPTOR_HANDLE endCPUHandle;
	CD3DX12_GPU_DESCRIPTOR_HANDLE endGPUHandle;

	void shiftHandles() {
		endCPUHandle.Offset(offset);
		endGPUHandle.Offset(offset);
	}
};