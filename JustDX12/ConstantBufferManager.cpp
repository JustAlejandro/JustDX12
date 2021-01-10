#include "ConstantBufferManager.h"

ConstantBufferManager::ConstantBufferManager(ComPtr<ID3D12Device2> device) {
	this->device = device;
}

DX12ConstantBuffer* ConstantBufferManager::getConstantBuffer(IndexedName indexedName) {
	auto buffer = buffers.find(indexedName);
	if (buffer == buffers.end()) {
		auto externalResource = externalBuffers.find(indexedName);
		if (externalResource == externalBuffers.end()) {
			OutputDebugStringA(("Couldn't find ConstantBuffer Named: " + indexedName.getName() + " with Index: " + std::to_string(indexedName.getIndex()) + "\n").c_str());
			throw "NO CONSTANTBUFFER FOUND ERROR";
		}
		return externalResource->second;
	}
	return &buffer->second;
}

DX12ConstantBuffer* ConstantBufferManager::importConstantBuffer(IndexedName indexName, DX12ConstantBuffer* externalResource) {
	externalBuffers[indexName] = externalResource;
	return externalResource;
}

DX12ConstantBuffer* ConstantBufferManager::makeConstantBuffer(ConstantBufferJob job) {
	if (buffers.find(IndexedName(job.name, job.usageIndex)) != buffers.end()) {
		OutputDebugStringA(("ConstBuffer named " + job.name + " already exists\n").c_str());
		return nullptr;
	}
	buffers.try_emplace(IndexedName(job.name, job.usageIndex), job.initialData, device.Get());
	return getConstantBuffer(IndexedName(job.name, job.usageIndex));
}
