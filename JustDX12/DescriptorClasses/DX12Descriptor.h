#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <vector>
using namespace Microsoft::WRL;

class DX12Resource;
class DX12ConstantBuffer;
class DX12Texture;

enum DESCRIPTOR_USAGE {
	DESCRIPTOR_USAGE_ALL = 0,
	DESCRIPTOR_USAGE_PER_PASS = 1,
	DESCRIPTOR_USAGE_PER_OBJECT = 2,
	DESCRIPTOR_USAGE_PER_MESHLET = 3,
	DESCRIPTOR_USAGE_SYSTEM_DEFINED = 4,
	DESCRIPTOR_USAGE_MAX = 5
};

struct DX12DescriptorHeap {
	DX12DescriptorHeap() = default;
	DX12DescriptorHeap(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT offset, UINT size) {
		this->heap = heap;
		this->type = type;
		this->offset = offset;
		this->size = size;
		this->startCPUHandle = heap->GetCPUDescriptorHandleForHeapStart();
		this->startGPUHandle = heap->GetGPUDescriptorHandleForHeapStart();
		this->availabilityBitmap = std::vector<bool>(size, true);
	}

	void freeHeapSpace(CD3DX12_CPU_DESCRIPTOR_HANDLE start, UINT size) {
		UINT startIdx = ((UINT64)start.ptr - startCPUHandle.ptr) / offset;
		for (UINT i = 0; i < size; i++) {
			availabilityBitmap[i + (size_t)startIdx] = false;
		}
	}

	ID3D12DescriptorHeap* getHeap() const {
		return heap.Get();
	}

	UINT getOffset() const {
		return offset;
	}

	std::pair<CD3DX12_CPU_DESCRIPTOR_HANDLE, CD3DX12_GPU_DESCRIPTOR_HANDLE> reserveHeapSpace(UINT numDescriptors) {
		// Simple bitscan, there are faster methods, but this is simple and works
		UINT index = 0;
		UINT freeBits = 0;
		while (index < size) {
			if (availabilityBitmap[index] == true) {
				freeBits++;
			}
			else {
				freeBits = 0;
			}
			if (freeBits == numDescriptors) {
				// Reserve the spots before returning
				for (UINT i = 0; i < numDescriptors; i++) {
					availabilityBitmap[index - i] = false;
				}
				return std::make_pair(
					CD3DX12_CPU_DESCRIPTOR_HANDLE(startCPUHandle, index - numDescriptors + 1, offset),
					CD3DX12_GPU_DESCRIPTOR_HANDLE(startGPUHandle, index - numDescriptors + 1, offset));
			}
			index++;
		}
		throw "Not enough space available for descriptors";
	}

private:
	UINT size;
	D3D12_DESCRIPTOR_HEAP_TYPE type;
	UINT offset;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
	CD3DX12_CPU_DESCRIPTOR_HANDLE startCPUHandle;
	CD3DX12_GPU_DESCRIPTOR_HANDLE startGPUHandle;

	std::vector<bool> availabilityBitmap;
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
	DX12DescriptorHeap* descriptorHeap = nullptr;
};
