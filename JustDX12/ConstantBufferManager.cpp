#include "ConstantBufferManager.h"

ConstantBufferManager::ConstantBufferManager(ComPtr<ID3D12Device2> device) {
	this->device = device;
}

DX12ConstantBuffer* ConstantBufferManager::getConstantBuffer(std::string name) {
	try {
		return &buffers.at(name);
	}
	catch (const std::out_of_range&) {
		OutputDebugStringA(("Couldn't find constant buffer named: " + name + "\n").c_str());
		throw "No ConstantBuffer Found ERROR";
	}
}

DX12ConstantBuffer* ConstantBufferManager::makeConstantBuffer(ConstantBufferJob job) {
	if (buffers.find(job.name + std::to_string(job.usageIndex)) != buffers.find(job.name + std::to_string(job.usageIndex))) {
		OutputDebugStringA(("ConstBuffer named " + job.name + " already exists\n").c_str());
		return nullptr;
	}
	buffers.try_emplace(job.name + std::to_string(job.usageIndex), job.initialData, device.Get());
	return getConstantBuffer(job.name + std::to_string(job.usageIndex));
}
