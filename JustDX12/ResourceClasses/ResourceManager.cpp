#include "ResourceClasses\ResourceManager.h"
#include <stdexcept>

ResourceManager::ResourceManager(ComPtr<ID3D12Device> device) {
	this->device = device;
}

DX12Resource* ResourceManager::getResource(std::string name) {
	auto resource = resources.find(name);
	if (resource == resources.end()) {
		auto externalResource = externalResources.find(name);
		if (externalResource == externalResources.end()) {
			OutputDebugStringA(("Couldn't find Resource Named: " + name + "\n").c_str());
			throw "NO RESOURCE FOUND ERROR";
		}
		return externalResource->second;
	}
	return &resource->second;
}

DX12Resource* ResourceManager::importResource(std::string name, DX12Resource* externalResource) {
	externalResources[name] = externalResource;
	return externalResource;
}

DX12Resource* ResourceManager::makeFromExisting(std::string name, DESCRIPTOR_TYPES types, ID3D12Resource* res, D3D12_RESOURCE_STATES state) {
	resources.try_emplace(name, types, res, state);
	return &resources.at(name);
}

DX12Resource* ResourceManager::makeResource(ResourceJob job) {
	if (resources.find(job.name) != resources.end()) {
		OutputDebugStringA(("Resource named " + job.name + " already exists\n").c_str());
		return nullptr;
	}
	resources.try_emplace(job.name, device, job.types, job.format, job.texHeight, job.texWidth);
	return getResource(job.name);
}

DX12Resource* ResourceManager::makeResource(std::string name, DESCRIPTOR_TYPES types, DXGI_FORMAT format, UINT texHeight, UINT texWidth) {
	if (resources.find(name) != resources.end()) {
		OutputDebugStringA(("Resource named " + name + " already exists\n").c_str());
		return nullptr;
	}
	resources.try_emplace(name, device, types, format, texHeight, texWidth);
	return getResource(name);
}
