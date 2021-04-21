#pragma once
#include "DX12ConstantBuffer.h"
#include "ConstantBufferTypes.h"

class TransformData {
public:
	TransformData(ID3D12Device5* device) {
		PerObjectConstants initData;
		constantBuffer = std::make_unique<DX12ConstantBuffer>(&initData, device);
	}
	void bindTransformToRoot(int slot, UINT frameIndex, ID3D12GraphicsCommandList* cmdList) {
		if (slot >= 0) {
			cmdList->SetGraphicsRootConstantBufferView(slot, constantBuffer->get(frameIndex)->GetGPUVirtualAddress());
		}
	}
	DX12Resource* getResourceForFrame(UINT frameIndex) const {
		return constantBuffer->getDX12Resource(frameIndex);
	}
	D3D12_GPU_VIRTUAL_ADDRESS getFrameTransformVirtualAddress(UINT instance, UINT frameIndex) const {
		return constantBuffer->get(frameIndex)->GetGPUVirtualAddress() + offsetof(PerObjectConstants::PerObjectConstantsStruct, World[instance]);
	}
	UINT getInstanceCount() const {
		return transform.data.instanceCount;
	}
	virtual void setInstanceCount(UINT count) {
		transform.data.instanceCount = count;
		dirtyFrames = CPU_FRAME_COUNT;
	}
	DirectX::XMFLOAT4X4 getTransform(UINT instance) const {
		return transform.data.World[instance];
	}
	void setTransform(UINT index, DirectX::XMFLOAT4X4 newTransform) {
		transform.data.World[index] = newTransform;
		dirtyFrames = CPU_FRAME_COUNT;
	}
	void submitUpdates(UINT index) {
		if (dirtyFrames > 0) {
			constantBuffer->prepareUpdateBuffer(&transform);
			constantBuffer->updateBuffer(index);
		}
		dirtyFrames--;
	}
	void submitUpdatesAll() {
		for (UINT i = 0; i < CPU_FRAME_COUNT; i++) {
			submitUpdates(i);
		}
	}
private:
	UINT dirtyFrames = 0;
	PerObjectConstants transform;
	std::unique_ptr<DX12ConstantBuffer> constantBuffer;
};

