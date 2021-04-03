#pragma once
#define NOMINMAX
#include <wrl.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <vector>

using namespace Microsoft::WRL;

// Note: CBV descriptors generally don't work since descriptors
//	aren't aware of the ring buffering of ConstantBuffers
typedef enum DESCRIPTOR_TYPES {
	DESCRIPTOR_TYPE_NONE	= 0,
	DESCRIPTOR_TYPE_RTV		= 1,
	DESCRIPTOR_TYPE_DSV		= 2,
	DESCRIPTOR_TYPE_UAV		= 4,
	DESCRIPTOR_TYPE_SRV		= 8,
	DESCRIPTOR_TYPE_CBV		= 16,
	DESCRIPTOR_TYPE_FLAG_SIMULTANEOUS_ACCESS = 32,
	DESCRIPTOR_TYPE_FLAG_RT_ACCEL_STRUCT = 64
} DESCRIPTOR_TYPE;
DEFINE_ENUM_FLAG_OPERATORS(DESCRIPTOR_TYPE);

// Represents a low level DX12 resource, at the moment only CommitedResources, and typically Textures
class DX12Resource {
public:
	DX12Resource() = default;
	DX12Resource(DESCRIPTOR_TYPES types, ID3D12Resource* res, D3D12_RESOURCE_STATES state);
	DX12Resource(ComPtr<ID3D12Device5> device, DESCRIPTOR_TYPES types, DXGI_FORMAT format, UINT texHeight, UINT texWidth);

	ID3D12Resource* get();
	D3D12_RESOURCE_STATES getState() const;
	DXGI_FORMAT getFormat();

	// Enqueues a resource barrier to the inputted 'transitionQueue' vector so the caller can batch transitions
	// TODO: find a better name, since "Deferred" in this program typically means multithreaded actions
	void changeStateDeferred(D3D12_RESOURCE_STATES destState, std::vector<CD3DX12_RESOURCE_BARRIER>& transitionQueue);
	void changeState(ComPtr<ID3D12GraphicsCommandList5> cmdList, D3D12_RESOURCE_STATES destState);

	// Local resources are only used by a single PipelineStage and aren't imported.
	// TODO: find a way to make this based on that local resources are a base type so there isn't such a waste of data
	bool local = true;

protected:
	DESCRIPTOR_TYPE type;
	DXGI_FORMAT format;
	ComPtr<ID3D12Resource> resource = nullptr;
	D3D12_RESOURCE_STATES curState;
};

