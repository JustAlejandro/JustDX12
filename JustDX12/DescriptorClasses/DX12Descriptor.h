#pragma once
#include <vector>

#include <wrl.h>
#include <d3d12.h>
#include <d3dx12.h>

using namespace Microsoft::WRL;

class DX12Resource;
class DX12ConstantBuffer;
class DX12Texture;

// Describes the context the Descriptor is used in
// Underutilized at the moment
// TODO: Revisit this concept.
enum DESCRIPTOR_USAGE {
	DESCRIPTOR_USAGE_ALL = 0,				// Bound once, never unbound
	DESCRIPTOR_USAGE_PER_PASS = 1,			// Bound for a single pass, then possibly repaced (unused until Passes implemented)
	DESCRIPTOR_USAGE_PER_OBJECT = 2,		// Bound only when a specific object is drawn (Renderer still uses, User-side deprecated)
	DESCRIPTOR_USAGE_PER_MESHLET = 3,		// Bound only for each meshlet individually
	DESCRIPTOR_USAGE_SYSTEM_DEFINED = 4,	// RESERVED, use for resources accessed through unique fields in PipelineStageDesc or other StageDescs (ex: TLAS for RT)
	DESCRIPTOR_USAGE_MAX = 5				// Limit Constant, not user compatible
};

// Wrapper over ID3D12DescriptorHeap that finds free descriptor ranges for continuous DX12Descriptor allocation
// Also capable of marking ranges as available for future reuse
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

	ID3D12DescriptorHeap* getHeap() const {
		return heap.Get();
	}

	UINT getOffset() const {
		return offset;
	}

	void freeHeapSpace(CD3DX12_CPU_DESCRIPTOR_HANDLE start, UINT size) {
		UINT startIdx = ((UINT64)start.ptr - startCPUHandle.ptr) / offset;
		for (UINT i = 0; i < size; i++) {
			availabilityBitmap[i + (size_t)startIdx] = true;
		}
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

// Contains all data needed to bind the descriptor to the root signature
// Or to free the descriptor when it's no longer needed.
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
