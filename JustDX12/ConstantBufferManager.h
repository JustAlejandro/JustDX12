#pragma once
#include <unordered_map>
#include "DX12ConstantBuffer.h"
#include "IndexedName.h"

// Class that manages access to ConstantBuffers, which are ring-buffered resources
// TODO: find if there's a way for CBManager ResourceManager, and DescriptorManager 
// to inherit from a base Manager class
class ConstantBufferManager {
public:
	ConstantBufferManager(ComPtr<ID3D12Device5> device);

	DX12ConstantBuffer* getConstantBuffer(IndexedName indexedName);

	DX12ConstantBuffer* importConstantBuffer(IndexedName indexName, DX12ConstantBuffer* externalResource);

	DX12ConstantBuffer* makeConstantBuffer(ConstantBufferJob job);
private:
	std::unordered_map<IndexedName, DX12ConstantBuffer> buffers;
	std::unordered_map<IndexedName, DX12ConstantBuffer*> externalBuffers;
	ComPtr<ID3D12Device5> device = nullptr;
};

