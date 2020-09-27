#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <d3dx12.h>
using namespace Microsoft::WRL;

class DX12Resource;
class DX12ConstantBuffer;

struct DX12Descriptor {
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle;
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle;
	union {
		DX12Resource* resourceTarget;
		DX12ConstantBuffer* constantBufferTarget;
	};
	ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;
};
