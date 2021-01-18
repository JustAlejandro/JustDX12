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

// This code is from NVidia's DXR sample
Microsoft::WRL::ComPtr<ID3D12Resource> CreateBlankBuffer(ID3D12Device5* device, ID3D12GraphicsCommandList5* cmdList, UINT64 byteSize, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initState, const D3D12_HEAP_PROPERTIES& heapProps) {
	D3D12_RESOURCE_DESC bufDesc = {};
	bufDesc.Alignment = 0;
	bufDesc.DepthOrArraySize = 1;
	bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufDesc.Flags = flags;
	bufDesc.Format = DXGI_FORMAT_UNKNOWN;
	bufDesc.Height = 1;
	bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	bufDesc.MipLevels = 1;
	bufDesc.SampleDesc.Count = 1;
	bufDesc.SampleDesc.Quality = 0;
	bufDesc.Width = byteSize;

	Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
	ThrowIfFailed(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, initState, nullptr, IID_PPV_ARGS(&buffer)));
	return buffer;
}

Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(ID3D12Device5* device, ID3D12GraphicsCommandList5* cmdList, const void* initData, UINT64 byteSize, Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer) {

	Microsoft::WRL::ComPtr<ID3D12Resource> defaultBuffer;
	auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
	device->CreateCommittedResource(
		&gDefaultHeapDesc,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(defaultBuffer.GetAddressOf()));
	device->CreateCommittedResource(
		&gUploadHeapDesc,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
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

	Microsoft::WRL::ComPtr<IDxcIncludeHandler> pIncludeHandler;
	library->CreateIncludeHandler(&pIncludeHandler);

	Microsoft::WRL::ComPtr<IDxcOperationResult> result;

	hr = compiler->Compile(
		sourceBlob.Get(),
		filename.c_str(),
		entryPoint.c_str(),
		target.c_str(),
		arguements.data(), arguements.size(),
		defines.data(), defines.size(),
		pIncludeHandler.Get(),
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

float AverageVector(std::vector<float>& vec) {
	float sum = 0.0f;
	for (const float& num : vec) {
		sum += num;
	}
	return sum / vec.size();
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

void WaitOnMultipleFencesForever(std::vector<ID3D12Fence*> fences, std::vector<UINT64> destVals, ID3D12Device1* device) {
	bool alreadyFinished = true;
	for (int i = 0; i < fences.size(); i++) {
		if (fences[i]->GetCompletedValue() < destVals[i]) {
			alreadyFinished = false;
			break;
		}
	}
	if (!alreadyFinished) {
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);

		device->SetEventOnMultipleFenceCompletion(fences.data(), destVals.data(), fences.size(), D3D12_MULTIPLE_FENCE_WAIT_FLAG_ALL, eventHandle);

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
