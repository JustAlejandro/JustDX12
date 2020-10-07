#pragma once
#include "DescriptorClasses\DX12Descriptor.h"
#include "ResourceClasses\DX12Resource.h"
#include "ConstantBufferManager.h"
#include <unordered_map>

class ResourceManager;
class DX12Resource;

struct DescriptorJob {
	std::string name;
	std::string target;
	DESCRIPTOR_TYPE type;
	union {
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	};
	int usageIndex = 0;
	DESCRIPTOR_USAGE usage = DESCRIPTOR_USAGE_PER_PASS;
};

struct hash_pair {
	size_t operator()(const std::pair<std::string, DESCRIPTOR_TYPE>& p) const {
		auto hash1 = std::hash<std::string>{}(p.first);
		auto hash2 = std::hash<DESCRIPTOR_TYPE>{}(p.second);
		return hash1 ^ hash2;
	}
};

class DescriptorManager {
public:
	DescriptorManager(ComPtr<ID3D12Device> device);
	void makeDescriptors(std::vector<DescriptorJob> descriptorJobs, ResourceManager* resourceManager, ConstantBufferManager* constantBufferManager);
	DX12Descriptor* getDescriptor(const std::string& name, const DESCRIPTOR_TYPE& type);
	std::vector<ID3D12DescriptorHeap*> getAllBindableHeaps();
	std::vector<std::pair<D3D12_RESOURCE_STATES, DX12Resource*>> requiredResourceStates();
	std::vector<DX12Descriptor*>* getAllDescriptorsOfType(DESCRIPTOR_TYPE type);
	D3D12_DESCRIPTOR_HEAP_TYPE heapTypeFromDescriptorType(DESCRIPTOR_TYPE type);

private:
	void makeDescriptorHeaps();
	D3D12_DESCRIPTOR_HEAP_FLAGS shaderVisibleFromHeapType(D3D12_DESCRIPTOR_HEAP_TYPE type);
	void createDescriptorView(DX12Descriptor& descriptor, DescriptorJob& job);
	UINT getDescriptorOffsetForType(D3D12_DESCRIPTOR_HEAP_TYPE type);

	DX12DescriptorHeap heaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	std::unordered_map<DESCRIPTOR_TYPE, std::vector<DX12Descriptor*>> descriptorsByType;
	std::unordered_map<std::pair<std::string, DESCRIPTOR_TYPE>, DX12Descriptor, hash_pair> descriptors;
	ComPtr<ID3D12Device> device = nullptr;
};
