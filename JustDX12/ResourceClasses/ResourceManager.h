#pragma once
#include <unordered_map>

#include "ResourceClasses\DX12Resource.h"

#include "DX12ConstantBuffer.h"

#include "Settings.h"

// Describes a Texture to be created by the ResourceManager (typically used for render targets
struct ResourceJob {
	std::string name;
	DESCRIPTOR_TYPES types;
	DXGI_FORMAT format = HELPER_TEXTURE_FORMAT;
	UINT texHeight = gScreenHeight;
	UINT texWidth = gScreenWidth;
	ResourceJob() = default;
	ResourceJob(std::string name, DESCRIPTOR_TYPES types, DXGI_FORMAT format = HELPER_TEXTURE_FORMAT, UINT texHeight = gScreenHeight, UINT texWidth = gScreenWidth) {
		this->name = name;
		this->types = types;
		this->format = format;
		this->texHeight = texHeight;
		this->texWidth = texWidth;
	}
};

// Simple storage class that manages memory and keeps used resources in scope/easily accessible
// Can import DX12Resources from other ResourceManagers, but doesn't keep track of their lifespan
class ResourceManager {
public:
	ResourceManager(ComPtr<ID3D12Device5> device);

	DX12Resource* getResource(std::string name);

	DX12Resource* importResource(std::string name, DX12Resource* externalResource);

	DX12Resource* makeFromExisting(std::string name, DESCRIPTOR_TYPES types, ID3D12Resource* res, D3D12_RESOURCE_STATES state);
	DX12Resource* makeResource(ResourceJob job);
	DX12Resource* makeResource(std::string name, DESCRIPTOR_TYPES types = DESCRIPTOR_TYPE_SRV, DXGI_FORMAT format = HELPER_TEXTURE_FORMAT, UINT texHeight = gScreenHeight, UINT texWidth = gScreenWidth);
private:
	std::unordered_map<std::string, DX12Resource> resources;
	std::unordered_map<std::string, DX12Resource*> externalResources;
	ComPtr<ID3D12Device5> device = nullptr;
};
