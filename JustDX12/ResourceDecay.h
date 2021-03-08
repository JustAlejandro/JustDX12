#pragma once
#include <Settings.h>

class DescriptorManager;

class ResourceDecay {
public:
	static void CheckDestroy();
	static void DestroyAll();
	// Destroys resource after CPU_FRAME_COUNT frames have progressed.
	// Useful for resources that could be in commands in flight, not useful for large temporary resources.
	static void DestroyAfterDelay(Microsoft::WRL::ComPtr<ID3D12Resource> resource);
	static void DestroyAfterSpecificDelay(Microsoft::WRL::ComPtr<ID3D12Resource> resource, UINT delay);
	static void DestroyAfterSpecificDelay(Microsoft::WRL::ComPtr<ID3D12QueryHeap> resource, UINT delay);
	static void DestroyOnEvent(Microsoft::WRL::ComPtr<ID3D12Resource> resource, HANDLE ev);
	// Function specifically used to keep two buffers in scope and setting a value on completion.
	// resource is the parameter flagged to be destroyed, at which point, dest will take on the value of src.
	static void DestroyOnEventAndFillPointer(Microsoft::WRL::ComPtr<ID3D12Resource> resource, HANDLE ev, Microsoft::WRL::ComPtr<ID3D12Resource> src, Microsoft::WRL::ComPtr<ID3D12Resource>* dest);
	static void DestroyOnDelayAndFillPointer(Microsoft::WRL::ComPtr<ID3D12Resource> resource, UINT delay, Microsoft::WRL::ComPtr<ID3D12Resource> src, Microsoft::WRL::ComPtr<ID3D12Resource>* dest);

	static void FreeDescriptorsAferDelay(DescriptorManager* manager, D3D12_DESCRIPTOR_HEAP_TYPE type, CD3DX12_CPU_DESCRIPTOR_HANDLE startHandle, UINT size);
private:
	ResourceDecay() = default;
	ResourceDecay(ResourceDecay const&) = delete;
	void operator=(ResourceDecay const&) = delete;

	static ResourceDecay& getInstance();

	struct FreeDescriptor {
		DescriptorManager* manager;
		D3D12_DESCRIPTOR_HEAP_TYPE type;
		CD3DX12_CPU_DESCRIPTOR_HANDLE startHandle;
		UINT size;
	};

	struct SwapEvent {
		union {
			HANDLE ev;
			UINT delay;
		};
		Microsoft::WRL::ComPtr<ID3D12Resource> src;
		Microsoft::WRL::ComPtr<ID3D12Resource>* dest;
		SwapEvent() = default;
		SwapEvent(HANDLE ev, Microsoft::WRL::ComPtr<ID3D12Resource> src = nullptr, Microsoft::WRL::ComPtr<ID3D12Resource>* dest = nullptr) {
			this->ev = ev;
			this->src = src;
			this->dest = dest;
		}
		SwapEvent(UINT delay, Microsoft::WRL::ComPtr<ID3D12Resource> src = nullptr, Microsoft::WRL::ComPtr<ID3D12Resource>* dest = nullptr) {
			this->delay = delay;
			this->src = src;
			this->dest = dest;
		}
	};

	std::array<std::vector<FreeDescriptor>, CPU_FRAME_COUNT> onDelayFreeDescriptor;

	std::list<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, SwapEvent>> onEventResources;
	std::list<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, int>> onSpecificDelayResources;
	std::array<std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>, CPU_FRAME_COUNT> onDelayResources;
	std::list<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, SwapEvent>> onDelaySwapResources;

	std::list<std::pair<Microsoft::WRL::ComPtr<ID3D12QueryHeap>, int>> onSpecificDelayQueries;
};

