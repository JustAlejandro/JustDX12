#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <d3dx12.h>
#include "DescriptorClasses\DescriptorManager.h"
#include "ResourceClasses\ResourceManager.h"

class SSAO {
public:
	SSAO(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format, UINT descriptorSize);

	ID3D12Resource* output();

	void BuildDescriptors(ID3D12Resource* input);
	void BuildResources();

	void Execute(ID3D12GraphicsCommandList* cmdList,
		ID3D12RootSignature* rootSig,
		ID3D12PipelineState* pso,
		ID3D12Resource* input,
		CD3DX12_GPU_DESCRIPTOR_HANDLE inputDes);

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;

private:
	UINT mWidth = 0;
	UINT mHeight = 0;
	UINT descriptorSize = 0;
	DXGI_FORMAT mFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	CD3DX12_CPU_DESCRIPTOR_HANDLE outCpuSRV;
	CD3DX12_CPU_DESCRIPTOR_HANDLE outCpuUAV;
	CD3DX12_CPU_DESCRIPTOR_HANDLE inCpuSRV;

	CD3DX12_GPU_DESCRIPTOR_HANDLE outGpuSRV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE outGpuUAV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE inGpuSRV;

	Microsoft::WRL::ComPtr<ID3D12Resource> outputRes = nullptr;
	DX12Resource* out = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Device> device = nullptr;

	DescriptorManager descriptorManager;
	ResourceManager resourceManager;
};

