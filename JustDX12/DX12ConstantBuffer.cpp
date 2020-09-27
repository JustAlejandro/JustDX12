#include "DX12ConstantBuffer.h"

DX12ConstantBuffer::DX12ConstantBuffer(ConstantBufferData* data, ID3D12Device* device) {
	this->data = data->clone();
	elementByteSize = CalcConstantBufferByteSize(data->byteSize());

	device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(elementByteSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&uploadBuffer));

	uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
	memcpy(mappedData, data->getData(), elementByteSize);
}

DX12ConstantBuffer::~DX12ConstantBuffer() {
	if (uploadBuffer != nullptr) {
		uploadBuffer->Unmap(0, nullptr);
	}
}

ID3D12Resource* DX12ConstantBuffer::get() {
	return uploadBuffer.Get();
}

UINT DX12ConstantBuffer::getBufferSize() {
	return data->byteSize();
}

void DX12ConstantBuffer::updateBuffer() {
	std::unique_lock<std::mutex> lk(dataUpdate);
	memcpy(mappedData, data->getData(), elementByteSize);
}

void DX12ConstantBuffer::prepareUpdateBuffer(ConstantBufferData* copySource) {
	std::unique_lock<std::mutex> lk(dataUpdate);
	data = copySource->clone();
}
