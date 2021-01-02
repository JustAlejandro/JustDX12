#include "ResourceClasses\DX12Resource.h"
#include "Settings.h"

DX12Resource::DX12Resource(DESCRIPTOR_TYPES types, ID3D12Resource* res, D3D12_RESOURCE_STATES state) {
	this->type = types;
	this->resource = res;
	this->curState = state;
	this->format = res->GetDesc().Format;
}

DX12Resource::DX12Resource(ComPtr<ID3D12Device2> device, DESCRIPTOR_TYPES types, DXGI_FORMAT format, UINT texHeight, UINT texWidth) {
	curState = D3D12_RESOURCE_STATE_COMMON;
	type = types;
	this->format = format;

	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = texWidth;
	texDesc.Height = texHeight;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = format;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
	if (types & DESCRIPTOR_TYPE_RTV) {
		flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	}
	if (types & DESCRIPTOR_TYPE_DSV) {
		flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	}
	if (types & DESCRIPTOR_TYPE_UAV) {
		flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}
	if (types & DESCRIPTOR_TYPE_FLAG_SIMULTANEOUS_ACCESS) {
		flags |= D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
	}

	texDesc.Flags = flags;

	D3D12_CLEAR_VALUE defaultDepthClear = DEFAULT_CLEAR_VALUE_DEPTH_STENCIL();
	D3D12_CLEAR_VALUE* clearVal = (types & DESCRIPTOR_TYPE_DSV) ? &defaultDepthClear : nullptr;
	device->CreateCommittedResource(
		&gDefaultHeapDesc,
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		curState,
		clearVal,
		IID_PPV_ARGS(&resource));
}

void DX12Resource::changeStateDeferred(D3D12_RESOURCE_STATES destState, std::vector<CD3DX12_RESOURCE_BARRIER>& transitionQueue) {
	if (curState == destState) return;

	transitionQueue.push_back(CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), curState, destState));
}

void DX12Resource::changeState(ComPtr<ID3D12GraphicsCommandList5> cmdList, D3D12_RESOURCE_STATES destState) {
	if (curState == destState) return;
	
	auto stateTransition = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), curState, destState);
	cmdList->ResourceBarrier(1, &stateTransition);
	curState = destState;
}

ID3D12Resource* DX12Resource::get() {
	return resource.Get();
}

D3D12_RESOURCE_STATES DX12Resource::getState() const {
	return curState;
}

DXGI_FORMAT DX12Resource::getFormat() {
	return format;
}
