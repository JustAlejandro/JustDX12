#pragma once
#include <windows.h>
#include <wrl.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <d3dcompiler.h>
#include <string>
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <dxcapi.h>
#include <vector>

struct DXDefine {
	std::wstring name;
	std::wstring value;
	static std::vector<DxcDefine> DXDefineToDxcDefine(const std::vector<DXDefine>& defines);
};

Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
	ID3D12Device* device,
	ID3D12GraphicsCommandList5* cmdList,
	const void* initData,
	UINT64 byteSize,
	Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);

Microsoft::WRL::ComPtr<IDxcBlob> compileShader(
	const std::wstring& filename,
	const std::vector<DxcDefine>& defines,
	const std::wstring& entryPoint,
	const std::wstring& target);

DirectX::XMFLOAT4X4 Identity();

UINT CalcConstantBufferByteSize(UINT byteSize);

UINT CalcBufferByteSize(UINT byteSize, UINT alignment);

void WaitOnFenceForever(Microsoft::WRL::ComPtr<ID3D12Fence> fence, int destVal);

void updateBoundingBoxMinMax(DirectX::XMFLOAT3& minPoint, DirectX::XMFLOAT3& maxPoint, const DirectX::XMFLOAT3& pos);

DirectX::BoundingBox boundingBoxFromMinMax(const DirectX::XMFLOAT3& min, const DirectX::XMFLOAT3& max);