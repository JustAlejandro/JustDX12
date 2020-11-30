#pragma once
#include "Settings.h"
#include "DX12Helper.h"
#include "ConstantBufferData.h"
#include <mutex>
#include "ResourceClasses\DX12Resource.h"

using namespace Microsoft::WRL;

struct ConstantBufferJob {
	std::string name;
	ConstantBufferData* initialData;
	int usageIndex = 0;
};

class DX12ConstantBuffer {
public:
	DX12ConstantBuffer(ConstantBufferData* data, ID3D12Device2* device);
	~DX12ConstantBuffer();

	ID3D12Resource* get(int index);
	UINT getBufferSize();
	void updateBuffer(int index);
	void prepareUpdateBuffer(ConstantBufferData* copySource);

private:
	std::unique_ptr<ConstantBufferData> data;
	std::array<ComPtr<ID3D12Resource>, CPU_FRAME_COUNT> uploadBuffer;
	std::array<BYTE*, CPU_FRAME_COUNT> mappedData;

	UINT elementByteSize = 0;

	std::mutex dataUpdate;
};

