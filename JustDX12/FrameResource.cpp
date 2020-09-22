#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount) {
	device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf()));

	passCB = std::make_unique<UploadBuffer<PerPassConstants>>(device, passCount, true);
	objectCB = std::make_unique<UploadBuffer<PerObjectConstants>>(device, objectCount, true);
}

FrameResource::~FrameResource() {
}
