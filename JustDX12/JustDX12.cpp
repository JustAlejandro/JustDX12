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

const int numFrameResources = 1;

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
	ComputePipelineStage* vrsComputeStage = nullptr;
	RenderPipelineStage* renderStage = nullptr;
	RenderPipelineStage* mergeStage = nullptr;
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
	MergeConstants mergeConstantCB;

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
	delete vrsComputeStage;
	for (int i = 0; i < objectsToRender.size(); i++) {
		delete objectsToRender[i];
	}
}

bool DemoApp::initialize() {
	if (!DX12App::initialize()) {
		return false;
	}

	{
		DescriptorJob perObjectConstants = { "PerObjectConstDesc", "PerObjectConstants", 
			DESCRIPTOR_TYPE_CBV, {}, 0, DESCRIPTOR_USAGE_PER_OBJECT };
		DescriptorJob perPassConstants = { "PerPassConstDesc", "PerPassConstants",
			DESCRIPTOR_TYPE_CBV, {}, 0, DESCRIPTOR_USAGE_PER_PASS };

		DescriptorJob rtvDescs[5];
		rtvDescs[0].name = "outTexDesc[0]";
		rtvDescs[0].target = "outTexArray[0]";
		rtvDescs[0].type = DESCRIPTOR_TYPE_RTV;
		rtvDescs[0].rtvDesc = DEFAULT_RTV_DESC();
		rtvDescs[0].usage = DESCRIPTOR_USAGE_PER_PASS;
		rtvDescs[0].usageIndex = 0;
		rtvDescs[1] = rtvDescs[0];
		rtvDescs[1].name = "outTexDesc[1]";
		rtvDescs[1].target = "outTexArray[1]";
		rtvDescs[2] = rtvDescs[0];
		rtvDescs[2].name = "outTexDesc[2]";
		rtvDescs[2].target = "outTexArray[2]";
		rtvDescs[3] = rtvDescs[0];
		rtvDescs[3].name = "outTexDesc[3]";
		rtvDescs[3].target = "outTexArray[3]";
		rtvDescs[4] = rtvDescs[0];
		rtvDescs[4].name = "outTexDesc[4]";
		rtvDescs[4].target = "outTexArray[4]";
		DescriptorJob dsvDesc;
		dsvDesc.name = "depthStencilView";
		dsvDesc.target = "depthTex";
		dsvDesc.type = DESCRIPTOR_TYPE_DSV;
		dsvDesc.dsvDesc = DEFAULT_DSV_DESC();
		dsvDesc.usage = DESCRIPTOR_USAGE_PER_PASS;
		dsvDesc.usageIndex = 0;
		RootParamDesc perObjRoot = { "PerObjectConstDesc", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER, 
			0, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, DESCRIPTOR_USAGE_PER_OBJECT };
		RootParamDesc perMeshTexRoot = { "texture_diffuse", ROOT_PARAMETER_TYPE_SRV,
			1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, DESCRIPTOR_USAGE_PER_MESH };
		RootParamDesc perPassRoot = { "PerPassConstDesc", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER,
			2, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, DESCRIPTOR_USAGE_PER_PASS };
		ResourceJob vrsTex = { "VRS", DESCRIPTOR_TYPE_UAV, DXGI_FORMAT_R8_UINT,
			(SCREEN_HEIGHT+vrsSupport.ShadingRateImageTileSize-1) / vrsSupport.ShadingRateImageTileSize,
			(SCREEN_WIDTH+vrsSupport.ShadingRateImageTileSize-1) / vrsSupport.ShadingRateImageTileSize };
		ResourceJob outTex = { "example", DESCRIPTOR_TYPE_RTV | DESCRIPTOR_TYPE_SRV,
			COLOR_TEXTURE_FORMAT, SCREEN_HEIGHT, SCREEN_WIDTH };
		ResourceJob outTexArray[5] = { outTex,outTex,outTex, outTex, outTex };
		// Attachment 0 (color) is used by the SSAO to write the final output (not great)
		// and VRS compute, so it needs a simul access flag.
		outTexArray[0].name = "outTexArray[0]";
		outTexArray[0].types |= DESCRIPTOR_TYPE_FLAG_SIMULTANEOUS_ACCESS;
		outTexArray[1].name = "outTexArray[1]";
		outTexArray[2].name = "outTexArray[2]";
		outTexArray[3].name = "outTexArray[3]";
		outTexArray[4].name = "outTexArray[4]";
		ResourceJob depthTex = { "depthTex", DESCRIPTOR_TYPE_DSV | DESCRIPTOR_TYPE_SRV,
			DEPTH_TEXTURE_FORMAT, SCREEN_HEIGHT, SCREEN_WIDTH };
		ConstantBufferJob perObjectJob = { "PerObjectConstants", new PerObjectConstants() };
		ConstantBufferJob perPassJob = { "PerPassConstants", new PerPassConstants() };
		std::vector<DXDefine> defines = { 
			{L"VRS", L""},
			{L"VRS_4X4", L""} };
		ShaderDesc vs = { "Default.hlsl", "Vertex Shader", "VS", SHADER_TYPE_VS, defines };
		ShaderDesc ps = { "Default.hlsl", "Pixel Shader", "PS", SHADER_TYPE_PS, defines };
		RenderTargetDesc renderTargets[5];
		renderTargets[0] = { "outTexDesc[0]", 0 };
		renderTargets[1] = { "outTexDesc[1]", 0 };
		renderTargets[2] = { "outTexDesc[2]", 0 };
		renderTargets[3] = { "outTexDesc[3]", 0 };
		renderTargets[4] = { "outTexDesc[4]", 0 };

		PipeLineStageDesc rasterDesc;
		rasterDesc.constantBufferJobs = { perObjectJob, perPassJob };
		rasterDesc.descriptorJobs = { perObjectConstants, perPassConstants,
			rtvDescs[0], rtvDescs[1], rtvDescs[2], rtvDescs[3], rtvDescs[4], dsvDesc };
		rasterDesc.externalResources = {};
		rasterDesc.renderTargets = std::vector<RenderTargetDesc>(std::begin(renderTargets), std::end(renderTargets));
		rasterDesc.resourceJobs = { outTexArray[0],outTexArray[1],outTexArray[2],outTexArray[3],outTexArray[4],depthTex,vrsTex };
		rasterDesc.rootSigDesc = { perObjRoot, perMeshTexRoot, perPassRoot };
		rasterDesc.samplerDesc = {};
		rasterDesc.shaderFiles = { vs, ps };

		RenderPipelineDesc rDesc;
		renderStage = new RenderPipelineStage(md3dDevice, rDesc, DEFAULT_VIEW_PORT(), mScissorRect);
		renderStage->deferSetup(rasterDesc);
		WaitOnFenceForever(renderStage->getFence(), renderStage->triggerFence());
	}

	{
		DescriptorJob constantsDesc = { "SSAOConstantsDesc", "SSAOConstants", DESCRIPTOR_TYPE_CBV, 
			{}, 0, DESCRIPTOR_USAGE_PER_PASS };
		DescriptorJob depthTex = { "inputDepth", "renderOutputTex", DESCRIPTOR_TYPE_SRV,
			DEFAULT_SRV_DESC(), 0, DESCRIPTOR_USAGE_PER_PASS };
		depthTex.srvDesc.Format = DEPTH_TEXTURE_SRV_FORMAT;
		DescriptorJob normalTex = { "normalTex", "renderOutputNormals", DESCRIPTOR_TYPE_SRV,
			DEFAULT_SRV_DESC(), 0, DESCRIPTOR_USAGE_PER_PASS };
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
		outTexDesc.uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
		outTexDesc.usage = DESCRIPTOR_USAGE_PER_PASS;
		outTexDesc.usageIndex = 0;
		RootParamDesc cbvPDesc = { "SSAOConstantsDesc", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER,
			0, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, DESCRIPTOR_USAGE_PER_PASS };
		RootParamDesc rootPDesc = { "inputDepth", ROOT_PARAMETER_TYPE_SRV,
			1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, DESCRIPTOR_USAGE_PER_PASS };
		RootParamDesc colorPDesc = { "colorTex", ROOT_PARAMETER_TYPE_SRV,
			2, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, DESCRIPTOR_USAGE_PER_PASS };
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
		RootParamDesc uavPDesc = { "SSAOOut", ROOT_PARAMETER_TYPE_UAV,
			7, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, DESCRIPTOR_USAGE_PER_PASS };
		ResourceJob outTex;
		outTex.name = "SSAOOutTexture";
		outTex.types = DESCRIPTOR_TYPE_UAV;
		outTex.format = DXGI_FORMAT_R32_FLOAT;
		ConstantBufferJob cbOut = { "SSAOConstants", new SSAOConstants() };
		ShaderDesc SSAOShaders = { "SSAO.hlsl", "SSAO", "SSAO", SHADER_TYPE_CS, {} };

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
		ComputePipelineDesc cDesc;
		cDesc.groupCount[0] = (UINT)ceilf(SCREEN_WIDTH / 8.0f);
		cDesc.groupCount[1] = (UINT)ceilf(SCREEN_HEIGHT / 8.0f);
		cDesc.groupCount[2] = 1;
		computeStage = new ComputePipelineStage(md3dDevice, cDesc);
		computeStage->deferSetup(stageDesc);
		WaitOnFenceForever(computeStage->getFence(), computeStage->triggerFence());
	}

	{
		DescriptorJob perPassConstants = { "MergeConstDesc", "MergeConstants",
			DESCRIPTOR_TYPE_CBV, {}, 0, DESCRIPTOR_USAGE_PER_PASS };
		DescriptorJob outDepthTex = { "outputDepth", "outputDepthTex" , DESCRIPTOR_TYPE_DSV,
			{}, 0, DESCRIPTOR_USAGE_ALL };
		outDepthTex.dsvDesc = DEFAULT_DSV_DESC();
		DescriptorJob ssaoTex;
		ssaoTex.name = "SSAOTexDesc";
		ssaoTex.target = "SSAOTex";
		ssaoTex.srvDesc = DEFAULT_SRV_DESC();
		ssaoTex.srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		ssaoTex.type = DESCRIPTOR_TYPE_SRV;
		DescriptorJob colorTex;
		colorTex.name = "colorTexDesc";
		colorTex.target = "colorTex";
		colorTex.srvDesc = DEFAULT_SRV_DESC();
		colorTex.type = DESCRIPTOR_TYPE_SRV;
		DescriptorJob normalTex = colorTex;
		normalTex.name = "normalTexDesc";
		normalTex.target = "normalTex";
		DescriptorJob tangentTex = colorTex;
		tangentTex.name = "tangentTexDesc";
		tangentTex.target = "tangentTex";
		DescriptorJob biNormalTex = colorTex;
		biNormalTex.name = "biNormalTexDesc";
		biNormalTex.target = "biNormalTex";
		DescriptorJob worldTex = colorTex;
		worldTex.name = "worldTexDesc";
		worldTex.target = "worldTex";
		DescriptorJob mergedTex;
		mergedTex.name = "mergedTexDesc";
		mergedTex.target = "mergedTex";
		mergedTex.type = DESCRIPTOR_TYPE_RTV;
		mergedTex.rtvDesc = DEFAULT_RTV_DESC();
		std::vector<RootParamDesc> params;
		params.push_back({ "MergeConstDesc", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER,
			0, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, DESCRIPTOR_USAGE_ALL });
		// Trying to bind all textures at once to be efficient.
		params.push_back({ "SSAOTexDesc", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
			1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6, DESCRIPTOR_USAGE_ALL });
		RenderTargetDesc mergedTarget = { "mergedTexDesc", 0 };
		ConstantBufferJob mergedCB = { "MergeConstants", new MergeConstants() };
		ResourceJob mergedTexRes = { "mergedTex", DESCRIPTOR_TYPE_RTV };
		ResourceJob outDepthTexRes = { "outputDepthTex", DESCRIPTOR_TYPE_DSV, DEPTH_TEXTURE_FORMAT };
		std::vector<DXDefine> defines = {
			{L"MAX_LIGHTS", std::to_wstring(MAX_LIGHTS)}
		};
		ShaderDesc mergeShaderVS = { "Merge.hlsl", "Merge Shader VS", "MergeVS", SHADER_TYPE_VS, defines };
		ShaderDesc mergeShaderPS = { "Merge.hlsl", "Merge Shader PS", "MergePS", SHADER_TYPE_PS, defines };

		PipeLineStageDesc stageDesc;
		stageDesc.descriptorJobs = { perPassConstants, outDepthTex, ssaoTex, colorTex, 
			normalTex, tangentTex, biNormalTex, worldTex, mergedTex };
		stageDesc.constantBufferJobs = { mergedCB };
		stageDesc.renderTargets = { mergedTarget };
		stageDesc.resourceJobs = { mergedTexRes, outDepthTexRes };
		stageDesc.rootSigDesc = params;
		stageDesc.shaderFiles = { mergeShaderVS, mergeShaderPS };
		stageDesc.externalResources = {
			{"SSAOTex", computeStage->getResource("SSAOOutTexture")},
			{"colorTex", renderStage->getResource("outTexArray[0]")},
			{"normalTex", renderStage->getResource("outTexArray[1]")},
			{"tangentTex", renderStage->getResource("outTexArray[2]")},
			{"biNormalTex", renderStage->getResource("outTexArray[3]")},
			{"worldTex", renderStage->getResource("outTexArray[4]")},
			{"VRS", renderStage->getResource("VRS")} };

		RenderPipelineDesc mergeRDesc;
		mergeStage = new RenderPipelineStage(md3dDevice, mergeRDesc, DEFAULT_VIEW_PORT(), mScissorRect);
		mergeStage->deferSetup(stageDesc);
		WaitOnFenceForever(mergeStage->getFence(), mergeStage->triggerFence());
		mergeStage->frustrumCull = false;
	}

	{
		DescriptorJob depthTex;
		depthTex.name = "inputDepth";
		depthTex.target = "renderOutputTex";
		depthTex.type = DESCRIPTOR_TYPE_SRV;
		depthTex.srvDesc = DEFAULT_SRV_DESC();
		depthTex.usage = DESCRIPTOR_USAGE_PER_PASS;
		depthTex.usageIndex = 0;
		DescriptorJob outTexDesc;
		outTexDesc.name = "VrsOut";
		outTexDesc.target = "VrsOutTexture";
		outTexDesc.type = DESCRIPTOR_TYPE_UAV;
		outTexDesc.uavDesc = DEFAULT_UAV_DESC();
		outTexDesc.uavDesc.Format = DXGI_FORMAT_R8_UINT;
		outTexDesc.usage = DESCRIPTOR_USAGE_PER_PASS;
		outTexDesc.usageIndex = 0;
		RootParamDesc rootPDesc;
		rootPDesc.name = "inputDepth";
		rootPDesc.type = ROOT_PARAMETER_TYPE_SRV;
		rootPDesc.numConstants = 1;
		rootPDesc.rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		rootPDesc.usagePattern = DESCRIPTOR_USAGE_PER_PASS;
		rootPDesc.slot = 0;
		RootParamDesc uavPDesc;
		uavPDesc.name = "VrsOut";
		uavPDesc.type = ROOT_PARAMETER_TYPE_UAV;
		uavPDesc.numConstants = 1;
		uavPDesc.rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		uavPDesc.usagePattern = DESCRIPTOR_USAGE_PER_PASS;
		uavPDesc.slot = 1;
		std::vector<DXDefine> defines = {
			{L"N", L"8" },
			{L"TILE_SIZE", std::to_wstring(vrsSupport.ShadingRateImageTileSize)},
			{L"EXTRA_SAMPLES", L"0"} };
		ShaderDesc VrsShader = { "..\\Shaders\\VRSCompute.hlsl", "VRS Compute", 
			"VRSOut", SHADER_TYPE_CS, defines };
		
		PipeLineStageDesc stageDesc;
		stageDesc.descriptorJobs = { depthTex, outTexDesc };
		stageDesc.rootSigDesc = { rootPDesc, uavPDesc };
		stageDesc.shaderFiles = { VrsShader };
		stageDesc.externalResources = {
			std::make_pair("renderOutputTex", mergeStage->getResource("mergedTex")),
			std::make_pair("VrsOutTexture", renderStage->getResource("VRS"))
		};
		ComputePipelineDesc cDesc;
		cDesc.groupCount[0] = (SCREEN_WIDTH + (vrsSupport.ShadingRateImageTileSize*8) - 1) / (vrsSupport.ShadingRateImageTileSize * 8);
		cDesc.groupCount[1] = (SCREEN_HEIGHT + (vrsSupport.ShadingRateImageTileSize*8) - 1) / (vrsSupport.ShadingRateImageTileSize * 8);
		cDesc.groupCount[2] = 1;
		
		vrsComputeStage = new ComputePipelineStage(md3dDevice, cDesc);
		vrsComputeStage->deferSetup(stageDesc);
	}
	modelLoader = new ModelLoader(md3dDevice);
	mergeStage->LoadModel(modelLoader, "screenTex.obj", baseDir);
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
	mergeStage->deferWaitOnFence(computeStage->getFence(), computeFenceValue);
	mergeStage->deferExecute();
	int mergeFenceValue = mergeStage->triggerFence();
	vrsComputeStage->deferWaitOnFence(mergeStage->getFence(), mergeFenceValue);
	vrsComputeStage->deferExecute();
	int vrsComputeFenceValue = vrsComputeStage->triggerFence();

	//PIXScopedEvent(mCommandList.Get(), PIX_COLOR(0.0, 0.0, 1.0), "Copy and Show");

	//WaitOnFenceForever(computeStage->getFence(), computeFenceValue);
	WaitOnFenceForever(vrsComputeStage->getFence(), vrsComputeFenceValue);

	DX12Resource* megeOut = mergeStage->getResource("mergedTex");
	megeOut->changeState(mCommandList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	mCommandList->CopyResource(CurrentBackBuffer(), megeOut->get());

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
	ssaoConstantCB.data.lightPos = DirectX::XMFLOAT4(sin(mCurrentFence / 250.0) * 600, 100.0, 0.0, 1.0);
	ssaoConstantCB.data.viewProj = mainPassCB.data.viewProj;

	computeStage->deferUpdateConstantBuffer("SSAOConstants", ssaoConstantCB);

	mergeConstantCB.data.numPointLights = 1;
	mergeConstantCB.data.lights[0].strength = 500.0;
	mergeConstantCB.data.lights[0].pos = DirectX::XMFLOAT3(sin(mCurrentFence / 250.0) * 600, 100.0, 0.0);

	mergeStage->deferUpdateConstantBuffer("MergeConstants", mergeConstantCB);
	renderStage->deferUpdateConstantBuffer("PerPassConstants", mainPassCB);
	if (!freezeCull) {
		renderStage->frustrum = DirectX::BoundingFrustum(proj);
		renderStage->frustrum.Transform(renderStage->frustrum, invView);
	}
	renderStage->eyePos = DirectX::XMFLOAT3(eyePos.x, eyePos.y, eyePos.z);
	renderStage->VRS = VRS;
	mergeStage->VRS = VRS;
}
