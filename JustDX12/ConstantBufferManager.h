#pragma once
#include <unordered_map>
#include "DX12ConstantBuffer.h"

class ConstantBufferManager {
public:
	ConstantBufferManager(ComPtr<ID3D12Device2> device);

	DX12ConstantBuffer* getConstantBuffer(std::string name);
	DX12ConstantBuffer* makeConstantBuffer(ConstantBufferJob job);
private:
	std::unordered_map<std::string, DX12ConstantBuffer> buffers;
	ComPtr<ID3D12Device2> device = nullptr;
};

