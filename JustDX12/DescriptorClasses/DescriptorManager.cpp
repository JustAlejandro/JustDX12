#include <string>

#include "DescriptorClasses\DescriptorManager.h"
#include "ResourceClasses\ResourceManager.h"

#include "Settings.h"

DescriptorManager::DescriptorManager(ComPtr<ID3D12Device5> device) {
	this->device = device;
	makeDescriptorHeaps();
}

std::vector<DX12Descriptor> DescriptorManager::makeDescriptors(std::vector<DescriptorJob> descriptorJobs, ResourceManager* resourceManager, ConstantBufferManager* constantBufferManager, bool registerIntoManager) {
	std::vector<DescriptorJob> jobByHeap[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	for (DescriptorJob& job : descriptorJobs) {
		jobByHeap[getHeapTypeFromDescriptorType(job.type)].push_back(job);
	}
	// Seperating jobs into their respective heaps speeds up allocation since they can be placed contiguously.
	std::vector<DX12Descriptor> newDescriptors;
	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++) {
		if (jobByHeap[i].size() == 0) {
			continue;
		}
		DX12DescriptorHeap& heap = heaps[i];
		std::pair<CD3DX12_CPU_DESCRIPTOR_HANDLE, CD3DX12_GPU_DESCRIPTOR_HANDLE> handles = heap.reserveHeapSpace((UINT)jobByHeap[i].size());
		for (DescriptorJob& job : jobByHeap[i]) {
			DX12Descriptor desc;
			desc.cpuHandle = handles.first;
			desc.gpuHandle = handles.second;
			desc.usage = job.usage;
			desc.usageIndex = job.usageIndex;

			if (job.directBinding) {
				desc.resourceTarget = job.directBindingTarget;
			}
			else {
				desc.resourceTarget = resourceManager->getResource(job.indirectTarget);
			}

			desc.descriptorHeap = &heap;
			createDescriptorView(desc, job);
			if (registerIntoManager) {
				auto registeredValue = descriptors.insert_or_assign(std::make_pair(IndexedName(job.name, job.usageIndex), job.type), desc);
				descriptorsByType[job.type].push_back(&registeredValue.first->second);
			}
			newDescriptors.push_back(desc);

			handles.first.Offset(1, heap.getOffset());
			handles.second.Offset(1, heap.getOffset());
		}
	}
	return newDescriptors;
}

bool DescriptorManager::containsDescriptorsOfType(DESCRIPTOR_TYPE type) {
	return descriptorsByType.find(type) != descriptorsByType.end();
}

DX12Descriptor* DescriptorManager::getDescriptor(const IndexedName& indexedName, const DESCRIPTOR_TYPE& type) {
	auto out = descriptors.find(std::make_pair(indexedName, type));
	if (out == descriptors.end()) {
		return nullptr;
	}
	return &out->second;
}

std::vector<DX12Descriptor*>* DescriptorManager::getAllDescriptorsOfType(DESCRIPTOR_TYPE type) {
	return &descriptorsByType.at(type);
}

std::vector<ID3D12DescriptorHeap*> DescriptorManager::getAllBindableHeaps() {
	return { heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].getHeap(),
			 heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].getHeap() };
}

std::vector<std::pair<D3D12_RESOURCE_STATES, DX12Resource*>> DescriptorManager::getRequiredResourceStates() {
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

void DescriptorManager::freeDescriptorRangeInHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, CD3DX12_CPU_DESCRIPTOR_HANDLE startHandle, UINT size) {
	heaps[type].freeHeapSpace(startHandle, size);
}

void DescriptorManager::makeDescriptorHeaps() {
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++) {
		D3D12_DESCRIPTOR_HEAP_TYPE type = static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i);
		heapDesc.NumDescriptors = maxDescriptorHeapSize[type];
		heapDesc.Type = type;
		heapDesc.Flags = getShaderVisibleFlagFromHeapType(type);
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heapRes;
		HRESULT result = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heapRes));
		if (result != S_OK) {
			OutputDebugStringA("Heap Creation Failed");
			throw "HEAP CREATION FAILED";
		}
		heaps[i] = DX12DescriptorHeap(heapRes, type, getDescriptorOffsetForType(type), heapDesc.NumDescriptors);
	}
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
	case DESCRIPTOR_TYPE_CBV:
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = descriptor.resourceTarget->get()->GetGPUVirtualAddress() + job.view.cbvDesc.offset;
		cbvDesc.SizeInBytes = job.view.cbvDesc.cbvSize;
		device->CreateConstantBufferView(&cbvDesc, descriptor.cpuHandle);
		break;
	default:
		OutputDebugStringA(("Couldn't create DescriptorView of type: " + std::to_string(job.type)).c_str());
		break;
	}
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
D3D12_DESCRIPTOR_HEAP_TYPE DescriptorManager::getHeapTypeFromDescriptorType(DESCRIPTOR_TYPE type) {
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

D3D12_DESCRIPTOR_HEAP_FLAGS DescriptorManager::getShaderVisibleFlagFromHeapType(D3D12_DESCRIPTOR_HEAP_TYPE type) {
	if (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER) {
		return D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	}
	return D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
}
