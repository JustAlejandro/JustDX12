#include <iostream>
#include "ModelLoading\ModelLoader.h"
#include "ModelLoading\TextureLoader.h"
#include "ModelLoading\Model.h"
#include "DX12App.h"
#include <d3dx12.h>
#include <unordered_map>
#include "FrameResource.h"
#include "DX12Helper.h"
#include <DirectXColors.h>
#include "PipelineStage/ComputePipelineStage.h"


std::string baseDir = "..\\Models";
std::string inputfile = "teapot.obj";
std::string sponzaDir = baseDir + "\\sponza";
std::string sponzaFile = "sponza.obj";

std::string warn;
std::string err;

const int numFrameResources = 3;

class DemoApp : public DX12App {
public:
	DemoApp(HINSTANCE hInstance);
	DemoApp(const DemoApp& rhs) = delete;
	DemoApp& operator=(const DemoApp& rhs) = delete;
	~DemoApp();

	virtual bool initialize()override;

	virtual void update()override;

	virtual void draw()override;

	virtual void onResize()override;

private:
	void renderObj();

	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildDescriptorHeaps();
	void BuildFrameResources();
	void BuildPSOs();

	void onKeyboardInput();

	virtual void mouseButtonDown(WPARAM btnState, int x, int y);
	virtual void mouseButtonUp(WPARAM btnState, int x, int y);
	virtual void mouseMove(WPARAM btnState, int x, int y);

	void updateCamera();
	
	void UpdateObjectCBs();
	void UpdateMaterialCBs();
	void UpdateMainPassCB();

private:
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	ComputePipelineStage* computeStage = nullptr;
	ModelLoader* modelLoader = nullptr;
	TextureLoader* textureLoader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mSSAORootSignature = nullptr;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> shaders;
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> defaultPSO = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> ssaoPSO = nullptr;

	std::vector<Model*> objectsToRender;

	PerPassConstants mainPassCB;

	DirectX::XMFLOAT4 eyePos = { 0.0f,70.0f,-10.0f, 1.0f };
	DirectX::XMFLOAT4X4 view = Identity();
	DirectX::XMFLOAT4X4 proj = Identity();

	float lookAngle[3] = { 0.0f, 0.0f, 0.0f };

	float mTheta = 1.3f * DirectX::XM_PI;
	float mPhi = 0.4f * DirectX::XM_PI;
	float mRadius = 10.0f;

	POINT lastMousePos;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd) {
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	try {
		DemoApp app(hInstance);
		if (!app.initialize())
			return 0;
		return app.run();
	}
	catch (...) {
		MessageBox(nullptr, L"Not Sure What",L"Oof, something broke.", MB_OK);
		return 0;
	}
}

DemoApp::DemoApp(HINSTANCE hInstance) : DX12App(hInstance) {
}

DemoApp::~DemoApp() {
	if (md3dDevice != nullptr)
		FlushCommandQueue();
	delete modelLoader;
	delete computeStage;
	for (int i = 0; i < objectsToRender.size(); i++) {
		delete objectsToRender[i];
	}
}

bool DemoApp::initialize() {
	if (!DX12App::initialize()) {
		return false;
	}

	DescriptorJob depthTex;
	depthTex.name = "inputDepth";
	depthTex.target = "renderOutputTex";
	depthTex.type = DESCRIPTOR_TYPE_SRV;
	depthTex.srvDesc = DEFAULT_SRV_DESC();
	DescriptorJob outTexDesc;
	outTexDesc.name = "SSAOOut";
	outTexDesc.target = "SSAOOutTexture";
	outTexDesc.type = DESCRIPTOR_TYPE_UAV;
	outTexDesc.uavDesc = DEFAULT_UAV_DESC();
	RootParamDesc rootPDesc;
	rootPDesc.name = "inputDepth";
	rootPDesc.type = ROOT_PARAMETER_TYPE_SRV;
	rootPDesc.numConstants = 1;
	rootPDesc.rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	RootParamDesc uavPDesc;
	uavPDesc.name = "SSAOOut";
	uavPDesc.type = ROOT_PARAMETER_TYPE_UAV;
	uavPDesc.numConstants = 1;
	uavPDesc.rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	ResourceJob outTex;
	outTex.name = "SSAOOutTexture";
	outTex.types = DESCRIPTOR_TYPE_UAV;
	ShaderDesc SSAOShaders;
	SSAOShaders.fileName = "..\\Shaders\\SSAO.hlsl";
	SSAOShaders.methodName = "SSAO";
	SSAOShaders.shaderName = "SSAO";
	SSAOShaders.type = SHADER_TYPE_CS;
	SSAOShaders.defines = nullptr;
	DX12Resource* leakingResource = new DX12Resource(DESCRIPTOR_TYPE_SRV, 
		deferredRenderPass.mAttachments[0].Get(), D3D12_RESOURCE_STATE_COMMON);

	PipeLineStageDesc stageDesc;
	stageDesc.descriptorJobs = { {depthTex, outTexDesc} };
	stageDesc.rootSigDesc = { rootPDesc, uavPDesc };
	stageDesc.samplerDesc = {};
	stageDesc.resourceJobs = { outTex };
	stageDesc.shaderFiles = { SSAOShaders };
	stageDesc.externalResources = { std::make_pair("renderOutputTex",leakingResource) };

	computeStage = new ComputePipelineStage(md3dDevice);

	computeStage->deferSetup(stageDesc);

	modelLoader = new ModelLoader(md3dDevice);
	objectsToRender.push_back(modelLoader->loadModel(sponzaFile, sponzaDir));

	//ssaoContainer = new SSAO(md3dDevice.Get(), mClientWidth, mClientHeight, mBackBufferFormat, mCbvSrvUavDescriptorSize);

	mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr);

	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();
	BuildFrameResources();
	BuildPSOs();
	//ssaoContainer->BuildDescriptors(deferredRenderPass.mAttachments[0].Get());

	mCommandList->Close();
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	FlushCommandQueue();

	return true;
}

void DemoApp::update() {
	//MessageBox(nullptr, L"Ladies and Gentlemen", L"We got em.", MB_OK);
	onKeyboardInput();
	updateCamera();

	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % numFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence) {
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle);
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	UpdateObjectCBs();
	UpdateMaterialCBs();
	UpdateMainPassCB();
}

void DemoApp::draw() {
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	cmdListAlloc->Reset();

	mCommandList->Reset(cmdListAlloc.Get(), defaultPSO.Get());

	//ID3D12DescriptorHeap* heaps[1] = { ssaoContainer->descriptorHeap.Get() };
	//mCommandList->SetDescriptorHeaps(1, heaps);

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), DirectX::Colors::LightSteelBlue,
		0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(),
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1.0f, 0, 0, nullptr);

	//mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());
	mCommandList->OMSetRenderTargets(3, &DeferredResourceView(), true, &DepthStencilView());

	// Here's where the SRV descriptor heap goes... Probably

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	ID3D12Resource* passCB = mCurrFrameResource->passCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

	// Draw the objects
	renderObj();

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(DeferredResource(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON));

	computeStage->deferExecute();
	int computeFenceValue = computeStage->triggerFence();
	WaitOnFenceForever(computeStage->getFence(), computeFenceValue);
	/*
	ssaoContainer->Execute(mCommandList.Get(), mSSAORootSignature.Get(), ssaoPSO.Get(),
		DeferredResource(), DeferredResourceViewGPU());

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(DeferredResource(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));
		*/
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST));

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(DeferredResource(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE));

	DX12Resource* SSAOOut = computeStage->getOut();
	SSAOOut->changeState(mCommandList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	mCommandList->CopyResource(CurrentBackBuffer(), SSAOOut->get());

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(DeferredResource(),
		D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT));
	/*
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST));

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(DeferredResource(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE));

	mCommandList->CopyResource(CurrentBackBuffer(), DeferredResource());

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(DeferredResource(),
		D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT));
		*/
	mCommandList->Close();

	ID3D12CommandList* cmdList[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdList), cmdList);

	mSwapChain->Present(0, 0);
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	mCurrFrameResource->Fence = ++mCurrentFence;

	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void DemoApp::onResize() {
	DX12App::onResize();

	DirectX::XMMATRIX P = DirectX::XMMatrixPerspectiveFovLH(0.25f * DirectX::XM_PI, getAspectRatio(), 1.0f, 5000.f);
	XMStoreFloat4x4(&proj, P);
}

void DemoApp::renderObj() {
	UINT objCBByteSize = CalcConstantBufferByteSize(sizeof(PerObjectConstants));
	
	ID3D12Resource* objectCB = mCurrFrameResource->objectCB->Resource();
	
	if (!objectsToRender[0]->loaded) {
		return;
	}

	mCommandList->IASetVertexBuffers(0, 1, &objectsToRender[0]->vertexBufferView());
	mCommandList->IASetIndexBuffer(&objectsToRender[0]->indexBufferView());
	mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	D3D12_GPU_VIRTUAL_ADDRESS objCBAdress = objectCB->GetGPUVirtualAddress() + 0;

	mCommandList->SetGraphicsRootConstantBufferView(0, objCBAdress);

	//mCommandList->DrawIndexedInstanced(objectsToRender[0]->indices.size(),
	//	1, 0, 0, 0);
	for (const Mesh& m : objectsToRender[0]->meshes) {
		mCommandList->DrawIndexedInstanced(m.indexCount,
			1, m.startIndexLocation, m.baseVertexLocation, 0);
	}
}

void DemoApp::BuildRootSignature() {
	CD3DX12_ROOT_PARAMETER slotRootParameter[3];

	slotRootParameter[0].InitAsConstantBufferView(0);
	slotRootParameter[1].InitAsConstantBufferView(1);
	slotRootParameter[2].InitAsConstantBufferView(2);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		3,
		slotRootParameter,
		0,
		nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr) {
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}

	md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf()));

	// SSAO Root Signature
	CD3DX12_DESCRIPTOR_RANGE srvTable;
	srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	CD3DX12_DESCRIPTOR_RANGE uavTable;
	uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	CD3DX12_ROOT_PARAMETER CSslotRootParameter[3];

	CSslotRootParameter[0].InitAsConstants(4, 0);
	CSslotRootParameter[1].InitAsDescriptorTable(1, &srvTable);
	CSslotRootParameter[2].InitAsDescriptorTable(1, &uavTable);

	CD3DX12_ROOT_SIGNATURE_DESC CSrootSigDesc(3, CSslotRootParameter,
		0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	Microsoft::WRL::ComPtr<ID3DBlob> CSserializedRootSig = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> CSerrorBlob = nullptr;
	HRESULT CShr = D3D12SerializeRootSignature(&CSrootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		CSserializedRootSig.GetAddressOf(), CSerrorBlob.GetAddressOf());
	if (CSerrorBlob != nullptr) {
		OutputDebugStringA((char*)CSerrorBlob->GetBufferPointer());
	}
	md3dDevice->CreateRootSignature(
		0,
		CSserializedRootSig->GetBufferPointer(),
		CSserializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mSSAORootSignature.GetAddressOf()));
}

void DemoApp::BuildShadersAndInputLayout() {
	shaders["standardVS"] = compileShader(L"..\\Shaders\\Default.hlsl",
		nullptr, "VS", "vs_5_0");
	shaders["standardPS"] = compileShader(L"..\\Shaders\\Default.hlsl",
		nullptr, "PS", "ps_5_0");

	shaders["SSAOCS"] = compileShader(L"..\\Shaders\\SSAO.hlsl",
		nullptr, "SSAO", "cs_5_0");

	inputLayout = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0 , 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA}
	};
}

void DemoApp::BuildDescriptorHeaps() {
	// TODO: Need to make these for textures, but for now not needed.
}

void DemoApp::BuildFrameResources() {
	// TODO: Need to make a new frame resource array here.
	for (int i = 0; i < numFrameResources; i++) {
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			1, (UINT)/*Num of objects*/1, (UINT)/*Num of Materials*/0));
	}
}

void DemoApp::BuildPSOs() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC defaultPsoDesc;

	ZeroMemory(&defaultPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	defaultPsoDesc.InputLayout = { inputLayout.data(), (UINT)inputLayout.size() };
	defaultPsoDesc.pRootSignature = mRootSignature.Get();
	defaultPsoDesc.VS = {
		reinterpret_cast<BYTE*>(shaders["standardVS"]->GetBufferPointer()),
		shaders["standardVS"]->GetBufferSize()};
	defaultPsoDesc.PS = {
		reinterpret_cast<BYTE*>(shaders["standardPS"]->GetBufferPointer()),
		shaders["standardPS"]->GetBufferSize()
	};
	defaultPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	defaultPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	defaultPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	defaultPsoDesc.SampleMask = UINT_MAX;
	defaultPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	defaultPsoDesc.NumRenderTargets = 3;
	defaultPsoDesc.RTVFormats[0] = mBackBufferFormat;
	defaultPsoDesc.RTVFormats[1] = mBackBufferFormat;
	defaultPsoDesc.RTVFormats[2] = mBackBufferFormat;
	defaultPsoDesc.SampleDesc.Count = 1;
	defaultPsoDesc.SampleDesc.Quality = 0;
	defaultPsoDesc.DSVFormat = mDepthStencilFormat;
	if (md3dDevice->CreateGraphicsPipelineState(&defaultPsoDesc, IID_PPV_ARGS(&defaultPSO)) < 0) {
		OutputDebugStringA("PSO setup failed");
	}

	D3D12_COMPUTE_PIPELINE_STATE_DESC ssaoPSODesc = {};
	ssaoPSODesc.pRootSignature = mSSAORootSignature.Get();
	ssaoPSODesc.CS = {
		reinterpret_cast<BYTE*>(shaders["SSAOCS"]->GetBufferPointer()),
		shaders["SSAOCS"]->GetBufferSize() };
	ssaoPSODesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	if (md3dDevice->CreateComputePipelineState(&ssaoPSODesc, IID_PPV_ARGS(&ssaoPSO)) < 0) {
		OutputDebugStringA("Compute PSO setup failed");
	}

}

void DemoApp::onKeyboardInput() {
	DirectX::XMFLOAT4 move = { 0.0f, 0.0f, 0.0f, 0.0f };

	DirectX::XMMATRIX look = DirectX::XMMatrixRotationRollPitchYaw(lookAngle[0], lookAngle[1], lookAngle[2]);

	if (GetAsyncKeyState('W') & 0x8000) {
		move.z += 0.25;
	}
	if (GetAsyncKeyState('S') & 0x8000) {
		move.z -= 0.25;
	}
	if (GetAsyncKeyState('A') & 0x8000) {
		move.x -= 0.25;
	}
	if (GetAsyncKeyState('D') & 0x8000) {
		move.x += 0.25;
	}


	DirectX::XMFLOAT4 moveRes = {};
	DirectX::XMStoreFloat4(&moveRes, DirectX::XMVector4Transform(
		DirectX::XMVectorSet(move.x, move.y, move.z, move.w), look));
	
	eyePos.x += moveRes.x;
	eyePos.y += moveRes.y * 0.0f;
	eyePos.z += moveRes.z;
}

void DemoApp::mouseButtonDown(WPARAM btnState, int x, int y) {
	lastMousePos.x = x;
	lastMousePos.y = y;

	SetCapture(hWindow);
}

void DemoApp::mouseButtonUp(WPARAM btnState, int x, int y) {
	ReleaseCapture();
}

void DemoApp::mouseMove(WPARAM btnState, int x, int y) {
	float dx = float(x - (SCREEN_WIDTH / 2));
	float dy = float(y - (SCREEN_HEIGHT / 2) + 12);

	lookAngle[0] = (lookAngle[0] + DirectX::XMConvertToRadians(0.25f * dy));
	lookAngle[1] = (lookAngle[1] + DirectX::XMConvertToRadians(0.25f * dx));
}

void DemoApp::updateCamera() {
	// Convert Spherical to Cartesian coordinates.
	//eyePos.x = mRadius * sinf(mPhi) * cosf(mTheta);
	//eyePos.z = mRadius * sinf(mPhi) * sinf(mTheta);
	//eyePos.y = mRadius * cosf(mPhi);

	// Build the view matrix.
	DirectX::XMVECTOR pos = DirectX::XMLoadFloat4(&eyePos);
	DirectX::XMVECTOR target = DirectX::XMVectorZero();
	target = DirectX::XMVectorAdd(DirectX::XMVector4Transform(
		DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), 
		DirectX::XMMatrixRotationRollPitchYaw(lookAngle[0], lookAngle[1], lookAngle[2])), pos);
	DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&this->view, view);
}

void DemoApp::UpdateObjectCBs() {
	UploadBuffer<PerObjectConstants>* objCB = mCurrFrameResource->objectCB.get();

	for (Model* model : objectsToRender) {
		DirectX::XMMATRIX world = DirectX::XMMatrixTranslation(model->pos.x, model->pos.y, model->pos.z);
		
		PerObjectConstants objConst;
		XMStoreFloat4x4(&objConst.World, XMMatrixTranspose(world));

		objCB->copyData(0, objConst);
	}
}

void DemoApp::UpdateMaterialCBs() {
}

void DemoApp::UpdateMainPassCB() {
	DirectX::XMMATRIX view = XMLoadFloat4x4(&this->view);
	DirectX::XMMATRIX proj = XMLoadFloat4x4(&this->proj);

	DirectX::XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	DirectX::XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	DirectX::XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	DirectX::XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);
	
	XMStoreFloat4x4(&mainPassCB.view, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mainPassCB.invView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mainPassCB.proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mainPassCB.invProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mainPassCB.viewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mainPassCB.invViewProj, XMMatrixTranspose(invViewProj));
	mainPassCB.EyePosW = DirectX::XMFLOAT3(eyePos.x, eyePos.y, eyePos.z);
	mainPassCB.RenderTargetSize = DirectX::XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mainPassCB.InvRenderTargetSize = DirectX::XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mainPassCB.NearZ = 1.0f;
	mainPassCB.FarZ = 5000.0f;
	mainPassCB.TotalTime = 0.0f;
	mainPassCB.DeltaTime = 0.0f;

	UploadBuffer<PerPassConstants>* passCB = mCurrFrameResource->passCB.get();
	passCB->copyData(0, mainPassCB);
}
