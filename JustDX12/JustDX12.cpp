#include <iostream>
#include <chrono>
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
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include "KeyboardWrapper.h"

#include <random>
#include <ctime>


std::string baseDir = "..\\Models";
std::string inputfile = "teapot.obj";
std::string sponzaDir = baseDir + "\\sponza";
std::string sponzaFile = "sponza.fbx";
std::string armorDir = baseDir + "\\parade_armor";
std::string armorFile = "armor2.fbx";
std::string headDir = baseDir + "\\head";
std::string headFile = "head.fbx";
std::string headSmallFile = "headSmall.fbx";
std::string headSmallMeshlet = "headSmall.bin";
std::string armorMeshlet = "armor.bin";
std::string headMeshlet = "headReduce.bin";

std::string warn;
std::string err;


class DemoApp : public DX12App {
public:
	DemoApp(HINSTANCE hInstance);
	DemoApp(const DemoApp& rhs) = delete;
	DemoApp& operator=(const DemoApp& rhs) = delete;
	~DemoApp();

	virtual bool initialize()override;

	virtual void update()override;

	virtual void draw()override;

	void ImGuiPrepareUI();

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

	bool showImgui = true;
	bool freezeCull = false;
	bool VRS = true;
	bool renderVRS = false;
	bool occlusionCull = true;
	std::deque<float> frametime;
	std::deque<float> cpuFrametime;

	ComputePipelineStage* computeStage = nullptr;
	ComputePipelineStage* vrsComputeStage = nullptr;
	RenderPipelineStage* renderStage = nullptr;
	RenderPipelineStage* deferStage = nullptr;
	RenderPipelineStage* mergeStage = nullptr;
	ModelLoader* modelLoader = nullptr;
	TextureLoader* textureLoader = nullptr;
	KeyboardWrapper keyboard;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mSSAORootSignature = nullptr;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> shaders;
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> defaultPSO = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> ssaoPSO = nullptr;

	PerPassConstants mainPassCB;
	VrsConstants vrsCB;
	SSAOConstants ssaoConstantCB;
	MergeConstants mergeConstantCB;

	DirectX::XMFLOAT4 eyePos = { 0.0f,70.0f,40.0f, 1.0f };
	DirectX::XMFLOAT4X4 view = Identity();
	DirectX::XMFLOAT4X4 proj = Identity();

	float lookAngle[3] = { 0.0f, DirectX::XM_PI, 0.0f };

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
	delete renderStage;
	delete deferStage;
	delete mergeStage;
	delete vrsComputeStage;
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

bool DemoApp::initialize() {
	if (!DX12App::initialize()) {
		return false;
	}

	std::vector<HANDLE> cpuWaitHandles;
	// Create Render stage first because of dependencies later.
	{
		DescriptorJob rtvDescs[6];
		rtvDescs[0].name = "outTexDesc[0]";
		rtvDescs[0].indirectTarget = "outTexArray[0]";
		rtvDescs[0].type = DESCRIPTOR_TYPE_RTV;
		rtvDescs[0].view.rtvDesc = DEFAULT_RTV_DESC();
		rtvDescs[0].usage = DESCRIPTOR_USAGE_PER_PASS;
		rtvDescs[0].usageIndex = 0;
		rtvDescs[1] = rtvDescs[0];
		rtvDescs[1].name = "outTexDesc[1]";
		rtvDescs[1].indirectTarget = "outTexArray[1]";
		rtvDescs[2] = rtvDescs[0];
		rtvDescs[2].name = "outTexDesc[2]";
		rtvDescs[2].indirectTarget = "outTexArray[2]";
		rtvDescs[3] = rtvDescs[0];
		rtvDescs[3].name = "outTexDesc[3]";
		rtvDescs[3].indirectTarget = "outTexArray[3]";
		rtvDescs[4] = rtvDescs[0];
		rtvDescs[4].name = "outTexDesc[4]";
		rtvDescs[4].indirectTarget = "outTexArray[4]";
		rtvDescs[5] = rtvDescs[0];
		rtvDescs[5].name = "outTexDesc[5]";
		rtvDescs[5].indirectTarget = "outTexArray[5]";
		DescriptorJob dsvDesc;
		dsvDesc.name = "depthStencilView";
		dsvDesc.indirectTarget = "depthTex";
		dsvDesc.type = DESCRIPTOR_TYPE_DSV;
		dsvDesc.view.dsvDesc = DEFAULT_DSV_DESC();
		dsvDesc.usage = DESCRIPTOR_USAGE_PER_PASS;
		dsvDesc.usageIndex = 0;
		RootParamDesc perObjRoot = { "PerObjectConstants", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER,
			0, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, DESCRIPTOR_USAGE_PER_OBJECT };
		RootParamDesc perMeshTexRoot = { "texture_diffuse", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
			1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, DESCRIPTOR_USAGE_PER_MESH };
		RootParamDesc perPassRoot = { "PerPassConstants", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER,
			2, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, DESCRIPTOR_USAGE_PER_PASS };
		ResourceJob vrsTex = { "VRS", DESCRIPTOR_TYPE_UAV, DXGI_FORMAT_R8_UINT,
			(SCREEN_HEIGHT+vrsSupport.ShadingRateImageTileSize-1) / vrsSupport.ShadingRateImageTileSize,
			(SCREEN_WIDTH+vrsSupport.ShadingRateImageTileSize-1) / vrsSupport.ShadingRateImageTileSize };
		ResourceJob outTex = { "example", DESCRIPTOR_TYPE_RTV | DESCRIPTOR_TYPE_SRV,
			COLOR_TEXTURE_FORMAT, SCREEN_HEIGHT, SCREEN_WIDTH };
		ResourceJob outTexArray[6] = { outTex,outTex,outTex, outTex, outTex, outTex };
		// Attachment 0 (color) is used by the SSAO to write the final output (not great)
		// and VRS compute, so it needs a simul access flag.
		outTexArray[0].name = "outTexArray[0]";
		outTexArray[0].types |= DESCRIPTOR_TYPE_FLAG_SIMULTANEOUS_ACCESS;
		outTexArray[1].name = "outTexArray[1]";
		outTexArray[2].name = "outTexArray[2]";
		outTexArray[3].name = "outTexArray[3]";
		outTexArray[4].name = "outTexArray[4]";
		outTexArray[5].name = "outTexArray[5]";
		ResourceJob depthTex = { "depthTex", DESCRIPTOR_TYPE_DSV | DESCRIPTOR_TYPE_SRV,
			DEPTH_TEXTURE_FORMAT, SCREEN_HEIGHT, SCREEN_WIDTH };
		ConstantBufferJob perObjectJob0 = { "PerObjectConstants", new PerObjectConstants(), 0 };
		ConstantBufferJob perObjectJob1 = { "PerObjectConstants", new PerObjectConstants(), 1 };
		ConstantBufferJob perObjectJob2 = { "PerObjectConstants", new PerObjectConstants(), 2 };
		ConstantBufferJob perObjectJob3 = { "PerObjectConstants", new PerObjectConstants(), 3 };
		ConstantBufferJob perObjectJob4 = { "PerObjectConstants", new PerObjectConstants(), 4 };
		ConstantBufferJob perObjectJob5 = { "PerObjectConstants", new PerObjectConstants(), 5 };
		ConstantBufferJob perObjectJobMeshlet0 = { "PerObjectConstantsMeshlet", new PerObjectConstants(), 0 };
		ConstantBufferJob perPassJob = { "PerPassConstants", new PerPassConstants() };
		std::vector<DXDefine> defines = { 
			{L"VRS", L""},
			{L"VRS_4X4", L""} };
		ShaderDesc vs = { "Default.hlsl", "Vertex Shader", "VS", SHADER_TYPE_VS, defines };
		ShaderDesc ps = { "Default.hlsl", "Pixel Shader", "PS", SHADER_TYPE_PS, defines };
		ShaderDesc as = { "MeshletAS.hlsl", "Amplification Shader", "AS", SHADER_TYPE_AS, {} };
		ShaderDesc ms = { "MeshletMS.hlsl", "Mesh Shader", "MS", SHADER_TYPE_MS, {} };
		ShaderDesc psMesh = { "MeshletMS.hlsl", "Mesh Pixel Shader", "PS", SHADER_TYPE_PS, {} };
		RenderTargetDesc renderTargets[6];
		renderTargets[0] = { "outTexDesc[0]", 0 };
		renderTargets[1] = { "outTexDesc[1]", 0 };
		renderTargets[2] = { "outTexDesc[2]", 0 };
		renderTargets[3] = { "outTexDesc[3]", 0 };
		renderTargets[4] = { "outTexDesc[4]", 0 };
		renderTargets[5] = { "outTexDesc[5]", 0 };


		PipeLineStageDesc rasterDesc;
		rasterDesc.constantBufferJobs = { perObjectJob0, perObjectJob1, perObjectJob2, perObjectJob3, perObjectJob4, perObjectJob5, perObjectJobMeshlet0, perPassJob };
		rasterDesc.descriptorJobs = { rtvDescs[0], rtvDescs[1], rtvDescs[2], rtvDescs[3], rtvDescs[4], rtvDescs[5], dsvDesc };
		rasterDesc.externalResources = {};
		rasterDesc.renderTargets = std::vector<RenderTargetDesc>(std::begin(renderTargets), std::end(renderTargets));
		rasterDesc.resourceJobs = { outTexArray[0],outTexArray[1],outTexArray[2],outTexArray[3],outTexArray[4],outTexArray[5],depthTex,vrsTex };
		rasterDesc.rootSigDesc = { perObjRoot, perMeshTexRoot, perPassRoot };
		rasterDesc.shaderFiles = { vs, psMesh, ps, as, ms };
		rasterDesc.textureFiles = {
			{"default_normal", "default_bump.dds"},
			{"default_spec", "default_spec.dds"},
			{"default_diff", "default_diff.dds"},
			{"default_alpha", "default_alpha.dds"}
		};


		RenderPipelineDesc rDesc;
		rDesc.supportsCulling = true;
		rDesc.usesMeshlets = true;
		// Have to generate meshlet root signature
		std::vector<RootParamDesc> meshParams;
		meshParams.push_back({ "MeshInfo", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER,
			0, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, DESCRIPTOR_USAGE_PER_MESHLET });
		meshParams.push_back({ "Vertices", ROOT_PARAMETER_TYPE_SRV,
			1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, DESCRIPTOR_USAGE_PER_MESHLET });
		meshParams.push_back({ "Meshlets", ROOT_PARAMETER_TYPE_SRV,
			2, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, DESCRIPTOR_USAGE_PER_MESHLET });
		meshParams.push_back({ "UniqueVertexIndices", ROOT_PARAMETER_TYPE_SRV,
			3, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, DESCRIPTOR_USAGE_PER_MESHLET });
		meshParams.push_back({ "PrimitiveIndices", ROOT_PARAMETER_TYPE_SRV,
			4, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, DESCRIPTOR_USAGE_PER_MESHLET });
		meshParams.push_back({ "MeshletCullData", ROOT_PARAMETER_TYPE_SRV,
			5, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, DESCRIPTOR_USAGE_PER_MESHLET });
		perObjRoot.name = "PerObjectConstantsMeshlet";
		perObjRoot.slot += 6;
		perMeshTexRoot.slot += 6;
		perMeshTexRoot.name = "mesh_texture_diffuse";
		perPassRoot.slot += 6;
		meshParams.push_back(perObjRoot);
		meshParams.push_back(perMeshTexRoot);
		meshParams.push_back(perPassRoot);
		rDesc.meshletRootSignature = meshParams;
		rDesc.textureToDescriptor.emplace_back(MODEL_FORMAT_DIFFUSE_TEX, "texture_diffuse");
		rDesc.textureToDescriptor.emplace_back(MODEL_FORMAT_SPECULAR_TEX, "texture_spec");
		rDesc.textureToDescriptor.emplace_back(MODEL_FORMAT_NORMAL_TEX, "texture_normal");
		rDesc.textureToDescriptor.emplace_back(MODEL_FORMAT_OPACITY_TEX, "texture_alpha");
		rDesc.defaultTextures[MODEL_FORMAT_DIFFUSE_TEX] = "default_diff";
		rDesc.defaultTextures[MODEL_FORMAT_SPECULAR_TEX] = "default_spec";
		rDesc.defaultTextures[MODEL_FORMAT_NORMAL_TEX] = "default_normal";
		rDesc.defaultTextures[MODEL_FORMAT_OPACITY_TEX] = "default_alpha";
		rDesc.meshletTextureToDescriptor.emplace_back(MODEL_FORMAT_DIFFUSE_TEX, "mesh_texture_diffuse");
		rDesc.meshletTextureToDescriptor.emplace_back(MODEL_FORMAT_SPECULAR_TEX, "mesh_texture_specular");
		rDesc.meshletTextureToDescriptor.emplace_back(MODEL_FORMAT_NORMAL_TEX, "mesh_texture_normal");
		rDesc.meshletTextureToDescriptor.emplace_back(MODEL_FORMAT_OPACITY_TEX, "mesh_texture_alpha");
		rDesc.supportsVRS = true;

		renderStage = new RenderPipelineStage(md3dDevice, rDesc, DEFAULT_VIEW_PORT(), mScissorRect);
		renderStage->deferSetup(rasterDesc);
		WaitOnFenceForever(renderStage->getFence(), renderStage->triggerFence());
	}
	cpuWaitHandles.push_back(renderStage->deferSetCpuEvent());
	// Create SSAO/Screen Space Shadow Pass.
	{
		DescriptorJob noiseTex("noise_texDesc", "noise_tex", DESCRIPTOR_TYPE_SRV, true);
		DescriptorJob depthTex("inputDepth", "renderOutputTex", DESCRIPTOR_TYPE_SRV, false, 0, DESCRIPTOR_USAGE_PER_PASS);
		depthTex.view.srvDesc = DEFAULT_SRV_DESC();
		depthTex.view.srvDesc.Format = DEPTH_TEXTURE_SRV_FORMAT;
		DescriptorJob normalTex("normalTex", "renderOutputNormals", DESCRIPTOR_TYPE_SRV, true, 0, DESCRIPTOR_USAGE_PER_PASS);
		DescriptorJob colorTex = normalTex;
		colorTex.name = "colorTex";
		colorTex.indirectTarget = "renderOutputColor";
		DescriptorJob tangentTex = normalTex;
		tangentTex.name = "tangentTex";
		tangentTex.indirectTarget = "renderOutputTangents";
		DescriptorJob binormalTex = normalTex;
		binormalTex.name = "binormalTex";
		binormalTex.indirectTarget = "renderOutputBinormals";
		DescriptorJob worldTex = normalTex;
		worldTex.name = "worldTex";
		worldTex.indirectTarget = "renderOutputPosition";
		DescriptorJob outTexDesc;
		outTexDesc.name = "SSAOOut";
		outTexDesc.indirectTarget = "SSAOOutTexture";
		outTexDesc.type = DESCRIPTOR_TYPE_UAV;
		outTexDesc.view.uavDesc = DEFAULT_UAV_DESC();
		outTexDesc.view.uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
		outTexDesc.usage = DESCRIPTOR_USAGE_PER_PASS;
		outTexDesc.usageIndex = 0;
		RootParamDesc cbvPDesc = { "SSAOConstants", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER,
			0, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, DESCRIPTOR_USAGE_PER_PASS };
		RootParamDesc noisePDesc = { "noise_texDesc", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
			1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, DESCRIPTOR_USAGE_PER_PASS };
		RootParamDesc rootPDesc = { "inputDepth", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
			2, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, DESCRIPTOR_USAGE_PER_PASS };
		RootParamDesc colorPDesc = { "colorTex", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
			3, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, DESCRIPTOR_USAGE_PER_PASS };
		RootParamDesc normalPDesc = colorPDesc;
		normalPDesc.name = "normalTex";
		normalPDesc.slot = 4;
		RootParamDesc tangentPDesc = normalPDesc;
		tangentPDesc.name = "tangentTex";
		tangentPDesc.slot = 5;
		RootParamDesc binormalPDesc = normalPDesc;
		binormalPDesc.name = "binormalTex";
		binormalPDesc.slot = 6;
		RootParamDesc worldPDesc = normalPDesc;
		worldPDesc.name = "worldTex";
		worldPDesc.slot = 7;
		RootParamDesc uavPDesc = { "SSAOOut", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
			8, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, DESCRIPTOR_USAGE_PER_PASS };
		ResourceJob outTex;
		outTex.name = "SSAOOutTexture";
		outTex.types = DESCRIPTOR_TYPE_UAV;
		outTex.format = DXGI_FORMAT_R32_FLOAT;
		ConstantBufferJob cbOut = { "SSAOConstants", new SSAOConstants() };
		ShaderDesc SSAOShaders = { "SSAO.hlsl", "SSAO", "SSAO", SHADER_TYPE_CS, {} };

		PipeLineStageDesc stageDesc;
		stageDesc.descriptorJobs = { { noiseTex, depthTex, colorTex, normalTex, tangentTex, binormalTex, worldTex, outTexDesc } };
		stageDesc.rootSigDesc = { cbvPDesc, noisePDesc, rootPDesc, colorPDesc, normalPDesc, tangentPDesc, binormalPDesc, worldPDesc, uavPDesc };
		stageDesc.resourceJobs = { outTex };
		stageDesc.shaderFiles = { SSAOShaders };
		stageDesc.constantBufferJobs = { cbOut };
		stageDesc.externalResources = { 
			std::make_pair("renderOutputTex",renderStage->getResource("depthTex")),
			std::make_pair("renderOutputColor",renderStage->getResource("outTexArray[0]")),
			std::make_pair("renderOutputSpec",renderStage->getResource("outTexArray[1]")),
			std::make_pair("renderOutputNormals",renderStage->getResource("outTexArray[2]")),
			std::make_pair("renderOutputTangents",renderStage->getResource("outTexArray[3]")),
			std::make_pair("renderOutputBinormals",renderStage->getResource("outTexArray[4]")),
			std::make_pair("renderOutputPosition",renderStage->getResource("outTexArray[5]"))
		};
		stageDesc.textureFiles = { {"noise_tex", "default_noise.dds"} };
		ComputePipelineDesc cDesc;
		cDesc.groupCount[0] = (UINT)ceilf(SCREEN_WIDTH / 8.0f);
		cDesc.groupCount[1] = (UINT)ceilf(SCREEN_HEIGHT / 8.0f);
		cDesc.groupCount[2] = 1;
		computeStage = new ComputePipelineStage(md3dDevice, cDesc);
		computeStage->deferSetup(stageDesc);
		WaitOnFenceForever(computeStage->getFence(), computeStage->triggerFence());
	}

	cpuWaitHandles.push_back(computeStage->deferSetCpuEvent());
	// Perform deferred shading.
	{
		DescriptorJob colorTex;
		colorTex.name = "colorTexDesc";
		colorTex.indirectTarget = "colorTex";
		colorTex.view.srvDesc = DEFAULT_SRV_DESC();
		colorTex.type = DESCRIPTOR_TYPE_SRV;
		DescriptorJob specTex = colorTex;
		specTex.name = "specTexDesc";
		specTex.indirectTarget = "specularTex";
		DescriptorJob normalTex = colorTex;
		normalTex.name = "normalTexDesc";
		normalTex.indirectTarget = "normalTex";
		DescriptorJob tangentTex = colorTex;
		tangentTex.name = "tangentTexDesc";
		tangentTex.indirectTarget = "tangentTex";
		DescriptorJob biNormalTex = colorTex;
		biNormalTex.name = "biNormalTexDesc";
		biNormalTex.indirectTarget = "biNormalTex";
		DescriptorJob worldTex = colorTex;
		worldTex.name = "worldTexDesc";
		worldTex.indirectTarget = "worldTex";
		DescriptorJob deferTexDesc;
		deferTexDesc.name = "deferTexDesc";
		deferTexDesc.indirectTarget = "deferTex";
		deferTexDesc.type = DESCRIPTOR_TYPE_RTV;
		deferTexDesc.view.rtvDesc = DEFAULT_RTV_DESC();
		std::vector<RootParamDesc> params;
		params.push_back({ "MergeConstants", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER,
			0, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, DESCRIPTOR_USAGE_ALL });
		// Trying to bind all textures at once to be efficient.
		params.push_back({ "colorTexDesc", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
			1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6, DESCRIPTOR_USAGE_ALL });
		RenderTargetDesc deferTarget = { "deferTexDesc", 0 };
		ConstantBufferJob mergedCB = { "MergeConstants", new MergeConstants() };
		ResourceJob deferTexRes = { "deferTex", DESCRIPTOR_TYPE_RTV };
		std::vector<DXDefine> defines = {
			{L"MAX_LIGHTS", std::to_wstring(MAX_LIGHTS)}
		};
		ShaderDesc deferShaderVS = { "DeferShading.hlsl", "Defer Shader VS", "DeferVS", SHADER_TYPE_VS, defines };
		ShaderDesc deferShaderPS = { "DeferShading.hlsl", "Defer Shader PS", "DeferPS", SHADER_TYPE_PS, defines };

		PipeLineStageDesc stageDesc;
		stageDesc.descriptorJobs = { colorTex, specTex,
			normalTex, tangentTex, biNormalTex, worldTex, deferTexDesc };
		stageDesc.constantBufferJobs = { mergedCB };
		stageDesc.renderTargets = { deferTarget };
		stageDesc.resourceJobs = { deferTexRes };
		stageDesc.rootSigDesc = params;
		stageDesc.shaderFiles = { deferShaderVS, deferShaderPS };
		stageDesc.externalResources = {
			{"colorTex", renderStage->getResource("outTexArray[0]")},
			{"specularTex", renderStage->getResource("outTexArray[1]")},
			{"normalTex", renderStage->getResource("outTexArray[2]")},
			{"tangentTex", renderStage->getResource("outTexArray[3]")},
			{"biNormalTex", renderStage->getResource("outTexArray[4]")},
			{"worldTex", renderStage->getResource("outTexArray[5]")},
			{"VRS", renderStage->getResource("VRS")} };
		stageDesc.textureFiles = {
			{"default_normal", "default_bump.dds"},
			{"default_spec", "default_spec.dds"},
			{"default_diff", "default_diff.dds"}
		};

		RenderPipelineDesc mergeRDesc;
		mergeRDesc.supportsVRS = true;
		mergeRDesc.supportsCulling = false;
		mergeRDesc.usesMeshlets = false;
		mergeRDesc.usesDepthTex = false;
		deferStage = new RenderPipelineStage(md3dDevice, mergeRDesc, DEFAULT_VIEW_PORT(), mScissorRect);
		deferStage->deferSetup(stageDesc);
		WaitOnFenceForever(deferStage->getFence(), deferStage->triggerFence());
		deferStage->frustrumCull = false;
	}

	cpuWaitHandles.push_back(deferStage->deferSetCpuEvent());
	// Merge deferred shading with SSAO/Shadows
	{
		PipeLineStageDesc stageDesc;
		stageDesc.descriptorJobs.push_back(DescriptorJob("SSAOTexDesc", "SSAOTex", DESCRIPTOR_TYPE_SRV));
		stageDesc.descriptorJobs.push_back(DescriptorJob("colorTexDesc", "colorTex", DESCRIPTOR_TYPE_SRV));
		stageDesc.descriptorJobs.push_back(DescriptorJob("mergedTexDesc", "mergedTex", DESCRIPTOR_TYPE_RTV));

		stageDesc.externalResources.push_back(std::make_pair("colorTex", deferStage->getResource("deferTex")));
		stageDesc.externalResources.push_back(std::make_pair("SSAOTex", computeStage->getResource("SSAOOutTexture")));

		stageDesc.renderTargets.push_back(RenderTargetDesc("mergedTexDesc", 0));

		stageDesc.resourceJobs.push_back(ResourceJob("mergedTex", DESCRIPTOR_TYPE_RTV));

		stageDesc.rootSigDesc.push_back(RootParamDesc("SSAOTexDesc", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2));

		stageDesc.shaderFiles.push_back(ShaderDesc("Merge.hlsl", "Merge Shader VS", "MergeVS", SHADER_TYPE_VS, { DXDefine(L"MAX_LIGHTS", std::to_wstring(MAX_LIGHTS)) }));
		stageDesc.shaderFiles.push_back(ShaderDesc("Merge.hlsl", "Merge Shader PS", "MergePS", SHADER_TYPE_PS, { DXDefine(L"MAX_LIGHTS", std::to_wstring(MAX_LIGHTS)) }));

		RenderPipelineDesc rDesc;
		rDesc.supportsCulling = false;
		rDesc.supportsVRS = false;
		rDesc.usesMeshlets = false;
		rDesc.usesDepthTex = false;

		mergeStage = new RenderPipelineStage(md3dDevice, rDesc, DEFAULT_VIEW_PORT(), mScissorRect);
		mergeStage->deferSetup(stageDesc);
		WaitOnFenceForever(mergeStage->getFence(), mergeStage->triggerFence());
	}
	cpuWaitHandles.push_back(mergeStage->deferSetCpuEvent());
	// Compute the VRS image for the next frame.
	{
		DescriptorJob depthTex;
		depthTex.name = "inputDepth";
		depthTex.indirectTarget = "renderOutputTex";
		depthTex.type = DESCRIPTOR_TYPE_SRV;
		depthTex.view.srvDesc = DEFAULT_SRV_DESC();
		depthTex.usage = DESCRIPTOR_USAGE_PER_PASS;
		depthTex.usageIndex = 0;
		DescriptorJob outTexDesc;
		outTexDesc.name = "VrsOut";
		outTexDesc.indirectTarget = "VrsOutTexture";
		outTexDesc.type = DESCRIPTOR_TYPE_UAV;
		outTexDesc.view.uavDesc = DEFAULT_UAV_DESC();
		outTexDesc.view.uavDesc.Format = DXGI_FORMAT_R8_UINT;
		outTexDesc.usage = DESCRIPTOR_USAGE_PER_PASS;
		outTexDesc.usageIndex = 0;
		RootParamDesc rootPDesc;
		rootPDesc.name = "inputDepth";
		rootPDesc.type = ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootPDesc.numConstants = 1;
		rootPDesc.rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		rootPDesc.usagePattern = DESCRIPTOR_USAGE_PER_PASS;
		rootPDesc.slot = 0;
		RootParamDesc uavPDesc;
		uavPDesc.name = "VrsOut";
		uavPDesc.type = ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		uavPDesc.numConstants = 1;
		uavPDesc.rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		uavPDesc.usagePattern = DESCRIPTOR_USAGE_PER_PASS;
		uavPDesc.slot = 1;
		std::vector<DXDefine> defines = {
			// Processing in square wavefronts, so have to round down.
			{L"N", std::to_wstring(waveSupport.WaveLaneCountMax)},
			{L"TILE_SIZE", std::to_wstring(vrsSupport.ShadingRateImageTileSize)},
			{L"EXTRA_SAMPLES", L"0"} };
		ShaderDesc VrsShader = { "..\\Shaders\\VRSCompute.hlsl", "VRS Compute", 
			"VRSOut", SHADER_TYPE_CS, defines };

		ConstantBufferJob VrsConst = { "VrsConstants", new VrsConstants() };
		RootParamDesc VrsConstPDesc;
		VrsConstPDesc.name = "VrsConstants";
		VrsConstPDesc.type = ROOT_PARAMETER_TYPE_CONSTANT_BUFFER;
		VrsConstPDesc.numConstants = 1;
		VrsConstPDesc.rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		VrsConstPDesc.usagePattern = DESCRIPTOR_USAGE_PER_PASS;
		VrsConstPDesc.slot = 2;

		PipeLineStageDesc stageDesc;
		stageDesc.descriptorJobs = { depthTex, outTexDesc };
		stageDesc.rootSigDesc = { rootPDesc, uavPDesc, VrsConstPDesc };
		stageDesc.shaderFiles = { VrsShader };
		stageDesc.constantBufferJobs = { VrsConst };
		stageDesc.externalResources = {
			std::make_pair("renderOutputTex", mergeStage->getResource("mergedTex")),
			std::make_pair("VrsOutTexture", renderStage->getResource("VRS"))
		};
		ComputePipelineDesc cDesc;
		cDesc.groupCount[0] = DivRoundUp(SCREEN_WIDTH, vrsSupport.ShadingRateImageTileSize);
		cDesc.groupCount[1] = DivRoundUp(SCREEN_HEIGHT, vrsSupport.ShadingRateImageTileSize);
		cDesc.groupCount[2] = 1;
		
		vrsComputeStage = new ComputePipelineStage(md3dDevice, cDesc);
		vrsComputeStage->deferSetup(stageDesc);
	}
	cpuWaitHandles.push_back(vrsComputeStage->deferSetCpuEvent());

	modelLoader = new ModelLoader(md3dDevice);
	deferStage->LoadModel(modelLoader, "screenTex.obj", baseDir);
	mergeStage->LoadModel(modelLoader, "screenTex.obj", baseDir);
	renderStage->LoadMeshletModel(modelLoader, headSmallMeshlet, headDir);
	renderStage->LoadModel(modelLoader, sponzaFile, sponzaDir);
	//renderStage->LoadMeshletModel(modelLoader, armorMeshlet, armorDir);
	//renderStage->LoadMeshletModel(modelLoader, headMeshlet, headDir);
	//renderStage->LoadModel(modelLoader, headSmallFile, headDir);
	//renderStage->LoadModel(modelLoader, headFile, headDir);
	//renderStage->LoadModel(modelLoader, armorFile, armorDir);

	
	mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr);

	BuildFrameResources();

	// All CPU side setup work must be somewhat complete to resolve the transitions between stages.
	WaitForMultipleObjects(cpuWaitHandles.size(), cpuWaitHandles.data(), TRUE, INFINITE);
	std::vector<CD3DX12_RESOURCE_BARRIER> initialTransitions = PipelineStage::setupResourceTransitions({ renderStage, computeStage, deferStage, mergeStage, vrsComputeStage });
	mCommandList->ResourceBarrier(initialTransitions.size(), initialTransitions.data());

	mCommandList->Close();
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	FlushCommandQueue();


	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(hWindow);
	ImGui_ImplDX12_Init(md3dDevice.Get(), CPU_FRAME_COUNT, COLOR_TEXTURE_FORMAT, mImguiHeap.Get(),
		mImguiHeap->GetCPUDescriptorHandleForHeapStart(), mImguiHeap->GetGPUDescriptorHandleForHeapStart());
	ImGui_ImplDX12_CreateDeviceObjects();

	return true;
}

void DemoApp::update() {
	onKeyboardInput();
	updateCamera();

	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % CPU_FRAME_COUNT;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	renderStage->nextFrame();
	computeStage->nextFrame();
	deferStage->nextFrame();
	mergeStage->nextFrame();
	vrsComputeStage->nextFrame();

	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence) {
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		assert(eventHandle != NULL);
		mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle);
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	UpdateObjectCBs();
	UpdateMaterialCBs();
	UpdateMainPassCB();
}

void DemoApp::draw() {
	ImGuiPrepareUI();

	std::chrono::high_resolution_clock::time_point startFrameTime = std::chrono::high_resolution_clock::now();

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	cmdListAlloc->Reset();

	mCommandList->Reset(cmdListAlloc.Get(), defaultPSO.Get());

	SetName(mCommandList.Get(), L"Meta Command List");

	auto transToCopy = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
	mCommandList->ResourceBarrier(1, &transToCopy);

	std::vector<HANDLE> eventHandles;
	eventHandles.push_back(renderStage->deferExecute());
	eventHandles.push_back(computeStage->deferExecute());
	eventHandles.push_back(deferStage->deferExecute());
	eventHandles.push_back(mergeStage->deferExecute());
	eventHandles.push_back(vrsComputeStage->deferExecute());

	PIXBeginEvent(mCommandList.Get(), PIX_COLOR(0.0, 0.0, 1.0), "Copy and Show");

	WaitForMultipleObjects(eventHandles.size(), eventHandles.data(), TRUE, INFINITE);

	// TODO: FIND A WAY TO MAKE SURE THIS RESOURCE ALWAYS GOES BACK TO THE CORRECT STATE
	DX12Resource* mergeOut = mergeStage->getResource("mergedTex");
	auto mergeTransitionIn = CD3DX12_RESOURCE_BARRIER::Transition(mergeOut->get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_SOURCE);
	mCommandList->ResourceBarrier(1, &mergeTransitionIn);
	mCommandList->CopyResource(CurrentBackBuffer(), mergeOut->get());
	auto mergeTransitionOut = CD3DX12_RESOURCE_BARRIER::Transition(mergeOut->get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
	mCommandList->ResourceBarrier(1, &mergeTransitionOut);

	auto transToRender = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mCommandList->ResourceBarrier(1, &transToRender);

	mCommandList->SetDescriptorHeaps(1, mImguiHeap.GetAddressOf());

	auto backBufferView = CurrentBackBufferView();
	mCommandList->OMSetRenderTargets(1, &backBufferView, true, nullptr);

	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mCommandList.Get());

	auto transToPresent = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	mCommandList->ResourceBarrier(1, &transToPresent);

	PIXEndEvent(mCommandList.Get());
	
	mCommandList->Close();

	ID3D12CommandList* cmdList[] = { renderStage->mCommandList.Get(), computeStage->mCommandList.Get(), deferStage->mCommandList.Get(), mergeStage->mCommandList.Get(), vrsComputeStage->mCommandList.Get(), mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdList), cmdList);

	mSwapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	mCurrFrameResource->Fence = ++mCurrentFence;

	mCommandQueue->Signal(mFence.Get(), mCurrentFence);

	std::chrono::high_resolution_clock::time_point endFrameTime = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double, std::milli> time_span = endFrameTime - startFrameTime;
	cpuFrametime.push_back(time_span.count());
}

void DemoApp::ImGuiPrepareUI() {
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGui::Begin("Info Window", &showImgui);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
	frametime.push_back(ImGui::GetIO().DeltaTime * 1000);
	if (frametime.size() > 1000) frametime.pop_front();
	if (cpuFrametime.size() > 1000) cpuFrametime.pop_front();
	std::vector<float> frametimeVec(frametime.begin(), frametime.end());
	std::vector<float> cpuFrametimeVec(cpuFrametime.begin(), cpuFrametime.end());
	ImGui::PlotLines("Frame Times (ms)", frametimeVec.data(), frametimeVec.size(), 0, "Frame Times (ms)", 0.0f, 8.0f, ImVec2(ImGui::GetWindowWidth(), 100));
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	ImGui::Text("Last 1000 Frame Average %.3f ms/frame", AverageVector(frametimeVec));
	ImGui::PlotLines("CPU Frame Times (ms)", cpuFrametimeVec.data(), cpuFrametimeVec.size(), 0, "CPU Frame Times (ms)", 0.0f, 8.0f, ImVec2(ImGui::GetWindowWidth(), 100));
	ImGui::Text("Last 1000 CPU Frame Average %.3f ms/frame", AverageVector(cpuFrametimeVec));
	ImGui::Checkbox("Frustrum Culling", &renderStage->frustrumCull);
	ImGui::Checkbox("Freeze Culling", &freezeCull);
	ImGui::Checkbox("Occlusion Predication Culling", &renderStage->occlusionCull);
	ImGui::Checkbox("Meshlet Normal Cone Culling", (bool*)&mainPassCB.data.meshletCull);
	ImGui::Checkbox("VRS", &VRS);
	ImGui::Checkbox("Render VRS", &renderVRS);
	ImGui::Checkbox("SSAO", (bool*)&ssaoConstantCB.data.showSSAO);
	ImGui::Checkbox("Screen Space Shadows", (bool*)&ssaoConstantCB.data.showSSShadows);
	ImGui::Checkbox("VRS Average Luminance", (bool*)&vrsCB.data.vrsAvgLum);
	ImGui::Checkbox("VRS Variance Luminance", (bool*)&vrsCB.data.vrsVarLum);
	ImGui::BeginTabBar("Adjustable Params");
	if (ImGui::BeginTabItem("SSAO/CS Shadow Params")) {
		ImGui::SliderInt("SSAO Samples", &ssaoConstantCB.data.rayCount, 1, 100);
		ImGui::SliderFloat("SSAO Ray Length", &ssaoConstantCB.data.rayLength, 0.0f, 10.0f);
		ImGui::SliderInt("Shadow Steps", &ssaoConstantCB.data.shadowSteps, 1, 100);
		ImGui::SliderFloat("Shadow Step Size", &ssaoConstantCB.data.shadowStepSize, 0.0f, 0.1f);
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem("VRS Ranges")) {
		ImGui::SliderFloat("VRS Short", &mainPassCB.data.VrsShort, 0.0f, 2000.0f);
		ImGui::SliderFloat("VRS Medium", &mainPassCB.data.VrsMedium, 0.0f, 2000.0f);
		ImGui::SliderFloat("VRS Long", &mainPassCB.data.VrsLong, 0.0f, 2000.0f);
		ImGui::SliderFloat("VRS Avg Lum Cutoff", &vrsCB.data.vrsAvgCut, 0.0f, 1.0f);
		ImGui::SliderFloat("VRS Lum Variance", &vrsCB.data.vrsVarianceCut, 0.0f, 1.0f);
		ImGui::SliderInt("VRS Lum Variance Voters", &vrsCB.data.vrsVarianceVotes, 0, vrsSupport.ShadingRateImageTileSize * vrsSupport.ShadingRateImageTileSize);
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem("Light Options")) {
		ImGui::SliderFloat3("Light Pos", (float*)&mergeConstantCB.data.lights[0].pos, -1000.0f, 1000.0f);
		ImGui::SliderFloat("Light Strength", &mergeConstantCB.data.lights[0].strength, 0.0f, 5000.0f);
		ImGui::ColorEdit3("Light Color", (float*)&mergeConstantCB.data.lights[0].color);
		ImGui::EndTabItem();
	}
	ImGui::EndTabBar();
	ImGui::End();
}

void DemoApp::onResize() {
	DX12App::onResize();

	DirectX::XMMATRIX P = DirectX::XMMatrixPerspectiveFovLH(0.25f * DirectX::XM_PI, getAspectRatio(), 1.0f, 5000.f);
	XMStoreFloat4x4(&proj, P);
}

void DemoApp::BuildFrameResources() {
	for (int i = 0; i < CPU_FRAME_COUNT; i++) {
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get()));
		mFrameResources[i].get()->CmdListAlloc->SetName((L"Alloc" + std::to_wstring(i)).c_str());
	}
}

void DemoApp::onKeyboardInput() {
	keyboard.update();

	DirectX::XMFLOAT4 move = { 0.0f, 0.0f, 0.0f, 0.0f };

	DirectX::XMMATRIX look = DirectX::XMMatrixRotationRollPitchYaw(lookAngle[0], lookAngle[1], lookAngle[2]);

	if (keyboard.getKeyStatus('W') & KEY_STATUS_PRESSED) {
		move.z += 100.0f;
	}
	if (keyboard.getKeyStatus('S') & KEY_STATUS_PRESSED) {
		move.z -= 100.0f;
	}
	if (keyboard.getKeyStatus('A') & KEY_STATUS_PRESSED) {
		move.x -= 100.0f;
	}
	if (keyboard.getKeyStatus('D') & KEY_STATUS_PRESSED) {
		move.x += 100.0f;
	}
	if (keyboard.getKeyStatus(VK_LSHIFT) & KEY_STATUS_PRESSED) {
		move.x = 4.0f * move.x;
		move.y = 4.0f * move.y;
		move.z = 4.0f * move.z;
	}
	if (keyboard.getKeyStatus(VK_LCONTROL) & KEY_STATUS_JUST_PRESSED) {
		lockMouse = !lockMouse;
		lockMouseDirty = true;
	}

	DirectX::XMFLOAT4 moveRes = {};
	DirectX::XMStoreFloat4(&moveRes, DirectX::XMVector4Transform(
		DirectX::XMVectorSet(move.x, move.y, move.z, move.w), look));
	
	float timeElapsed = ImGui::GetIO().DeltaTime;
	eyePos.x += moveRes.x * timeElapsed;
	eyePos.y += moveRes.y * 0.0f;
	eyePos.z += moveRes.z * timeElapsed;
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
	float dy = float(y - (SCREEN_HEIGHT / 2) + 17);

	lookAngle[0] = std::min(std::max((lookAngle[0] + DirectX::XMConvertToRadians(0.25f * dy)), -AI_MATH_HALF_PI_F + 0.01f), AI_MATH_HALF_PI_F - 0.01f);
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

}

void DemoApp::UpdateMaterialCBs() {
}

void DemoApp::UpdateMainPassCB() {
	DirectX::XMMATRIX view = XMLoadFloat4x4(&this->view);
	DirectX::XMMATRIX proj = XMLoadFloat4x4(&this->proj);

	DirectX::XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	auto viewDet = XMMatrixDeterminant(view);
	DirectX::XMMATRIX invView = XMMatrixInverse(&viewDet, view);
	auto projDet = XMMatrixDeterminant(proj);
	DirectX::XMMATRIX invProj = XMMatrixInverse(&projDet, proj);
	auto viewProjDet = XMMatrixDeterminant(viewProj);
	DirectX::XMMATRIX invViewProj = XMMatrixInverse(&viewProjDet, viewProj);

	XMStoreFloat4x4(&mainPassCB.data.view, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mainPassCB.data.invView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mainPassCB.data.proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mainPassCB.data.invProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mainPassCB.data.viewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mainPassCB.data.invViewProj, XMMatrixTranspose(invViewProj));
	// Easiest way to stop updating the viewer position for meshlet culling.
	// Sadly breaks specular lighting, but this is a debug toggle, so no big deal.
	if (!freezeCull) {
		mainPassCB.data.EyePosW = DirectX::XMFLOAT3(eyePos.x, eyePos.y, eyePos.z);
	}
	mainPassCB.data.RenderTargetSize = DirectX::XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mainPassCB.data.InvRenderTargetSize = DirectX::XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mainPassCB.data.NearZ = 1.0f;
	mainPassCB.data.FarZ = 5000.0f;
	mainPassCB.data.DeltaTime = ImGui::GetIO().DeltaTime;
	mainPassCB.data.TotalTime += mainPassCB.data.DeltaTime;
	mainPassCB.data.renderVRS = renderVRS;

	ssaoConstantCB.data.range = mainPassCB.data.FarZ / (mainPassCB.data.FarZ - mainPassCB.data.NearZ);
	ssaoConstantCB.data.rangeXNear = ssaoConstantCB.data.range * mainPassCB.data.NearZ;
	ssaoConstantCB.data.lightPos = { mergeConstantCB.data.lights[0].pos.x, mergeConstantCB.data.lights[0].pos.y, mergeConstantCB.data.lights[0].pos.z, 1.0f };
	ssaoConstantCB.data.viewProj = mainPassCB.data.viewProj;

	computeStage->deferUpdateConstantBuffer("SSAOConstants", ssaoConstantCB);

	mergeConstantCB.data.viewPos = mainPassCB.data.EyePosW;
	mergeConstantCB.data.numPointLights = 1;

	vrsComputeStage->deferUpdateConstantBuffer("VrsConstants", vrsCB);
	deferStage->deferUpdateConstantBuffer("MergeConstants", mergeConstantCB);
	renderStage->deferUpdateConstantBuffer("PerPassConstants", mainPassCB);
	if (!freezeCull) {
		renderStage->frustrum = DirectX::BoundingFrustum(proj);
		renderStage->frustrum.Transform(renderStage->frustrum, invView);
	}
	renderStage->eyePos = DirectX::XMFLOAT3(eyePos.x, eyePos.y, eyePos.z);
	renderStage->VRS = VRS;
	deferStage->VRS = VRS;
}
