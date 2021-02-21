#include "ResourceDecay.h"

void ResourceDecay::CheckDestroy() {
	ResourceDecay& instance = getInstance();

	instance.onDelayResources[gFrameIndex].clear();

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

void ResourceDecay::DestroyAfterDelay(Microsoft::WRL::ComPtr<ID3D12Resource> resource) {
	ResourceDecay& instance = getInstance();

	instance.onDelayResources[gFrameIndex].push_back(resource);
}

void ResourceDecay::DestroyOnEvent(Microsoft::WRL::ComPtr<ID3D12Resource> resource, HANDLE ev) {
	ResourceDecay& instance = getInstance();
	instance.onEventResources.push_back(std::make_pair(resource, SwapEvent(ev)));
}

void ResourceDecay::DestroyOnEventAndFillPointer(Microsoft::WRL::ComPtr<ID3D12Resource> resource, HANDLE ev, Microsoft::WRL::ComPtr<ID3D12Resource> src, Microsoft::WRL::ComPtr<ID3D12Resource>* dest) {
	ResourceDecay& instance = getInstance();
	instance.onEventResources.push_back(std::make_pair(resource, SwapEvent(ev, src, dest)));
}

ResourceDecay& ResourceDecay::getInstance() {
	static ResourceDecay instance;
	return instance;
}
