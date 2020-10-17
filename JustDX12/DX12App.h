#pragma once
#include <windows.h>
#include <wrl.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <d3dx12.h>

#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")


class DX12App {
protected:
	DX12App(HINSTANCE hInstance);
	DX12App(const DX12App& rhs) = delete;
	DX12App& operator=(const DX12App& rhs) = delete;
	virtual ~DX12App();

public:
	static DX12App* getApp();

	static Microsoft::WRL::ComPtr<ID3D12Device> getDevice();

	float getAspectRatio()const;

	int run();

	virtual bool initialize();
	LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	const D3D12_FEATURE_DATA_D3D12_OPTIONS6& getVrsOptions();

protected:
	void createScreenRtvDsvDescriptorHeaps();
	virtual void update() = 0;
	virtual void draw() = 0;
	virtual void onResize();
	
	virtual void mouseButtonDown(WPARAM btnState, int x, int y) {};
	virtual void mouseButtonUp(WPARAM btnState, int x, int y) {};
	virtual void mouseMove(WPARAM btnState, int x, int y) {};

	bool initWindow();
	bool initDX12();
	void createCommandObjects();
	void createSwapChain();

	void FlushCommandQueue();

	ID3D12Resource* CurrentBackBuffer()const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView()const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView()const;

protected:
	static DX12App* app;

	HINSTANCE hAppInst = nullptr;
	HWND hWindow = nullptr;
	RECT hWindowPos = { 0 };
	POINT hWindowCenter = { 0 };

	Microsoft::WRL::ComPtr<IDXGIFactory4> mdxgiFactory;
	Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;
	Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice;

	Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
	UINT64 mCurrentFence = 0;

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList5> mCommandList;

	static const int SwapChainBufferCount = 2;
	int mCurrBackBuffer = 0;
	Microsoft::WRL::ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferCount];
	Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;

	D3D12_VIEWPORT mScreenViewport;
	D3D12_RECT mScissorRect;

	UINT mRtvDescriptorSize = 0;
	UINT mDsvDescriptorSize = 0;
	UINT mCbvSrvUavDescriptorSize = 0;

	D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
	DXGI_FORMAT mBackBufferFormat;
	DXGI_FORMAT mDepthStencilFormat;
	int mClientWidth;
	int mClientHeight;

	D3D12_FEATURE_DATA_D3D12_OPTIONS6 vrsSupport;
};

