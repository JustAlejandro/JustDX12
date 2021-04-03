#include "DX12ConstantBuffer.h"
#include "ResourceDecay.h"

DX12ConstantBuffer::DX12ConstantBuffer(ConstantBufferData* data, ID3D12Device5* device) {
	this->data = data->clone();
	elementByteSize = CalcConstantBufferByteSize(data->byteSize());

	ComPtr<ID3D12Resource> uploadBuffTemp = nullptr;

	for (UINT i = 0; i < CPU_FRAME_COUNT; i++) {
		auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(elementByteSize);

		device->CreateCommittedResource(
			&uploadHeap,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uploadBuffTemp));

		uploadBuffTemp->Map(0, nullptr, reinterpret_cast<void**>(&mappedData[i]));
		memcpy(mappedData[i], data->getData(), data->byteSize());

		uploadBuffer[i] = DX12Resource(DESCRIPTOR_TYPE_CBV, uploadBuffTemp.Get(), D3D12_RESOURCE_STATE_GENERIC_READ);
	}
	dirtyFrames = CPU_FRAME_COUNT;
}

DX12ConstantBuffer::~DX12ConstantBuffer() {
	for (UINT i = 0; i < CPU_FRAME_COUNT; i++) {
		if (uploadBuffer[i].get() != nullptr) {
			uploadBuffer[i].get()->Unmap(0, nullptr);
			ResourceDecay::destroyAfterDelay(uploadBuffer[i].get());
		}
	}
}

ID3D12Resource* DX12ConstantBuffer::get(int index) {
	return uploadBuffer[index].get();
}

DX12Resource* DX12ConstantBuffer::getDX12Resource(int index)
{
	return &uploadBuffer[index];
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
