#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <vector>
using namespace Microsoft::WRL;

struct DX12Descriptor;

typedef enum DESCRIPTOR_TYPES {
	DESCRIPTOR_TYPE_NONE	= 0,
	DESCRIPTOR_TYPE_RTV		= 1,
	DESCRIPTOR_TYPE_DSV		= 2,
	DESCRIPTOR_TYPE_UAV		= 4,
	DESCRIPTOR_TYPE_SRV		= 8,
	DESCRIPTOR_TYPE_CBV		= 16
} DESCRIPTOR_TYPE;
DEFINE_ENUM_FLAG_OPERATORS(DESCRIPTOR_TYPE);

class DX12Resource {
public:
	DX12Resource(DESCRIPTOR_TYPES types, ID3D12Resource* res, D3D12_RESOURCE_STATES state);
	DX12Resource(ComPtr<ID3D12Device> device, DESCRIPTOR_TYPES types, 
		DXGI_FORMAT format, UINT texHeight, UINT texWidth);
	void changeState(ComPtr<ID3D12GraphicsCommandList> cmdList, D3D12_RESOURCE_STATES destState);
	ID3D12Resource* get();
	DXGI_FORMAT getFormat();

private:
	DESCRIPTOR_TYPE type;
	DXGI_FORMAT format;
	ComPtr<ID3D12Resource> resource = nullptr;
	D3D12_RESOURCE_STATES curState;
	std::vector<DX12Descriptor*> descriptors;
};

