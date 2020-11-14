#include "DX12Helper.h"
#include "Settings.h"
#include <cmath>
#include <d3d12shader.h>
#include <dxcapi.h>
#include <vector>

UINT32 GetFormatSize(DXGI_FORMAT format) {
	switch (format) {
	case DXGI_FORMAT_R32G32B32A32_FLOAT: return 16;
	case DXGI_FORMAT_R32G32B32_FLOAT: return 12;
	case DXGI_FORMAT_R32G32_FLOAT: return 8;
	case DXGI_FORMAT_R32_FLOAT: return 4;
	default: throw std::exception("Unimplemented type");
	}
}

Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(ID3D12Device2* device, ID3D12GraphicsCommandList5* cmdList, const void* initData, UINT64 byteSize, Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer) {

	Microsoft::WRL::ComPtr<ID3D12Resource> defaultBuffer;

	device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(byteSize),
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(defaultBuffer.GetAddressOf()));

	device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(byteSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(uploadBuffer.GetAddressOf()));

	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = initData;
	subResourceData.RowPitch = byteSize;
	subResourceData.SlicePitch = subResourceData.RowPitch;

	UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);
	
	return defaultBuffer;
}

Microsoft::WRL::ComPtr<IDxcBlob> compileShader(const std::wstring& filename, const std::vector<DxcDefine>& defines, const std::wstring& entryPoint, const std::wstring& target) {
	std::vector<LPCWSTR> arguements;
#if defined(DEBUG) || defined(_DEBUG)
	arguements.push_back(L"-Zi");
	arguements.push_back(L"-Od");
	arguements.push_back(L"-Qembed_debug");
#endif
	Microsoft::WRL::ComPtr<IDxcLibrary> library;
	HRESULT hr = DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));
	if (FAILED(hr)) {
		PostQuitMessage(0);
	}

	Microsoft::WRL::ComPtr<IDxcCompiler> compiler;
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
	if (FAILED(hr)) {
		PostQuitMessage(0);
	}

	UINT32 codePage = CP_UTF8;
	Microsoft::WRL::ComPtr<IDxcBlobEncoding> sourceBlob;
	hr = library->CreateBlobFromFile(filename.c_str(), &codePage, &sourceBlob);
	if (FAILED(hr)) {
		PostQuitMessage(0);
	}

	Microsoft::WRL::ComPtr<IDxcOperationResult> result;

	hr = compiler->Compile(
		sourceBlob.Get(),
		filename.c_str(),
		entryPoint.c_str(),
		target.c_str(),
		arguements.data(), arguements.size(),
		defines.data(), defines.size(),
		NULL,
		&result);
	
	if (SUCCEEDED(hr)) {
		result->GetStatus(&hr);
	}
	if (FAILED(hr)) {
		if (result) {
			Microsoft::WRL::ComPtr<IDxcBlobEncoding> errorsBlob;
			hr = result->GetErrorBuffer(&errorsBlob);
			if (SUCCEEDED(hr) && errorsBlob) {
				OutputDebugStringA((char*)errorsBlob->GetBufferPointer());
			}
		}
		MessageBox(nullptr, L"SHADER COMPILE FAILED", L"OOPS", MB_OK);
		PostQuitMessage(0);
	}

	Microsoft::WRL::ComPtr<IDxcBlob> code;
	result->GetResult(&code);

	return code;
}

DirectX::XMFLOAT4X4 Identity() {
	static DirectX::XMFLOAT4X4 I(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);

	return I;
}

UINT CalcConstantBufferByteSize(UINT byteSize) {
	// Constant buffers have to be multiples of 256 byte size
	int constBufferSize = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
	return (byteSize + constBufferSize - 1) & ~(constBufferSize - 1);
}

UINT CalcBufferByteSize(UINT byteSize, UINT alignment) {
	if ((alignment == 0) || (alignment & alignment - 1)) {
		throw "Non power of 2 alignment";
	}
	return (byteSize + (alignment-1)) & ~(alignment-1);
}

void WaitOnFenceForever(Microsoft::WRL::ComPtr<ID3D12Fence> fence, int destVal) {
	if (fence->GetCompletedValue() < destVal) {
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);

		fence->SetEventOnCompletion(destVal, eventHandle);

		PIXNotifyWakeFromFenceSignal(eventHandle);

		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

void updateBoundingBoxMinMax(DirectX::XMFLOAT3& minPoint, DirectX::XMFLOAT3& maxPoint, const DirectX::XMFLOAT3& pos) {
	maxPoint.x = std::fmax(maxPoint.x, pos.x);
	maxPoint.y = std::fmax(maxPoint.y, pos.y);
	maxPoint.z = std::fmax(maxPoint.z, pos.z);
	minPoint.x = std::fmin(minPoint.x, pos.x);
	minPoint.y = std::fmin(minPoint.y, pos.y);
	minPoint.z = std::fmin(minPoint.z, pos.z);
}

DirectX::BoundingBox boundingBoxFromMinMax(const DirectX::XMFLOAT3& min, const DirectX::XMFLOAT3& max) {
	DirectX::BoundingBox box;
	DirectX::BoundingBox::CreateFromPoints(box, DirectX::XMLoadFloat3(&min), DirectX::XMLoadFloat3(&max));
	return box;
}

std::vector<DxcDefine> DXDefine::DXDefineToDxcDefine(const std::vector<DXDefine>& defines) {
	std::vector<DxcDefine> dxcDefines;
	for (const auto& def : defines) {
		dxcDefines.push_back({ def.name.c_str(), def.value.c_str() });
	}
	return dxcDefines;
}
