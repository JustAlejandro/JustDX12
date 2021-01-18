#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device5* device, D3D12_COMMAND_LIST_TYPE cmdListType) {
	device->CreateCommandAllocator(
		cmdListType,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf()));
}

FrameResource::~FrameResource() {
}
