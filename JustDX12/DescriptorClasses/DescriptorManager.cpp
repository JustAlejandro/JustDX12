#include "DescriptorClasses\DescriptorManager.h"
#include "ResourceClasses\ResourceManager.h"
#include <string>
#include "Settings.h"

DescriptorManager::DescriptorManager(ComPtr<ID3D12Device2> device) {
	this->device = device;
	makeDescriptorHeaps();
}

void DescriptorManager::makeDescriptors(std::vector<DescriptorJob> descriptorJobs, ResourceManager* resourceManager, ConstantBufferManager* constantBufferManager) {
	for (DescriptorJob& job : descriptorJobs) {
		DX12Descriptor& desc = descriptors[std::make_pair(job.name + std::to_string(job.usageIndex), job.type)];
		DX12DescriptorHeap& heap = heaps[heapTypeFromDescriptorType(job.type)];

		desc.cpuHandle = heap.endCPUHandle;
		desc.gpuHandle = heap.endGPUHandle;
		desc.usage = job.usage;
		desc.usageIndex = job.usageIndex;

		if (job.type == DESCRIPTOR_TYPE_CBV) {
			// Can't make a CBV descriptor because constant buffers are ring buffered per frame.
			OutputDebugStringA("Error: Can't make a CBV descriptor, try binding the CBV directly with root params");
			continue;
		}
		else {
			if (job.directBinding) {
				desc.resourceTarget = job.directBindingTarget;
			}
			else {
				desc.resourceTarget = resourceManager->getResource(job.indirectTarget);
			}
		}

		desc.descriptorHeap = heap.heap.Get();
		createDescriptorView(desc, job);
		descriptorsByType[job.type].push_back(&desc);

		heap.shiftHandles();
	}
}

DX12Descriptor* DescriptorManager::getDescriptor(const std::string& name, const DESCRIPTOR_TYPE& type) {
	auto out = descriptors.find(std::make_pair(name, type));
	if (out == descriptors.end()) {
		return nullptr;
	}
	return &out->second;
}

std::vector<ID3D12DescriptorHeap*> DescriptorManager::getAllBindableHeaps() {
	return { heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].heap.Get(),
			 heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].heap.Get() };
}

std::vector<std::pair<D3D12_RESOURCE_STATES, DX12Resource*>> DescriptorManager::requiredResourceStates() {
	std::vector<std::pair<D3D12_RESOURCE_STATES, DX12Resource*>> states;
	for (auto& entry : descriptors) {
		switch (entry.first.second) {
		case DESCRIPTOR_TYPE_NONE:
			states.emplace_back(D3D12_RESOURCE_STATE_COMMON, entry.second.resourceTarget);
			break;
		case DESCRIPTOR_TYPE_RTV:
			states.emplace_back(D3D12_RESOURCE_STATE_RENDER_TARGET, entry.second.resourceTarget);
			break;
		case DESCRIPTOR_TYPE_DSV:
			states.emplace_back(D3D12_RESOURCE_STATE_DEPTH_WRITE, entry.second.resourceTarget);
			break;
		case DESCRIPTOR_TYPE_UAV:
			states.emplace_back(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, entry.second.resourceTarget);
			break;
		case DESCRIPTOR_TYPE_SRV:
			states.emplace_back(D3D12_RESOURCE_STATE_GENERIC_READ, entry.second.resourceTarget);
			break;
		case DESCRIPTOR_TYPE_CBV:
			//Stateless(ish), doesn't really need to be changed
			break;
		default:
			throw "Invalid Type Given";
			break;
		}
	}
	return states;
}

std::vector<DX12Descriptor*>* DescriptorManager::getAllDescriptorsOfType(DESCRIPTOR_TYPE type) {
	return &descriptorsByType.at(type);
}

void DescriptorManager::createDescriptorView(DX12Descriptor& descriptor, DescriptorJob& job) {
	switch (job.type) {
	case DESCRIPTOR_TYPE_RTV:
		device->CreateRenderTargetView(descriptor.resourceTarget->get(), job.autoDesc ? nullptr : &job.view.rtvDesc, descriptor.cpuHandle);
		break;
	case DESCRIPTOR_TYPE_DSV:
		device->CreateDepthStencilView(descriptor.resourceTarget->get(), job.autoDesc ? nullptr : &job.view.dsvDesc, descriptor.cpuHandle);
		break;
	case DESCRIPTOR_TYPE_SRV:
		device->CreateShaderResourceView(descriptor.resourceTarget->get(), job.autoDesc ? nullptr : &job.view.srvDesc, descriptor.cpuHandle);
		break;
	case DESCRIPTOR_TYPE_UAV:
		device->CreateUnorderedAccessView(descriptor.resourceTarget->get(), nullptr, job.autoDesc ? nullptr : &job.view.uavDesc, descriptor.cpuHandle);
		break;
	default:
		OutputDebugStringA(("Couldn't create DescriptorView of type: " + std::to_string(job.type)).c_str());
		break;
	}
}

void DescriptorManager::makeDescriptorHeaps() {
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++) {
		D3D12_DESCRIPTOR_HEAP_TYPE type = static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i);
		heapDesc.NumDescriptors = maxDescriptorHeapSize[type];
		heapDesc.Type = type;
		heapDesc.Flags = shaderVisibleFromHeapType(type);
		HRESULT result = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heaps[i].heap));
		if (result != S_OK) {
			OutputDebugStringA("Heap Creation Failed");
			throw "HEAP CREATION FAILED";
		}

		heaps[i].size = heapDesc.NumDescriptors;
		heaps[i].type = type;
		heaps[i].offset = getDescriptorOffsetForType(type);
		heaps[i].startCPUHandle = heaps[i].heap->GetCPUDescriptorHandleForHeapStart();
		heaps[i].startGPUHandle = heaps[i].heap->GetGPUDescriptorHandleForHeapStart();
		heaps[i].endCPUHandle = heaps[i].startCPUHandle;
		heaps[i].endGPUHandle = heaps[i].startGPUHandle;
	}
}

D3D12_DESCRIPTOR_HEAP_TYPE DescriptorManager::heapTypeFromDescriptorType(DESCRIPTOR_TYPE type) {
	D3D12_DESCRIPTOR_HEAP_TYPE descriptorHeapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	int typeCounter = 0;
	if (type & DESCRIPTOR_TYPE_RTV) {
		descriptorHeapType = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		typeCounter++;
	}
	if (type & DESCRIPTOR_TYPE_DSV) {
		descriptorHeapType = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		typeCounter++;
	}
	if (type & (DESCRIPTOR_TYPE_SRV | DESCRIPTOR_TYPE_UAV | DESCRIPTOR_TYPE_CBV)) {
		descriptorHeapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		typeCounter++;
	}
	if (typeCounter != 1) {
		OutputDebugStringA(("Conflicting DESCIPTOR_TYPE recieved: " + std::to_string(type)).c_str());
	}
	return descriptorHeapType;
}

D3D12_DESCRIPTOR_HEAP_FLAGS DescriptorManager::shaderVisibleFromHeapType(D3D12_DESCRIPTOR_HEAP_TYPE type) {
	if (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER) {
		return D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	}
	return D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
}

UINT DescriptorManager::getDescriptorOffsetForType(D3D12_DESCRIPTOR_HEAP_TYPE type) {
	switch (type) {
	case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
		return gCbvSrvUavDescriptorSize;
	case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
		return gSamplerDescriptorSize;
		break;
	case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:
		return gRtvDescriptorSize;
	case D3D12_DESCRIPTOR_HEAP_TYPE_DSV:
		return gDsvDescriptorSize;
	default:
		break;
	}
	return -1;
}