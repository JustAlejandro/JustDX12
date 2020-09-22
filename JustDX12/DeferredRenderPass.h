#pragma once
#include <wrl.h>
#include <d3d12.h>

struct DeferredRenderPass {
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRTVHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDepthHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mAttachments[3];
};

