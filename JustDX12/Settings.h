#pragma once
#include <d3d12.h>
#include "pix3.h"
#include <array>
#include <d3dx12.h>

#define USE_PIX
#define CLEAR_MODEL_MEMORY

#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080

#define COLOR_TEXTURE_FORMAT DXGI_FORMAT_R16G16B16A16_FLOAT
#define DEPTH_TEXTURE_FORMAT DXGI_FORMAT_R24G8_TYPELESS
#define DEPTH_TEXTURE_DSV_FORMAT DXGI_FORMAT_D24_UNORM_S8_UINT
#define DEPTH_TEXTURE_SRV_FORMAT DXGI_FORMAT_R24_UNORM_X8_TYPELESS

extern UINT gRtvDescriptorSize;
extern UINT gDsvDescriptorSize;
extern UINT gCbvSrvUavDescriptorSize;

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
	gDsvDefaultDesc.Flags = D3D12_DSV_FLAG_NONE;
	gDsvDefaultDesc.Format = DEPTH_TEXTURE_DSV_FORMAT;
	gDsvDefaultDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
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
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}
