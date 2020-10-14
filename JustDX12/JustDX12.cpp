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
#include "PipelineStage\RenderPipelineStage.h"

#include <random>


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
	void BuildFrameResources();

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

	bool freezeCull = false;
	bool VRS = true;
	bool renderVRS = false;

	ComputePipelineStage* computeStage = nullptr;
	RenderPipelineStage* renderStage = nullptr;
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
	SSAOConstants ssaoConstantCB;

	DirectX::XMFLOAT4 eyePos = { 0.0f,70.0f,-10.0f, 1.0f };
	DirectX::XMFLOAT4X4 view = Identity();
	DirectX::XMFLOAT4X4 proj = Identity();

	float lookAngle[3] = { 0.0f, 0.0f, 0.0f };

	float mTheta = 1.3f * DirectX::XM_PI;
	float mPhi = 0.4f * DirectX::XM_PI;
	float mRadius = 10.0f;

	POINT lastMousePos;

	std::random_device rd;
	std::uniform_real_distribution<float> distro;
	std::ranlux24_base gen;
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
	gen = std::ranlux24_base(rd());
	distro = std::uniform_real_distribution<float>(-1.0, 1.0);

	for (int i = 0; i < 10; i++) {
		ssaoConstantCB.data.rand[i].x = distro(gen);
		ssaoConstantCB.data.rand[i].y = distro(gen);
		ssaoConstantCB.data.rand[i].z = abs(distro(gen)) + 0.2;
		ssaoConstantCB.data.rand[i].w = abs(distro(gen));
	}
}

DemoApp::~DemoApp() {
	if (md3dDevice != nullptr)
		FlushCommandQueue();
	delete modelLoader;
	delete computeStage;
	delete renderStage;
	for (int i = 0; i < objectsToRender.size(); i++) {
		delete objectsToRender[i];
	}
}

bool DemoApp::initialize() {
	if (!DX12App::initialize()) {
		return false;
	}

	{
		DescriptorJob perObjectConstants;
		perObjectConstants.name = "PerObjectConstDesc";
		perObjectConstants.target = "PerObjectConstants";
		perObjectConstants.type = DESCRIPTOR_TYPE_CBV;
		perObjectConstants.usage = DESCRIPTOR_USAGE_PER_OBJECT;
		perObjectConstants.usageIndex = 0;
		DescriptorJob perPassConstants;
		perPassConstants.name = "PerPassConstDesc";
		perPassConstants.target = "PerPassConstants";
		perPassConstants.type = DESCRIPTOR_TYPE_CBV;
		perPassConstants.usage = DESCRIPTOR_USAGE_PER_PASS;
		perPassConstants.usageIndex = 0;
		DescriptorJob rtvDescs[5];
		rtvDescs[0].name = "outTexDesc[0]";
		rtvDescs[0].target = "outTexArray[0]";
		rtvDescs[0].type = DESCRIPTOR_TYPE_RTV;
		rtvDescs[0].rtvDesc = DEFAULT_RTV_DESC();
		rtvDescs[0].usage = DESCRIPTOR_USAGE_PER_PASS;
		rtvDescs[0].usageIndex = 0;
		rtvDescs[1].name = "outTexDesc[1]";
		rtvDescs[1].target = "outTexArray[1]";
		rtvDescs[1].type = DESCRIPTOR_TYPE_RTV;
		rtvDescs[1].rtvDesc = DEFAULT_RTV_DESC();
		rtvDescs[1].usage = DESCRIPTOR_USAGE_PER_PASS;
		rtvDescs[1].usageIndex = 0;
		rtvDescs[2].name = "outTexDesc[2]";
		rtvDescs[2].target = "outTexArray[2]";
		rtvDescs[2].type = DESCRIPTOR_TYPE_RTV;
		rtvDescs[2].rtvDesc = DEFAULT_RTV_DESC();
		rtvDescs[2].usage = DESCRIPTOR_USAGE_PER_PASS;
		rtvDescs[2].usageIndex = 0;
		rtvDescs[3].name = "outTexDesc[3]";
		rtvDescs[3].target = "outTexArray[3]";
		rtvDescs[3].type = DESCRIPTOR_TYPE_RTV;
		rtvDescs[3].rtvDesc = DEFAULT_RTV_DESC();
		rtvDescs[3].usage = DESCRIPTOR_USAGE_PER_PASS;
		rtvDescs[3].usageIndex = 0;
		rtvDescs[4].name = "outTexDesc[4]";
		rtvDescs[4].target = "outTexArray[4]";
		rtvDescs[4].type = DESCRIPTOR_TYPE_RTV;
		rtvDescs[4].rtvDesc = DEFAULT_RTV_DESC();
		rtvDescs[4].usage = DESCRIPTOR_USAGE_PER_PASS;
		rtvDescs[4].usageIndex = 0;
		DescriptorJob dsvDesc;
		dsvDesc.name = "depthStencilView";
		dsvDesc.target = "depthTex";
		dsvDesc.type = DESCRIPTOR_TYPE_DSV;
		dsvDesc.dsvDesc = DEFAULT_DSV_DESC();
		dsvDesc.usage = DESCRIPTOR_USAGE_PER_PASS;
		dsvDesc.usageIndex = 0;
		RootParamDesc perObjRoot;
		perObjRoot.name = "PerObjectConstDesc";
		perObjRoot.numConstants = 1;
		perObjRoot.type = ROOT_PARAMETER_TYPE_CONSTANT_BUFFER;
		perObjRoot.rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		perObjRoot.usagePattern = DESCRIPTOR_USAGE_PER_OBJECT;
		perObjRoot.slot = 0;
		RootParamDesc perMeshTexRoot;
		perMeshTexRoot.name = "texture_diffuse";
		perMeshTexRoot.numConstants = 1;
		perMeshTexRoot.type = ROOT_PARAMETER_TYPE_SRV;
		perMeshTexRoot.rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		perMeshTexRoot.usagePattern = DESCRIPTOR_USAGE_PER_MESH;
		perMeshTexRoot.slot = 1;
		RootParamDesc perPassRoot;
		perPassRoot.name = "PerPassConstDesc";
		perPassRoot.numConstants = 1;
		perPassRoot.type = ROOT_PARAMETER_TYPE_CONSTANT_BUFFER;
		perPassRoot.rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		perPassRoot.usagePattern = DESCRIPTOR_USAGE_PER_PASS;
		perPassRoot.slot = 2;
		ResourceJob outTex;
		outTex.format = COLOR_TEXTURE_FORMAT;
		outTex.texHeight = SCREEN_HEIGHT;
		outTex.texWidth = SCREEN_WIDTH;
		outTex.types = DESCRIPTOR_TYPE_RTV | DESCRIPTOR_TYPE_SRV;
		ResourceJob outTexArray[5] = { outTex,outTex,outTex, outTex, outTex };
		outTexArray[0].name = "outTexArray[0]";
		outTexArray[1].name = "outTexArray[1]";
		outTexArray[2].name = "outTexArray[2]";
		outTexArray[3].name = "outTexArray[3]";
		outTexArray[4].name = "outTexArray[4]";
		ResourceJob depthTex;
		depthTex.format = DEPTH_TEXTURE_FORMAT;
		depthTex.name = "depthTex";
		depthTex.texHeight = SCREEN_HEIGHT;
		depthTex.texWidth = SCREEN_WIDTH;
		depthTex.types = DESCRIPTOR_TYPE_DSV | DESCRIPTOR_TYPE_SRV;
		ConstantBufferJob perObjectJob;
		perObjectJob.initialData = new PerObjectConstants();
		perObjectJob.name = "PerObjectConstants";
		ConstantBufferJob perPassJob;
		perPassJob.initialData = new PerPassConstants();
		perPassJob.name = "PerPassConstants";
		std::vector<DxcDefine> defines = { {L"VRS", NULL},
			{L"VRS_4X4", NULL } };
		ShaderDesc vs;
		vs.methodName = "VS";
		vs.shaderName = "Vertex Shader";
		vs.defines = defines;
		vs.type = SHADER_TYPE_VS;
		vs.fileName = "..\\Shaders\\Default.hlsl";
		ShaderDesc ps;
		ps.methodName = "PS";
		ps.defines = defines;
		ps.shaderName = "Pixel Shader";
		ps.type = SHADER_TYPE_PS;
		ps.fileName = "..\\Shaders\\Default.hlsl";
		RenderTargetDesc renderTargets[5];
		renderTargets[0].descriptorName = "outTexDesc[0]";
		renderTargets[0].slot = 0;
		renderTargets[1].descriptorName = "outTexDesc[1]";
		renderTargets[1].slot = 1;
		renderTargets[2].descriptorName = "outTexDesc[2]";
		renderTargets[2].slot = 2;
		renderTargets[3].descriptorName = "outTexDesc[3]";
		renderTargets[3].slot = 3;
		renderTargets[4].descriptorName = "outTexDesc[4]";
		renderTargets[4].slot = 4;

		PipeLineStageDesc rasterDesc;
		rasterDesc.constantBufferJobs = { perObjectJob, perPassJob };
		rasterDesc.descriptorJobs = { perObjectConstants, perPassConstants,
			rtvDescs[0], rtvDescs[1], rtvDescs[2], rtvDescs[3], rtvDescs[4], dsvDesc };
		rasterDesc.externalResources = {};
		rasterDesc.renderTargets = std::vector<RenderTargetDesc>(std::begin(renderTargets), std::end(renderTargets));
		rasterDesc.resourceJobs = { outTexArray[0],outTexArray[1],outTexArray[2],outTexArray[3],outTexArray[4],depthTex };
		rasterDesc.rootSigDesc = { perObjRoot, perMeshTexRoot, perPassRoot };
		rasterDesc.samplerDesc = {};
		rasterDesc.shaderFiles = { vs, ps };

		renderStage = new RenderPipelineStage(md3dDevice, DEFAULT_VIEW_PORT(), mScissorRect);
		renderStage->deferSetup(rasterDesc);
		WaitOnFenceForever(renderStage->getFence(), renderStage->triggerFence());
	}

	{
		DescriptorJob constantsDesc;
		constantsDesc.name = "SSAOConstantsDesc";
		constantsDesc.target = "SSAOConstants";
		constantsDesc.type = DESCRIPTOR_TYPE_CBV;
		constantsDesc.usage = DESCRIPTOR_USAGE_PER_PASS;
		constantsDesc.usageIndex = 0;
		DescriptorJob depthTex;
		depthTex.name = "inputDepth";
		depthTex.target = "renderOutputTex";
		depthTex.type = DESCRIPTOR_TYPE_SRV;
		depthTex.srvDesc = DEFAULT_SRV_DESC();
		depthTex.srvDesc.Format = DEPTH_TEXTURE_SRV_FORMAT;
		depthTex.usage = DESCRIPTOR_USAGE_PER_PASS;
		depthTex.usageIndex = 0;
		DescriptorJob normalTex;
		normalTex.name = "normalTex";
		normalTex.target = "renderOutputNormals";
		normalTex.type = DESCRIPTOR_TYPE_SRV;
		normalTex.srvDesc = DEFAULT_SRV_DESC();
		normalTex.usage = DESCRIPTOR_USAGE_PER_PASS;
		normalTex.usageIndex = 0;
		DescriptorJob colorTex = normalTex;
		colorTex.name = "colorTex";
		colorTex.target = "renderOutputColor";
		DescriptorJob tangentTex = normalTex;
		tangentTex.name = "tangentTex";
		tangentTex.target = "renderOutputTangents";
		DescriptorJob binormalTex = normalTex;
		binormalTex.name = "binormalTex";
		binormalTex.target = "renderOutputBinormals";
		DescriptorJob worldTex = normalTex;
		worldTex.name = "worldTex";
		worldTex.target = "renderOutputPosition";
		DescriptorJob outTexDesc;
		outTexDesc.name = "SSAOOut";
		outTexDesc.target = "SSAOOutTexture";
		outTexDesc.type = DESCRIPTOR_TYPE_UAV;
		outTexDesc.uavDesc = DEFAULT_UAV_DESC();
		outTexDesc.usage = DESCRIPTOR_USAGE_PER_PASS;
		outTexDesc.usageIndex = 0;
		RootParamDesc cbvPDesc;
		cbvPDesc.name = "SSAOConstantsDesc";
		cbvPDesc.type = ROOT_PARAMETER_TYPE_CONSTANT_BUFFER;
		cbvPDesc.numConstants = 1;
		cbvPDesc.rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		cbvPDesc.usagePattern = DESCRIPTOR_USAGE_PER_PASS;
		cbvPDesc.slot = 0;
		RootParamDesc rootPDesc;
		rootPDesc.name = "inputDepth";
		rootPDesc.type = ROOT_PARAMETER_TYPE_SRV;
		rootPDesc.numConstants = 1;
		rootPDesc.rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		rootPDesc.usagePattern = DESCRIPTOR_USAGE_PER_PASS;
		rootPDesc.slot = 1;
		RootParamDesc colorPDesc;
		colorPDesc.name = "colorTex";
		colorPDesc.type = ROOT_PARAMETER_TYPE_SRV;
		colorPDesc.numConstants = 1;
		colorPDesc.rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		colorPDesc.usagePattern = DESCRIPTOR_USAGE_PER_PASS;
		colorPDesc.slot = 2;
		RootParamDesc normalPDesc = colorPDesc;
		normalPDesc.name = "normalTex";
		normalPDesc.slot = 3;
		RootParamDesc tangentPDesc = normalPDesc;
		tangentPDesc.name = "tangentTex";
		tangentPDesc.slot = 4;
		RootParamDesc binormalPDesc = normalPDesc;
		binormalPDesc.name = "binormalTex";
		binormalPDesc.slot = 5;
		RootParamDesc worldPDesc = normalPDesc;
		worldPDesc.name = "worldTex";
		worldPDesc.slot = 6;
		RootParamDesc uavPDesc;
		uavPDesc.name = "SSAOOut";
		uavPDesc.type = ROOT_PARAMETER_TYPE_UAV;
		uavPDesc.numConstants = 1;
		uavPDesc.rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		uavPDesc.usagePattern = DESCRIPTOR_USAGE_PER_PASS;
		uavPDesc.slot = 7;
		ResourceJob outTex;
		outTex.name = "SSAOOutTexture";
		outTex.types = DESCRIPTOR_TYPE_UAV;
		SSAOConstants* initialData = new SSAOConstants();
		ConstantBufferJob cbOut;
		cbOut.initialData = initialData;
		cbOut.name = "SSAOConstants";
		ShaderDesc SSAOShaders;
		SSAOShaders.fileName = "..\\Shaders\\SSAO.hlsl";
		SSAOShaders.methodName = "SSAO";
		SSAOShaders.shaderName = "SSAO";
		SSAOShaders.type = SHADER_TYPE_CS;

		PipeLineStageDesc stageDesc;
		stageDesc.descriptorJobs = { {constantsDesc, depthTex, colorTex, normalTex, tangentTex, binormalTex, worldTex, outTexDesc} };
		stageDesc.rootSigDesc = { cbvPDesc, rootPDesc, colorPDesc, normalPDesc, tangentPDesc, binormalPDesc, worldPDesc, uavPDesc };
		stageDesc.samplerDesc = {};
		stageDesc.resourceJobs = { outTex };
		stageDesc.shaderFiles = { SSAOShaders };
		stageDesc.constantBufferJobs = { cbOut };
		stageDesc.externalResources = { 
			std::make_pair("renderOutputTex",renderStage->getResource("depthTex")),
			std::make_pair("renderOutputColor",renderStage->getResource("outTexArray[0]")),
			std::make_pair("renderOutputNormals",renderStage->getResource("outTexArray[1]")),
			std::make_pair("renderOutputTangents",renderStage->getResource("outTexArray[2]")),
			std::make_pair("renderOutputBinormals",renderStage->getResource("outTexArray[3]")),
			std::make_pair("renderOutputPosition",renderStage->getResource("outTexArray[4]"))
		};

		computeStage = new ComputePipelineStage(md3dDevice);
		computeStage->deferSetup(stageDesc);
	}
	modelLoader = new ModelLoader(md3dDevice);
	renderStage->LoadModel(modelLoader, sponzaFile, sponzaDir);
	
	mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr);

	BuildFrameResources();

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

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST));

	renderStage->deferExecute();
	int renderFenceValue = renderStage->triggerFence();
	computeStage->deferWaitOnFence(renderStage->getFence(), renderFenceValue);
	computeStage->deferExecute();
	int computeFenceValue = computeStage->triggerFence();

	//PIXScopedEvent(mCommandList.Get(), PIX_COLOR(0.0, 0.0, 1.0), "Copy and Show");

	WaitOnFenceForever(computeStage->getFence(), computeFenceValue);

	DX12Resource* SSAOOut = computeStage->getResource("SSAOOutTexture");
	SSAOOut->changeState(mCommandList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	mCommandList->CopyResource(CurrentBackBuffer(), SSAOOut->get());

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT));
	
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

void DemoApp::BuildFrameResources() {
	for (int i = 0; i < numFrameResources; i++) {
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			1, (UINT)/*Num of objects*/1, (UINT)/*Num of Materials*/0));
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
	if (GetAsyncKeyState(VK_LSHIFT) & 0x8000) {
		move.x = 4.0 * move.x;
		move.y = 4.0 * move.y;
		move.z = 4.0 * move.z;
	}
	renderStage->frustrumCull = true;
	if (GetAsyncKeyState('F') & 0x8000) {
		renderStage->frustrumCull = false;
	}
	freezeCull = false;
	if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
		freezeCull = true;
	}
	VRS = true;
	if (GetAsyncKeyState(VK_LSHIFT) & 0x8000) {
		VRS = false;
	}
	if (GetAsyncKeyState('T') & 0x8000) {
		renderVRS = !renderVRS;
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

	for (Model* model : objectsToRender) {
		PerObjectConstants objConst;

		renderStage->deferUpdateConstantBuffer("PerObjectConstants", objConst);
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
	
	XMStoreFloat4x4(&mainPassCB.data.view, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mainPassCB.data.invView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mainPassCB.data.proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mainPassCB.data.invProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mainPassCB.data.viewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mainPassCB.data.invViewProj, XMMatrixTranspose(invViewProj));
	mainPassCB.data.EyePosW = DirectX::XMFLOAT3(eyePos.x, eyePos.y, eyePos.z);
	mainPassCB.data.RenderTargetSize = DirectX::XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mainPassCB.data.InvRenderTargetSize = DirectX::XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mainPassCB.data.NearZ = 1.0f;
	mainPassCB.data.FarZ = 5000.0f;
	mainPassCB.data.TotalTime = 0.0f;
	mainPassCB.data.DeltaTime = 0.0f;
	mainPassCB.data.renderVRS = renderVRS;
	
	ssaoConstantCB.data.range = mainPassCB.data.FarZ / (mainPassCB.data.FarZ - mainPassCB.data.NearZ);
	ssaoConstantCB.data.rangeXnear = ssaoConstantCB.data.range * mainPassCB.data.NearZ;
	ssaoConstantCB.data.lightPos = DirectX::XMFLOAT4(sin(mCurrentFence / 250.0) * 600, 200.0, 0.0, 1.0);
	ssaoConstantCB.data.viewProj = mainPassCB.data.viewProj;

	computeStage->deferUpdateConstantBuffer("SSAOConstants", ssaoConstantCB);

	renderStage->deferUpdateConstantBuffer("PerPassConstants", mainPassCB);
	if (!freezeCull) {
		renderStage->frustrum = DirectX::BoundingFrustum(proj);
		renderStage->frustrum.Transform(renderStage->frustrum, invView);
	}
	renderStage->eyePos = DirectX::XMFLOAT3(eyePos.x, eyePos.y, eyePos.z);
	renderStage->VRS = VRS;
}
