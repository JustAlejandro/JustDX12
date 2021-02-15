#include "DX12ConstantBuffer.h"

DX12ConstantBuffer::DX12ConstantBuffer(ConstantBufferData* data, ID3D12Device5* device) {
	this->data = data->clone();
	elementByteSize = CalcConstantBufferByteSize(data->byteSize());

	for (UINT i = 0; i < CPU_FRAME_COUNT; i++) {
		auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(elementByteSize);
		device->CreateCommittedResource(
			&uploadHeap,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uploadBuffer[i]));

		uploadBuffer[i]->Map(0, nullptr, reinterpret_cast<void**>(&mappedData[i]));
		memcpy(mappedData[i], data->getData(), data->byteSize());
	}
	dirtyFrames = CPU_FRAME_COUNT;
}

DX12ConstantBuffer::~DX12ConstantBuffer() {
	for (UINT i = 0; i < CPU_FRAME_COUNT; i++) {
		if (uploadBuffer[i] != nullptr) {
			uploadBuffer[i]->Unmap(0, nullptr);
		}
	}
}

ID3D12Resource* DX12ConstantBuffer::get(int index) {
	return uploadBuffer[index].Get();
}

UINT DX12ConstantBuffer::getBufferSize() {
	return data->byteSize();
}

void DX12ConstantBuffer::updateBuffer(UINT index) {
	std::unique_lock<std::mutex> lk(dataUpdate);
	if (dirtyFrames > 0) {
		memcpy(mappedData[index], data->getData(), data->byteSize());
	}
	dirtyFrames--;
}

void DX12ConstantBuffer::prepareUpdateBuffer(ConstantBufferData* copySource) {
	std::unique_lock<std::mutex> lk(dataUpdate);
	data = copySource->clone();
	dirtyFrames = CPU_FRAME_COUNT;
}
