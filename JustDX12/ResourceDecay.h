#pragma once
#include <Settings.h>
#include <mutex>

class DescriptorManager;

// ResourceDecay is a Singleton that helps the program safely remove GPU side resources, meaning the CPU can "delete"
// a resource, and the ResourceDecay structure will hold onto the resource until a condition is met
// Typically this would be wanting to wait until the GPU has processed all commands using a resources before fully removing it
// Also offers functionality of being able to fill pointers when events are completed, making it a good fit for updating the
// ModelLoader
// checkDestroy() must be called for this Singleton to make any updates though, otherwise it will just hold onto resources forever
class ResourceDecay {
private:
	ResourceDecay() = default;
	ResourceDecay(ResourceDecay const&) = delete;
	void operator=(ResourceDecay const&) = delete;

	static ResourceDecay& getInstance();
public:
	// Performs any delete or swap operations needed this frame. Must be called at the start of every frame.
	static void checkDestroy();
	// Clears all resources that the ResourceDecay is keeping alive. Useful for debugging GPU memory leaks before program exits
	static void destroyAll();
	// Destroys resource after CPU_FRAME_COUNT frames have progressed.
	// Useful for resources that could be in commands in flight, not useful for large temporary resources.
	static void destroyAfterDelay(Microsoft::WRL::ComPtr<ID3D12Resource> resource);
	static void destroyAfterSpecificDelay(Microsoft::WRL::ComPtr<ID3D12Resource> resource, UINT delay);
	static void destroyAfterSpecificDelay(Microsoft::WRL::ComPtr<ID3D12QueryHeap> resource, UINT delay);
	static void destroyOnEvent(Microsoft::WRL::ComPtr<ID3D12Resource> resource, HANDLE ev);
	// Function specifically used to keep two buffers in scope and setting a value on completion.
	// resource is the parameter flagged to be destroyed, at which point, dest will take on the value of src.
	static void destroyOnEventAndFillPointer(Microsoft::WRL::ComPtr<ID3D12Resource> resource, HANDLE ev, Microsoft::WRL::ComPtr<ID3D12Resource> src, Microsoft::WRL::ComPtr<ID3D12Resource>* dest);
	static void destroyOnDelayAndFillPointer(Microsoft::WRL::ComPtr<ID3D12Resource> resource, UINT delay, Microsoft::WRL::ComPtr<ID3D12Resource> src, Microsoft::WRL::ComPtr<ID3D12Resource>* dest);

	static void freeDescriptorsAferDelay(DescriptorManager* manager, D3D12_DESCRIPTOR_HEAP_TYPE type, CD3DX12_CPU_DESCRIPTOR_HANDLE startHandle, UINT size);

private:

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

	// Needs to be thread-safe, so for now doing the old and bad approach of just giving each one a lock (hasn't come up, but it will)
	std::mutex onDelayFreeDescriptorMutex;
	std::array<std::vector<FreeDescriptor>, CPU_FRAME_COUNT> onDelayFreeDescriptor;

	std::mutex onEventResourcesMutex;
	std::list<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, SwapEvent>> onEventResources;
	std::mutex onSpecificDelayResourcesMutex;
	std::list<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, int>> onSpecificDelayResources;
	std::mutex onDelayResourcesMutex;
	std::array<std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>, CPU_FRAME_COUNT> onDelayResources;
	std::mutex onDelaySwapResourcesMutex;
	std::list<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, SwapEvent>> onDelaySwapResources;

	std::mutex onSpecificDelayQueriesMutex;
	std::list<std::pair<Microsoft::WRL::ComPtr<ID3D12QueryHeap>, int>> onSpecificDelayQueries;
};

