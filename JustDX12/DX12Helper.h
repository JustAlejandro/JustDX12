#pragma once
#include <windows.h>
#include <wrl.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <d3dcompiler.h>
#include <string>
#include <DirectXMath.h>

Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	const void* initData,
	UINT64 byteSize,
	Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);

Microsoft::WRL::ComPtr<ID3DBlob> compileShader(
	const std::wstring& filename,
	const D3D_SHADER_MACRO* defines,
	const std::string& entryPoint,
	const std::string& target);

DirectX::XMFLOAT4X4 Identity();

UINT CalcConstantBufferByteSize(UINT byteSize);