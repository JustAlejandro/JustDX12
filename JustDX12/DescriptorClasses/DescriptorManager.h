#pragma once

#include <unordered_map>

#include "IndexedName.h"

#include "DescriptorClasses\DX12Descriptor.h"
#include "ResourceClasses\DX12Resource.h"

#include "ConstantBufferManager.h"


class ResourceManager;
class DX12Resource;


// Defines a Descriptor to be created.
// Setting 'directBinding' to false obtains the Resource tied to the descriptor by name
// Setting 'autoDesc' to true will try to interpret at runtime from Resource format the 'view'
// If autoDesc is false, 'view' must be filled out manually, not in constructor.
struct DescriptorJob {
	DescriptorJob() = default;
	DescriptorJob(std::string name, DX12Resource* directBindingTarget, DESCRIPTOR_TYPE type, bool autoDesc = true, int usageIndex = 0, DESCRIPTOR_USAGE usage = DESCRIPTOR_USAGE_ALL) {
		this->name = name;
		this->directBinding = true;
		this->directBindingTarget = directBindingTarget;
		this->autoDesc = autoDesc;
		this->type = type;
		this->usageIndex = usageIndex;
		this->usage = usage;
		this->view.srvDesc = {};
	}
	DescriptorJob(std::string name, std::string targetResourceName, DESCRIPTOR_TYPE type, bool autoDesc = true, int usageIndex = 0, DESCRIPTOR_USAGE usage = DESCRIPTOR_USAGE_ALL) {
		this->name = name;
		this->directBinding = false;
		this->indirectTarget = targetResourceName;
		this->autoDesc = autoDesc;
		this->type = type;
		this->usageIndex = usageIndex;
		this->usage = usage;
		this->view.srvDesc = {};
	}
	union ViewDesc {
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
		struct SIMPLIFIED_CBV_VIEW_DESC {
			UINT cbvSize;
			UINT offset;
		};
		SIMPLIFIED_CBV_VIEW_DESC cbvDesc;
	};

	std::string name;
	// Only true if the DX12Resource* is given
	bool directBinding = false;
	std::string indirectTarget;
	DX12Resource* directBindingTarget;
	DESCRIPTOR_TYPE type;
	bool autoDesc = false;
	// Constructor won't fill this in, so you've got to do it if 'autoDesc == false'
	ViewDesc view;
	int usageIndex = 0;
	DESCRIPTOR_USAGE usage = DESCRIPTOR_USAGE_PER_PASS;
};

struct hash_pair {
	size_t operator()(const std::pair<IndexedName, DESCRIPTOR_TYPE>& p) const {
		auto hash1 = std::hash<IndexedName>{}(p.first);
		auto hash2 = std::hash<DESCRIPTOR_TYPE>{}(p.second);
		return hash1 ^ hash2;
	}
};

// Helper class that wraps both access and creation of Descriptors
// Has support for descriptor deallocation through ResourceDecay::freeDescriptorsAferDelay
// Possible Improvements: don't always allocate descriptor heap sizes at fixed size
// allow DescriptorManager owner to describe limits/heap requirements
class DescriptorManager {
public:
	DescriptorManager(ComPtr<ID3D12Device5> device);

	// Creates descriptors based on 'descriptorJobs' returns DX12Descriptor struct vec that user must
	// control access to if 'registerIntoManager' is true. Otherwise, user can call 'getDescriptor'
	// to obtain any of the created descriptors at any time.
	std::vector<DX12Descriptor> makeDescriptors(std::vector<DescriptorJob> descriptorJobs, ResourceManager* resourceManager, ConstantBufferManager* constantBufferManager, bool registerIntoManager = true);

	bool containsDescriptorsOfType(DESCRIPTOR_TYPE type);

	DX12Descriptor* getDescriptor(const IndexedName& indexedName, const DESCRIPTOR_TYPE& type);
	std::vector<DX12Descriptor*>* getAllDescriptorsOfType(DESCRIPTOR_TYPE type);
	std::vector<ID3D12DescriptorHeap*> getAllBindableHeaps();
	std::vector<std::pair<D3D12_RESOURCE_STATES, DX12Resource*>> getRequiredResourceStates();

	void freeDescriptorRangeInHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, CD3DX12_CPU_DESCRIPTOR_HANDLE startHandle, UINT size);

private:
	void makeDescriptorHeaps();
	void createDescriptorView(DX12Descriptor& descriptor, DescriptorJob& job);

	UINT getDescriptorOffsetForType(D3D12_DESCRIPTOR_HEAP_TYPE type);
	D3D12_DESCRIPTOR_HEAP_TYPE getHeapTypeFromDescriptorType(DESCRIPTOR_TYPE type);
	D3D12_DESCRIPTOR_HEAP_FLAGS getShaderVisibleFlagFromHeapType(D3D12_DESCRIPTOR_HEAP_TYPE type);

private:
	DX12DescriptorHeap heaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	std::unordered_map<std::pair<IndexedName, DESCRIPTOR_TYPE>, DX12Descriptor, hash_pair> descriptors;
	std::unordered_map<DESCRIPTOR_TYPE, std::vector<DX12Descriptor*>> descriptorsByType;
	ComPtr<ID3D12Device5> device = nullptr;
};
