#include "DescriptorClasses\DescriptorManager.h"
#include "ResourceClasses\ResourceManager.h"
#include <string>
#include "Settings.h"

DescriptorManager::DescriptorManager(ComPtr<ID3D12Device> device) {
	this->device = device;
}

std::vector<DX12Descriptor*> DescriptorManager::makeDescriptorHeap(std::vector<DescriptorJob> descriptorJobs, ResourceManager* resourceManager, ConstantBufferManager* constantBufferManager) {
	DESCRIPTOR_TYPE descriptorType = DESCRIPTOR_TYPE_NONE;
	for (const DescriptorJob& job : descriptorJobs) {
		descriptorType |= job.type;
	}
	
	std::vector<DX12Descriptor*> generatedDescriptors;
	generatedDescriptors.reserve(descriptorJobs.size());

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = descriptorJobs.size();
	heapDesc.Type = heapTypeFromDescriptorType(descriptorType);
	heapDesc.Flags = shaderVisibleFromHeapType(heapDesc.Type);
	
	ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;
	device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap));
	descriptorHeaps.push_back(descriptorHeap);

	CD3DX12_CPU_DESCRIPTOR_HANDLE hCPUDescriptor(descriptorHeap->GetCPUDescriptorHandleForHeapStart());
	CD3DX12_GPU_DESCRIPTOR_HANDLE hGPUDescriptor(descriptorHeap->GetGPUDescriptorHandleForHeapStart());
	UINT descriptorSize = getDescriptorOffsetForType(heapDesc.Type);
	for (DescriptorJob& job : descriptorJobs) {
		DX12Descriptor& desc = descriptors[std::make_pair(job.name, job.type)];
		desc.cpuHandle = hCPUDescriptor;
		desc.gpuHandle = hGPUDescriptor;
		if (job.type == DESCRIPTOR_TYPE_CBV) {
			DX12ConstantBuffer* buffer = constantBufferManager->getConstantBuffer(job.target);
			desc.constantBufferTarget = buffer;
			job.cbvDesc.BufferLocation = buffer->get()->GetGPUVirtualAddress();
			job.cbvDesc.SizeInBytes = buffer->getBufferSize();
		}
		else {
			desc.resourceTarget = resourceManager->getResource(job.target);
		}

		createDescriptorView(desc, job);
		
		hCPUDescriptor.Offset(1, descriptorSize);
		hGPUDescriptor.Offset(1, descriptorSize);
		descriptorsByType[job.type].push_back(&desc);
		generatedDescriptors.emplace_back(&desc);
	}
	return generatedDescriptors;
}

DX12Descriptor* DescriptorManager::getDescriptor(std::string name, DESCRIPTOR_TYPE type) {
	return &descriptors.at(std::make_pair(name, type));
}

std::vector<ID3D12DescriptorHeap*> DescriptorManager::getAllHeaps() {
	std::vector<ID3D12DescriptorHeap*> allHeaps;
	for (ComPtr<ID3D12DescriptorHeap>& heap : descriptorHeaps) {
		allHeaps.push_back(heap.Get());
	}
	return allHeaps;
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
}

std::vector<DX12Descriptor*> DescriptorManager::getAllDescriptorsOfType(DESCRIPTOR_TYPE type) {
	return descriptorsByType.at(type);
}

void DescriptorManager::createDescriptorView(DX12Descriptor& descriptor, DescriptorJob& job) {
	switch (job.type) {
	case DESCRIPTOR_TYPE_RTV:
		device->CreateRenderTargetView(descriptor.resourceTarget->get(), &job.rtvDesc, descriptor.cpuHandle);
		break;
	case DESCRIPTOR_TYPE_DSV:
		device->CreateDepthStencilView(descriptor.resourceTarget->get(), &job.dsvDesc, descriptor.cpuHandle);
		break;
	case DESCRIPTOR_TYPE_SRV:
		device->CreateShaderResourceView(descriptor.resourceTarget->get(), &job.srvDesc, descriptor.cpuHandle);
		break;
	case DESCRIPTOR_TYPE_UAV:
		device->CreateUnorderedAccessView(descriptor.resourceTarget->get(), nullptr, &job.uavDesc, descriptor.cpuHandle);
		break;
	case DESCRIPTOR_TYPE_CBV:
		device->CreateConstantBufferView(&job.cbvDesc, descriptor.cpuHandle);
		break;
	default:
		OutputDebugStringA(("Couldn't create DescriptorView of type: " + std::to_string(job.type)).c_str());
		break;
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
		OutputDebugStringA("Can't support Samplers Yet...");
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