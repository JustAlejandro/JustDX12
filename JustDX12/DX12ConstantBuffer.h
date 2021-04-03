#pragma once
#include <mutex>

#include "ResourceClasses\DX12Resource.h"
#include "ConstantBufferData.h"

#include "DX12Helper.h"
#include "Settings.h"

using namespace Microsoft::WRL;

// Contains data required to create a ConstantBuffer
// data handed over through initialData will be deleted on creation,
// so just pass this as a new and forget about it
struct ConstantBufferJob {
	std::string name;
	// Raw C++ pointer obtained through 'new', 'delete' will be called after creation is completed
	ConstantBufferData* initialData;
	int usageIndex = 0;
	ConstantBufferJob() = default;
	ConstantBufferJob(std::string name, ConstantBufferData* initialData, int usageIndex = 0) {
		this->name = name;
		this->initialData = initialData;
		this->usageIndex = usageIndex;
	}
};

// Generic representation of arbitrary data in DX12 ConstantBuffers
// Uses a cyclical buffer so we never overwrite data that's in use
// TODO: pull ConstantBuffers into the Default heap, rather than Upload
class DX12ConstantBuffer {
public:
	DX12ConstantBuffer(ConstantBufferData* data, ID3D12Device5* device);
	~DX12ConstantBuffer();

	ID3D12Resource* get(int index);
	DX12Resource* getDX12Resource(int index);
	UINT getBufferSize();

	void updateBuffer(UINT index);
	void prepareUpdateBuffer(ConstantBufferData* copySource);

private:
	std::unique_ptr<ConstantBufferData> data;
	std::array<DX12Resource, CPU_FRAME_COUNT> uploadBuffer;
	std::array<BYTE*, CPU_FRAME_COUNT> mappedData;

	UINT dirtyFrames = 0;
	UINT elementByteSize = 0;

	std::mutex dataUpdate;
};

