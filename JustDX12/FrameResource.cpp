#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device2* device) {
	device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf()));
}

FrameResource::~FrameResource() {
}
