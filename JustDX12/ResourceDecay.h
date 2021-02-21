#pragma once
#include <Settings.h>

class ResourceDecay {
public:
	static void CheckDestroy();
	// Destroys resource after CPU_FRAME_COUNT frames have progressed.
	// Useful for resources that could be in commands in flight, not useful for large temporary resources.
	static void DestroyAfterDelay(Microsoft::WRL::ComPtr<ID3D12Resource> resource);
	static void DestroyOnEvent(Microsoft::WRL::ComPtr<ID3D12Resource> resource, HANDLE ev);
	// Function specifically used to keep two buffers in scope and setting a value on completion.
	// resource is the parameter flagged to be destroyed, at which point, dest will take on the value of src.
	static void DestroyOnEventAndFillPointer(Microsoft::WRL::ComPtr<ID3D12Resource> resource, HANDLE ev, Microsoft::WRL::ComPtr<ID3D12Resource> src, Microsoft::WRL::ComPtr<ID3D12Resource>* dest);
private:
	ResourceDecay() = default;
	ResourceDecay(ResourceDecay const&) = delete;
	void operator=(ResourceDecay const&) = delete;

	static ResourceDecay& getInstance();

	struct SwapEvent {
		HANDLE ev;
		Microsoft::WRL::ComPtr<ID3D12Resource> src;
		Microsoft::WRL::ComPtr<ID3D12Resource>* dest;
		SwapEvent() = default;
		SwapEvent(HANDLE ev, Microsoft::WRL::ComPtr<ID3D12Resource> src = nullptr, Microsoft::WRL::ComPtr<ID3D12Resource>* dest = nullptr) {
			this->ev = ev;
			this->src = src;
			this->dest = dest;
		}
	};

	std::list<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, SwapEvent>> onEventResources;
	std::array<std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>, CPU_FRAME_COUNT> onDelayResources;
};

