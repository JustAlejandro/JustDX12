#pragma once
#include "ResourceClasses\DX12Resource.h"
#include <unordered_map>
#include "Settings.h"
#include "DX12ConstantBuffer.h"

struct ResourceJob {
	std::string name;
	DESCRIPTOR_TYPES types;
	DXGI_FORMAT format = COLOR_TEXTURE_FORMAT;
	UINT texHeight = SCREEN_HEIGHT;
	UINT texWidth = SCREEN_WIDTH;
};

class ResourceManager {
public:
	ResourceManager(ComPtr<ID3D12Device2> device);
	DX12Resource* getResource(std::string name);
	DX12Resource* importResource(std::string name, DX12Resource* externalResource);
	DX12Resource* makeFromExisting(std::string name, DESCRIPTOR_TYPES types, ID3D12Resource* res, D3D12_RESOURCE_STATES state);
	DX12Resource* makeResource(ResourceJob job);
	DX12Resource* makeResource(std::string name, DESCRIPTOR_TYPES types = DESCRIPTOR_TYPE_SRV,
		DXGI_FORMAT format = COLOR_TEXTURE_FORMAT, UINT texHeight = SCREEN_HEIGHT, UINT texWidth = SCREEN_WIDTH);
private:
	std::unordered_map<std::string, DX12Resource> resources;
	std::unordered_map<std::string, DX12Resource*> externalResources;
	ComPtr<ID3D12Device2> device = nullptr;
};
