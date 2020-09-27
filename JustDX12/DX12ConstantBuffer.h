#pragma once
#include "DX12Helper.h"
#include "ConstantBufferData.h"
#include <mutex>
#include "ResourceClasses\DX12Resource.h"

using namespace Microsoft::WRL;

struct ConstantBufferJob {
	std::string name;
	ConstantBufferData* initialData;
};

class DX12ConstantBuffer {
public:
	DX12ConstantBuffer(ConstantBufferData* data, ID3D12Device* device);
	~DX12ConstantBuffer();

	ID3D12Resource* get();
	UINT getBufferSize();
	void updateBuffer();
	void prepareUpdateBuffer(ConstantBufferData* copySource);

private:
	std::unique_ptr<ConstantBufferData> data;
	ComPtr<ID3D12Resource> uploadBuffer;
	BYTE* mappedData = nullptr;

	UINT elementByteSize = 0;

	std::mutex dataUpdate;
};

