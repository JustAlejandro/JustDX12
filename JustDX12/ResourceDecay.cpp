#include "ResourceDecay.h"

void ResourceDecay::CheckDestroy() {
	ResourceDecay& instance = getInstance();

	instance.onDelayResources[gFrameIndex].clear();

	for (auto iter = instance.onSpecificDelayResources.begin(); iter != instance.onSpecificDelayResources.end();) {
		iter->second--;
		if (iter->second == 0) {
			iter = instance.onSpecificDelayResources.erase(iter);
		}
		else {
			iter++;
		}
	}

	for (auto iter = instance.onSpecificDelayQueries.begin(); iter != instance.onSpecificDelayQueries.end();) {
		iter->second--;
		if (iter->second == 0) {
			iter = instance.onSpecificDelayQueries.erase(iter);
		}
		else {
			iter++;
		}
	}

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

void ResourceDecay::DestroyAll() {
	ResourceDecay& instance = getInstance();

	for (auto& vec : instance.onDelayResources) {
		vec.clear();
	}
	instance.onEventResources.clear();
	instance.onSpecificDelayQueries.clear();
	instance.onSpecificDelayResources.clear();
}

void ResourceDecay::DestroyAfterDelay(Microsoft::WRL::ComPtr<ID3D12Resource> resource) {
	ResourceDecay& instance = getInstance();
	instance.onDelayResources[gFrameIndex].push_back(resource);
}

void ResourceDecay::DestroyAfterSpecificDelay(Microsoft::WRL::ComPtr<ID3D12Resource> resource, UINT delay) {
	ResourceDecay& instance = getInstance();

	instance.onSpecificDelayResources.push_back(std::make_pair(resource, delay));
}

void ResourceDecay::DestroyAfterSpecificDelay(Microsoft::WRL::ComPtr<ID3D12QueryHeap> resource, UINT delay) {
	ResourceDecay& instance = getInstance();

	instance.onSpecificDelayQueries.push_back(std::make_pair(resource, delay));
}

void ResourceDecay::DestroyOnEvent(Microsoft::WRL::ComPtr<ID3D12Resource> resource, HANDLE ev) {
	ResourceDecay& instance = getInstance();
	instance.onEventResources.push_back(std::make_pair(resource, SwapEvent(ev)));
}

void ResourceDecay::DestroyOnEventAndFillPointer(Microsoft::WRL::ComPtr<ID3D12Resource> resource, HANDLE ev, Microsoft::WRL::ComPtr<ID3D12Resource> src, Microsoft::WRL::ComPtr<ID3D12Resource>* dest) {
	ResourceDecay& instance = getInstance();
	instance.onEventResources.push_back(std::make_pair(resource, SwapEvent(ev, src, dest)));
}

void ResourceDecay::DestroyOnDelayAndFillPointer(Microsoft::WRL::ComPtr<ID3D12Resource> resource, UINT delay, Microsoft::WRL::ComPtr<ID3D12Resource> src, Microsoft::WRL::ComPtr<ID3D12Resource>* dest) {
	ResourceDecay& instance = getInstance();
	instance.onDelaySwapResources.push_back(std::make_pair(resource, SwapEvent(delay, src, dest)));
}

ResourceDecay& ResourceDecay::getInstance() {
	static ResourceDecay instance;
	return instance;
}
