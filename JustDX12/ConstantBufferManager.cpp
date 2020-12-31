#include "ConstantBufferManager.h"

ConstantBufferManager::ConstantBufferManager(ComPtr<ID3D12Device2> device) {
	this->device = device;
}

DX12ConstantBuffer* ConstantBufferManager::getConstantBuffer(IndexedName indexedName) {
	try {
		return &buffers.at(indexedName);
	}
	catch (const std::out_of_range&) {
		OutputDebugStringA(("Couldn't find constant buffer named: " + indexedName.getName() + "\n" +
			"At Index: " + std::to_string(indexedName.getIndex()) + "\n").c_str());
		throw "No ConstantBuffer Found ERROR";
	}
}

DX12ConstantBuffer* ConstantBufferManager::makeConstantBuffer(ConstantBufferJob job) {
	if (buffers.find(IndexedName(job.name, job.usageIndex)) != buffers.end()) {
		OutputDebugStringA(("ConstBuffer named " + job.name + " already exists\n").c_str());
		return nullptr;
	}
	buffers.try_emplace(IndexedName(job.name, job.usageIndex), job.initialData, device.Get());
	return getConstantBuffer(IndexedName(job.name, job.usageIndex));
}
