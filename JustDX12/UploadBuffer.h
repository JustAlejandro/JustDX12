#pragma once

#include <d3dx12.h>
#include <wrl.h>

template<typename T>
class UploadBuffer {
public:
	UploadBuffer(ID3D12Device* device, UINT elementCount, bool isConstantBuffer) {
		this->isConstantBuffer = isConstantBuffer;
		elementByteSize = sizeof(T);

		if (isConstantBuffer) {
			elementByteSize = CalcConstantBufferByteSize(sizeof(T));
		}

		device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(elementByteSize * elementCount),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uploadBuffer));

		uploadBuffer->Map(0, nullptr,
			reinterpret_cast<void**>(&mappedData));
	}

	UploadBuffer(const UploadBuffer& rhs) = delete;
	UploadBuffer& operator=(const UploadBuffer& rhs) = delete;

	~UploadBuffer() {
		if (uploadBuffer != nullptr) {
			uploadBuffer->Unmap(0, nullptr);
		}
		mappedData = nullptr;
	}

	ID3D12Resource* Resource() const {
		return uploadBuffer.Get();
	}

	void copyData(int elementIndex, const T& data) {
		memcpy(&mappedData[elementIndex * elementByteSize], &data, sizeof(T));
	}

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
	BYTE* mappedData = nullptr;

	UINT elementByteSize = 0;
	bool isConstantBuffer = false;
};