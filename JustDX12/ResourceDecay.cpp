#include "ResourceDecay.h"
#include "DescriptorClasses/DescriptorManager.h"

void ResourceDecay::checkDestroy() {
	ResourceDecay& instance = getInstance();

	{
		std::lock_guard<std::mutex> lk(instance.onDelayResourcesMutex);
		instance.onDelayResources[gFrameIndex].clear();
	}

	{
		std::lock_guard<std::mutex> lk(instance.onDelayFreeDescriptorMutex);
		for (auto& freeDesc : instance.onDelayFreeDescriptor[gFrameIndex]) {
			freeDesc.manager->freeDescriptorRangeInHeap(freeDesc.type, freeDesc.startHandle, freeDesc.size);
		}
		instance.onDelayFreeDescriptor[gFrameIndex].clear();
	}

	{
		std::lock_guard<std::mutex> lk(instance.onSpecificDelayResourcesMutex);
		for (auto iter = instance.onSpecificDelayResources.begin(); iter != instance.onSpecificDelayResources.end();) {
			iter->second--;
			if (iter->second == 0) {
				iter = instance.onSpecificDelayResources.erase(iter);
			}
			else {
				iter++;
			}
		}
	}

	{
		std::lock_guard<std::mutex> lk(instance.onSpecificDelayQueriesMutex);
		for (auto iter = instance.onSpecificDelayQueries.begin(); iter != instance.onSpecificDelayQueries.end();) {
			iter->second--;
			if (iter->second == 0) {
				iter = instance.onSpecificDelayQueries.erase(iter);
			}
			else {
				iter++;
			}
		}
	}

	{
		std::lock_guard<std::mutex> lk(instance.onDelaySwapResourcesMutex);
		for (auto iter = instance.onDelaySwapResources.begin(); iter != instance.onDelaySwapResources.end();) {
			iter->second.delay--;
			if (iter->second.delay == 0) {
				*iter->second.dest = iter->second.src.Get();
				iter = instance.onDelaySwapResources.erase(iter);
			}
			else {
				iter++;
			}
		}
	}

	{
		std::lock_guard<std::mutex> lk(instance.onEventResourcesMutex);
		for (auto iter = instance.onEventResources.begin(); iter != instance.onEventResources.end();) {
			DWORD status = WaitForSingleObject(iter->second.ev, 0);
			// Never waits if not ready, just checking if the event is completed.
			if (status == WAIT_OBJECT_0) {
				if (iter->second.dest != nullptr) {
					// Swap Pointers if asked
					*iter->second.dest = iter->second.src.Get();
				}
				CloseHandle(iter->second.ev);
				iter = instance.onEventResources.erase(iter);
			}
			else if (status == WAIT_FAILED) {
				OutputDebugString(L"EVENT WAIT FAILED");
				throw;
			}
			else {
				iter++;
			}
		}
	}
}

void ResourceDecay::destroyAll() {
	ResourceDecay& instance = getInstance();

	for (auto& vec : instance.onDelayResources) {
		vec.clear();
	}
	instance.onEventResources.clear();
	instance.onSpecificDelayQueries.clear();
	instance.onSpecificDelayResources.clear();
}

void ResourceDecay::destroyAfterDelay(Microsoft::WRL::ComPtr<ID3D12Resource> resource) {
	ResourceDecay& instance = getInstance();
	std::lock_guard<std::mutex> lk(instance.onDelayResourcesMutex);
	instance.onDelayResources[gFrameIndex].push_back(resource);
}

void ResourceDecay::destroyAfterSpecificDelay(Microsoft::WRL::ComPtr<ID3D12Resource> resource, UINT delay) {
	ResourceDecay& instance = getInstance();
	std::lock_guard<std::mutex> lk(instance.onSpecificDelayResourcesMutex);
	instance.onSpecificDelayResources.push_back(std::make_pair(resource, delay));
}

void ResourceDecay::destroyAfterSpecificDelay(Microsoft::WRL::ComPtr<ID3D12QueryHeap> resource, UINT delay) {
	ResourceDecay& instance = getInstance();
	std::lock_guard<std::mutex> lk(instance.onSpecificDelayQueriesMutex);
	instance.onSpecificDelayQueries.push_back(std::make_pair(resource, delay));
}

void ResourceDecay::destroyOnEvent(Microsoft::WRL::ComPtr<ID3D12Resource> resource, HANDLE ev) {
	ResourceDecay& instance = getInstance();
	std::lock_guard<std::mutex> lk(instance.onEventResourcesMutex);
	instance.onEventResources.push_back(std::make_pair(resource, SwapEvent(ev)));
}

void ResourceDecay::destroyOnEventAndFillPointer(Microsoft::WRL::ComPtr<ID3D12Resource> resource, HANDLE ev, Microsoft::WRL::ComPtr<ID3D12Resource> src, Microsoft::WRL::ComPtr<ID3D12Resource>* dest) {
	ResourceDecay& instance = getInstance();
	std::lock_guard<std::mutex> lk(instance.onEventResourcesMutex);
	instance.onEventResources.push_back(std::make_pair(resource, SwapEvent(ev, src, dest)));
}

void ResourceDecay::destroyOnDelayAndFillPointer(Microsoft::WRL::ComPtr<ID3D12Resource> resource, UINT delay, Microsoft::WRL::ComPtr<ID3D12Resource> src, Microsoft::WRL::ComPtr<ID3D12Resource>* dest) {
	ResourceDecay& instance = getInstance();
	std::lock_guard<std::mutex> lk(instance.onDelaySwapResourcesMutex);
	instance.onDelaySwapResources.push_back(std::make_pair(resource, SwapEvent(delay, src, dest)));
}

void ResourceDecay::freeDescriptorsAferDelay(DescriptorManager* manager, D3D12_DESCRIPTOR_HEAP_TYPE type, CD3DX12_CPU_DESCRIPTOR_HANDLE startHandle, UINT size) {
	ResourceDecay& instance = getInstance();
	std::lock_guard<std::mutex> lk(instance.onDelayFreeDescriptorMutex);
	instance.onDelayFreeDescriptor[gFrameIndex].push_back({ manager, type, startHandle, size });
}

ResourceDecay& ResourceDecay::getInstance() {
	static ResourceDecay instance;
	return instance;
}
