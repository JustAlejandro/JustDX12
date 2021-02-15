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
	ConstantBufferJob() = default;
	ConstantBufferJob(std::string name, ConstantBufferData* initialData, int usageIndex = 0) {
		this->name = name;
		this->initialData = initialData;
		this->usageIndex = usageIndex;
	}
};

class DX12ConstantBuffer {
public:
	DX12ConstantBuffer(ConstantBufferData* data, ID3D12Device5* device);
	~DX12ConstantBuffer();

	ID3D12Resource* get(int index);
	UINT getBufferSize();
	void updateBuffer(UINT index);
	void prepareUpdateBuffer(ConstantBufferData* copySource);

private:
	std::unique_ptr<ConstantBufferData> data;
	std::array<ComPtr<ID3D12Resource>, CPU_FRAME_COUNT> uploadBuffer;
	std::array<BYTE*, CPU_FRAME_COUNT> mappedData;

	UINT dirtyFrames = 0;
	UINT elementByteSize = 0;

	std::mutex dataUpdate;
};

