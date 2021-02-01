#include "DX12App.h"
#include <windowsx.h>
#include <cassert>
#include <d3dx12.h>
#include "Settings.h"
#include "imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

using Microsoft::WRL::ComPtr;

LRESULT CALLBACK
MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	// Forward hwnd on because we can get messages (e.g., WM_CREATE)
	// before CreateWindow returns, and thus before mhMainWnd is valid.
	return DX12App::getApp()->MsgProc(hwnd, msg, wParam, lParam);
}

DX12App* DX12App::app = nullptr;
DX12App* DX12App::getApp() {
	return app;
}

Microsoft::WRL::ComPtr<ID3D12Device5> DX12App::getDevice() {
	return getApp()->md3dDevice;
}

DX12App::DX12App(HINSTANCE hInstance) : hAppInst(hInstance) {
	mBackBufferFormat = COLOR_TEXTURE_FORMAT;
	mDepthStencilFormat = DEPTH_TEXTURE_DSV_FORMAT;
	mClientWidth = SCREEN_WIDTH;
	mClientHeight = SCREEN_HEIGHT;
	assert(app == nullptr);
	app = this;
}

DX12App::~DX12App() {
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

float DX12App::getAspectRatio() const {
	return float(mClientWidth) / mClientHeight;
}

int DX12App::run() {
	MSG msg = { 0 };

	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			update();
			draw();
		}
	}
	return 0;
}

bool DX12App::initialize() {
	if (!initWindow()) {
		return false;
	}
	if (!initDX12()) {
		return false;
	}

	onResize();

	md3dDevice->CheckFeatureSupport(
		D3D12_FEATURE_D3D12_OPTIONS6,
		&vrsSupport,
		sizeof(vrsSupport));

	md3dDevice->CheckFeatureSupport(
		D3D12_FEATURE_D3D12_OPTIONS1,
		&waveSupport,
		sizeof(waveSupport));

	return true;
}

bool DX12App::initWindow() {
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hAppInst;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"MainWnd";

	if (!RegisterClass(&wc)) {
		MessageBox(0, L"RegisterClass Failed.", 0, 0);
		return false;
	}

	// Compute window rectangle dimensions based on requested client area dimensions.
	RECT R = { 0, 0, mClientWidth, mClientHeight };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	hWindow = CreateWindow(L"MainWnd", L"RenderTests",
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, hAppInst, 0);
	if (!hWindow) {
		MessageBox(0, L"CreateWindow Failed.", 0, 0);
		return false;
	}

	ShowWindow(hWindow, SW_SHOW);
	UpdateWindow(hWindow);

	GetWindowRect(hWindow, &hWindowPos);
	ClipCursor(&hWindowPos);
	hWindowCenter.x = (hWindowPos.left + hWindowPos.right) / 2;
	hWindowCenter.y = (hWindowPos.top + hWindowPos.bottom) / 2;
	SetCursorPos(hWindowCenter.x, hWindowCenter.y);
	return true;
}

bool DX12App::initDX12() {
#if defined(DEBUG) || defined(_DEBUG) || GPU_DEBUG
	ComPtr<ID3D12Debug> debug;
	if (D3D12GetDebugInterface(IID_PPV_ARGS(&debug)) < 0) {
		OutputDebugStringA("COULDN'T CREATE DEBUG INTERFACE\n");
		return false;
	}
	debug->EnableDebugLayer();
#endif

	CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory));

	HRESULT hardware = D3D12CreateDevice(nullptr,
		D3D_FEATURE_LEVEL_12_1,
		IID_PPV_ARGS(&md3dDevice));

	if (hardware < 0) {
		OutputDebugStringA("FAILED TO CREATE DX12 DEVICE WITH LEVEL 12.1\nDefaulting to WARP Adapter...");
		
		ComPtr<IDXGIAdapter4> warp;
		if (mdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&warp)) < 0) {
			OutputDebugStringA("COULDN'T FIND WARP ADAPTER\n");
			return false;
		}
		if (D3D12CreateDevice(warp.Get(),
				D3D_FEATURE_LEVEL_12_1,
				IID_PPV_ARGS(&md3dDevice)) < 0) {
			OutputDebugStringA("COULDN'T CREATE WARP ADAPTER OF FEATURE LEVEL 12.1\nMake sure you're on at least Windows 10 Build 1709");
			return false;
		}
	}

	BOOL allowTearing = FALSE;
	HRESULT tearingRes = mdxgiFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
	if (tearingRes < 0 || !allowTearing) {
		OutputDebugStringA("Device doesn't support DXGI_FEATURE_PRESENT_ALLOW_TEARING.");
		return false;
	}

	md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence));

	for (auto& fence : mAuxFences) {
		md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	}

	mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	gRtvDescriptorSize = mRtvDescriptorSize;
	mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	gDsvDescriptorSize = mDsvDescriptorSize;
	mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	gCbvSrvUavDescriptorSize = mCbvSrvUavDescriptorSize;
	gSamplerDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

	createCommandObjects();
	createSwapChain();
	createScreenRtvDsvDescriptorHeaps();

	return true;
}

void DX12App::createCommandObjects() {
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue));

	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mComputeCommandQueue));

	md3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(mDirectCmdListAlloc.GetAddressOf()));

	md3dDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		mDirectCmdListAlloc.Get(),
		nullptr,
		IID_PPV_ARGS(mCommandList.GetAddressOf()));

	mCommandList->Close();
}

void DX12App::createSwapChain() {
	mSwapChain.Reset();

	DXGI_SWAP_CHAIN_DESC sd;
	sd.BufferDesc.Width = mClientWidth;
	sd.BufferDesc.Height = mClientHeight;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = mBackBufferFormat;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = SwapChainBufferCount;
	sd.OutputWindow = hWindow;
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

	mdxgiFactory->CreateSwapChain(
		mCommandQueue.Get(),
		&sd,
		mSwapChain.GetAddressOf());
}

void DX12App::FlushCommandQueue() {
	mCurrentFence++;

	mCommandQueue->Signal(mFence.Get(), mCurrentFence);

	if (mFence->GetCompletedValue() < mCurrentFence) {
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		assert(eventHandle != NULL);

		mFence->SetEventOnCompletion(mCurrentFence, eventHandle);
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

ID3D12Resource* DX12App::CurrentBackBuffer()const {
	return mSwapChainBuffer[mCurrBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12App::CurrentBackBufferView()const {
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
		mCurrBackBuffer,
		mRtvDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12App::DepthStencilView()const {
	return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

LRESULT DX12App::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
	if (!lockMouse) {
		if (lockMouseDirty) {
			lockMouseDirty = false;
			ShowCursor(true);
			ClipCursor(nullptr);
		}
		return true;
	}
	if (lockMouseDirty) {
		lockMouseDirty = false;
		ShowCursor(false);
		GetWindowRect(hWindow, &hWindowPos);
		ClipCursor(&hWindowPos);
		hWindowCenter.x = (hWindowPos.left + hWindowPos.right) / 2;
		hWindowCenter.y = (hWindowPos.top + hWindowPos.bottom) / 2;
		SetCursorPos(hWindowCenter.x, hWindowCenter.y);
	}
	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0; 
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		mouseButtonDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		mouseButtonUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		mouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		SetCursorPos(hWindowCenter.x, hWindowCenter.y);
		return 0;
	case WM_KEYUP:
		if (wParam == VK_ESCAPE) {
			PostQuitMessage(0);
		}
		// Use VK_BLEH to put more keys
		return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

const D3D12_FEATURE_DATA_D3D12_OPTIONS6& DX12App::getVrsOptions() {
	return vrsSupport;
}

void DX12App::createScreenRtvDsvDescriptorHeaps() {
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	md3dDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf()));


	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	md3dDevice->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf()));

	D3D12_DESCRIPTOR_HEAP_DESC imguiHeapDesc;
	imguiHeapDesc.NumDescriptors = 1;
	imguiHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	imguiHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	imguiHeapDesc.NodeMask = 0;
	md3dDevice->CreateDescriptorHeap(
		&imguiHeapDesc, IID_PPV_ARGS(mImguiHeap.GetAddressOf()));
}

void DX12App::onResize() {
	assert(md3dDevice);
	assert(mSwapChain);
	assert(mDirectCmdListAlloc);

	// Flush before changing any resources.
	FlushCommandQueue();

	mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr);

	// Release the previous resources we will be recreating.
	for (int i = 0; i < SwapChainBufferCount; ++i)
		mSwapChainBuffer[i].Reset();
	mDepthStencilBuffer.Reset();

	// Resize the swap chain.
	mSwapChain->ResizeBuffers(
		SwapChainBufferCount,
		mClientWidth, mClientHeight,
		mBackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);

	mCurrBackBuffer = 0;

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < SwapChainBufferCount; i++) {
		mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i]));
		md3dDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
		rtvHeapHandle.Offset(1, mRtvDescriptorSize);
	}

	// Create the depth/stencil buffer and view.
	D3D12_RESOURCE_DESC depthStencilDesc;
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = mClientWidth;
	depthStencilDesc.Height = mClientHeight;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;

	// Correction 11/12/2016: SSAO chapter requires an SRV to the depth buffer to read from 
	// the depth buffer.  Therefore, because we need to create two views to the same resource:
	//   1. SRV format: DXGI_FORMAT_R24_UNORM_X8_TYPELESS
	//   2. DSV Format: DXGI_FORMAT_D24_UNORM_S8_UINT
	// we need to create the depth buffer resource with a typeless format.  
	depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;

	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.SampleDesc.Quality = 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = mDepthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;
	auto defaultHeapDesc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	md3dDevice->CreateCommittedResource(
		&defaultHeapDesc,
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf()));

	// Create descriptor to mip level 0 of entire resource using the format of the resource.
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = mDepthStencilFormat;
	dsvDesc.Texture2D.MipSlice = 0;
	md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &dsvDesc, DepthStencilView());

	// Transition the resource from its initial state to be used as a depth buffer.
	auto depthTransition = CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	mCommandList->ResourceBarrier(1, &depthTransition);

	// Execute the resize commands.
	mCommandList->Close();
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until resize is complete.
	FlushCommandQueue();

	// Update the viewport transform to cover the client area.
	mScreenViewport.TopLeftX = 0;
	mScreenViewport.TopLeftY = 0;
	mScreenViewport.Width = static_cast<float>(mClientWidth);
	mScreenViewport.Height = static_cast<float>(mClientHeight);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;

	mScissorRect = { 0, 0, mClientWidth, mClientHeight };
}
