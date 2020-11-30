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
#include <stdexcept>

struct DXDefine {
	std::wstring name;
	std::wstring value;
	static std::vector<DxcDefine> DXDefineToDxcDefine(const std::vector<DXDefine>& defines);
};

template <typename T, typename U>
constexpr T DivRoundUp(T num, U denom) {
	return (num + denom - 1) / denom;
}

UINT32 GetFormatSize(DXGI_FORMAT format);

Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
	ID3D12Device2* device,
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

float AverageVector(std::vector<float>& vec);

UINT CalcConstantBufferByteSize(UINT byteSize);

UINT CalcBufferByteSize(UINT byteSize, UINT alignment);

void WaitOnFenceForever(Microsoft::WRL::ComPtr<ID3D12Fence> fence, int destVal);

void WaitOnMultipleFencesForever(std::vector<ID3D12Fence*> fences, std::vector<UINT64> destVals, ID3D12Device1* device);

void updateBoundingBoxMinMax(DirectX::XMFLOAT3& minPoint, DirectX::XMFLOAT3& maxPoint, const DirectX::XMFLOAT3& pos);

DirectX::BoundingBox boundingBoxFromMinMax(const DirectX::XMFLOAT3& min, const DirectX::XMFLOAT3& max);

// Assign a name to the object to aid with debugging. (Taken from Microsoft DXSampleHelper)
#if defined(_DEBUG) || defined(DBG)
inline void SetName(ID3D12Object* pObject, LPCWSTR name) {
	pObject->SetName(name);
}
inline void SetNameIndexed(ID3D12Object* pObject, LPCWSTR name, UINT index) {
	WCHAR fullName[50];
	if (swprintf_s(fullName, L"%s[%u]", name, index) > 0) {
		pObject->SetName(fullName);
	}
}
#else
inline void SetName(ID3D12Object*, LPCWSTR) {
}
inline void SetNameIndexed(ID3D12Object*, LPCWSTR, UINT) {
}
#endif

#define NAME_D3D12_OBJECT(x) SetName((x).Get(), L#x)
#define NAME_D3D12_OBJECT_INDEXED(x, n) SetNameIndexed((x)[n].Get(), L#x, n)

// HR Exception parsing from Microsoft Samples
inline std::string HrToString(HRESULT hr) {
	char s_str[64] = {};
	sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
	return std::string(s_str);
}

class HrException : public std::runtime_error {
public:
	HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), m_hr(hr) {}
	HRESULT Error() const { return m_hr; }
private:
	const HRESULT m_hr;
};

#define SAFE_RELEASE(p) if (p) (p)->Release()

inline void ThrowIfFailed(HRESULT hr) {
	if (FAILED(hr)) {
		throw HrException(hr);
	}
}