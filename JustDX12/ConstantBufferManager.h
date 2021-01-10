#pragma once
#include <unordered_map>
#include "DX12ConstantBuffer.h"
#include "IndexedName.h"


class ConstantBufferManager {
public:
	ConstantBufferManager(ComPtr<ID3D12Device2> device);

	DX12ConstantBuffer* getConstantBuffer(IndexedName indexedName);

	DX12ConstantBuffer* importConstantBuffer(IndexedName indexName, DX12ConstantBuffer* externalResource);

	DX12ConstantBuffer* makeConstantBuffer(ConstantBufferJob job);
private:
	std::unordered_map<IndexedName, DX12ConstantBuffer> buffers;
	std::unordered_map<IndexedName, DX12ConstantBuffer*> externalBuffers;
	ComPtr<ID3D12Device2> device = nullptr;
};

