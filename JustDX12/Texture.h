#pragma once
#define NOMINMAX
#include <string>
#include <wrl.h>

#include "ResourceClasses/DX12Resource.h"

// Soon to be deprecated, once Meshlet loading is done in a similar manner as regular Models, enum will be removed
enum TEX_STATUS {
	TEX_STATUS_NOT_LOADED = 0,
	TEX_STATUS_LOADED = 1,
	TEX_STATUS_DESCRIPTOR_BOUND = 2
};

class DX12Texture : public DX12Resource {
public:
	std::string Filename;

	std::string dir;

	D3D12_RESOURCE_DESC MetaData;

	TEX_STATUS status;

	// Making it easier to access for the TextureLoader
	// Since the TextureLoader is the only one that knows these exist it should be fine.
	using DX12Resource::resource;
	using DX12Resource::curState;
	using DX12Resource::format;
	using DX12Resource::type;
};
