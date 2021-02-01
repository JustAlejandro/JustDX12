#pragma once
#include <d3d12.h>
#include "pix3.h"
#include <array>
#include <d3dx12.h>
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <math.h>

#define USE_PIX
#define CLEAR_MODEL_MEMORY

#define CPU_FRAME_COUNT 3
#define AUXILLARY_FENCE_COUNT 5

#define GPU_DEBUG true

#define SCREEN_WIDTH 3200
#define SCREEN_HEIGHT 1800

#define MAX_LIGHTS 10
#define MAX_INSTANCES 16

#define COLOR_TEXTURE_FORMAT DXGI_FORMAT_R16G16B16A16_FLOAT
#define DEPTH_TEXTURE_FORMAT DXGI_FORMAT_R24G8_TYPELESS
#define DEPTH_TEXTURE_DSV_FORMAT DXGI_FORMAT_D24_UNORM_S8_UINT
#define DEPTH_TEXTURE_SRV_FORMAT DXGI_FORMAT_R24_UNORM_X8_TYPELESS

extern UINT gRtvDescriptorSize;
extern UINT gDsvDescriptorSize;
extern UINT gCbvSrvUavDescriptorSize;
extern UINT gSamplerDescriptorSize;

extern D3D12_HEAP_PROPERTIES gDefaultHeapDesc;
extern D3D12_HEAP_PROPERTIES gUploadHeapDesc;

const UINT maxDescriptorHeapSize[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {
	100000,
	100,
	100,
	100 };

#define SHADING_RATE_COUNT 3
const FLOAT shadingRateDistance[] = {
	500.0f,
	1000.0f,
	2000.0f
};

const D3D12_SHADING_RATE shadingRates[] = {
	D3D12_SHADING_RATE_1X1,
	D3D12_SHADING_RATE_2X1,
	D3D12_SHADING_RATE_2X2,
	D3D12_SHADING_RATE_4X4
};

enum MODEL_FORMAT {
	MODEL_FORMAT_NONE = 0,
	MODEL_FORMAT_POSITON = 1 << 0,
	MODEL_FORMAT_NORMAL = 1 << 1,
	MODEL_FORMAT_TEXCOORD = 1 << 2,
	MODEL_FORMAT_DIFFUSE_TEX = 1 << 3,
	MODEL_FORMAT_NORMAL_TEX = 1 << 4,
	MODEL_FORMAT_SPECULAR_TEX = 1 << 5,
	MODEL_FORMAT_OPACITY_TEX = 1 << 6,
};

inline MODEL_FORMAT simpleMtlTypeToModelFormat(std::string type) {
	if (type == "diffuse") {
		return MODEL_FORMAT_DIFFUSE_TEX;
	}
	if (type == "normal") {
		return MODEL_FORMAT_NORMAL_TEX;
	}
	if (type == "specular") {
		return MODEL_FORMAT_SPECULAR_TEX;
	}
	if (type == "opacity") {
		return MODEL_FORMAT_OPACITY_TEX;
	}
	return MODEL_FORMAT_NONE;
}

inline D3D12_VIEWPORT DEFAULT_VIEW_PORT() {
	D3D12_VIEWPORT defaultViewPort;
	defaultViewPort.TopLeftX = 0;
	defaultViewPort.TopLeftY = 0;
	defaultViewPort.Width = static_cast<float>(SCREEN_WIDTH);
	defaultViewPort.Height = static_cast<float>(SCREEN_HEIGHT);
	defaultViewPort.MinDepth = 0.0f;
	defaultViewPort.MaxDepth = 1.0f;
	return defaultViewPort;
}

inline D3D12_RENDER_TARGET_VIEW_DESC DEFAULT_RTV_DESC() {
	D3D12_RENDER_TARGET_VIEW_DESC gRtvDefaultDesc;
	gRtvDefaultDesc.Format = COLOR_TEXTURE_FORMAT;
	gRtvDefaultDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	gRtvDefaultDesc.Texture2D.MipSlice = 0;
	gRtvDefaultDesc.Texture2D.PlaneSlice = 0;
	return gRtvDefaultDesc;
}

inline D3D12_DEPTH_STENCIL_VIEW_DESC DEFAULT_DSV_DESC() {
	D3D12_DEPTH_STENCIL_VIEW_DESC gDsvDefaultDesc;
	gDsvDefaultDesc.Format = DEPTH_TEXTURE_DSV_FORMAT;
	gDsvDefaultDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	gDsvDefaultDesc.Flags = D3D12_DSV_FLAG_NONE;
	gDsvDefaultDesc.Texture2D.MipSlice = 0;
	return gDsvDefaultDesc;
}

inline D3D12_SHADER_RESOURCE_VIEW_DESC DEFAULT_SRV_DESC() {
	D3D12_SHADER_RESOURCE_VIEW_DESC gSrvDefaultDesc;
	gSrvDefaultDesc.Format = COLOR_TEXTURE_FORMAT;
	gSrvDefaultDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	gSrvDefaultDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	gSrvDefaultDesc.Texture2D.MostDetailedMip = 0;
	gSrvDefaultDesc.Texture2D.MipLevels = 1;
	gSrvDefaultDesc.Texture2D.PlaneSlice = 0;
	return gSrvDefaultDesc;
}

inline D3D12_UNORDERED_ACCESS_VIEW_DESC DEFAULT_UAV_DESC() {
	D3D12_UNORDERED_ACCESS_VIEW_DESC gUavDefaultDesc;
	gUavDefaultDesc.Format = COLOR_TEXTURE_FORMAT;
	gUavDefaultDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	gUavDefaultDesc.Texture2D.MipSlice = 0;
	gUavDefaultDesc.Texture2D.PlaneSlice = 0;
	return gUavDefaultDesc;
}

inline D3D12_CLEAR_VALUE DEFAULT_CLEAR_VALUE() {
	D3D12_CLEAR_VALUE onClear;
	onClear.Format = COLOR_TEXTURE_FORMAT;
	onClear.Color[0] = 0.0f;
	onClear.Color[1] = 0.0f;
	onClear.Color[2] = 0.0f;
	onClear.Color[3] = 0.0f;
	return onClear;
}

inline D3D12_CLEAR_VALUE DEFAULT_CLEAR_VALUE_DEPTH_STENCIL() {
	D3D12_CLEAR_VALUE optClear;
	optClear.Format = DEPTH_TEXTURE_DSV_FORMAT;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;
	return optClear;
}

inline D3D12_SHADING_RATE getShadingRateFromDistance(const DirectX::XMFLOAT3& pos, const DirectX::BoundingBox& bb) {
	if (bb.Contains(DirectX::XMLoadFloat3(&pos))) {
		return shadingRates[0];
	}
	FLOAT distance = 0.0;
	DirectX::XMVECTOR displaceVec = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&pos), DirectX::XMLoadFloat3(&bb.Center));
	DirectX::XMStoreFloat(&distance, DirectX::XMVector3Dot(displaceVec, displaceVec));
	distance = sqrt(distance);
	for (int i = 0; i < SHADING_RATE_COUNT; i++) {
		if (distance < shadingRateDistance[i]) {
			return shadingRates[i];
		}
	}
	return shadingRates[SHADING_RATE_COUNT];
}

inline std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers() {
	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_MIRROR,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_MIRROR,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_MIRROR,  // addressW
		0.0f,							  // mipLODBias
		8);								  // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,							   // mipLODBias
		8);								   // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}
