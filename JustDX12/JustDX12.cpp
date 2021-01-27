#include <iostream>
#include <chrono>
#include "ModelLoading\ModelLoader.h"
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
std::string sponzaDir = baseDir + "\\sponza";
std::string sponzaFile = "sponza.fbx";
std::string armorDir = baseDir + "\\parade_armor";
std::string armorFile = "armor.fbx";
std::string headDir = baseDir + "\\head";
std::string headFile = "head.fbx";
std::string armorMeshlet = "armor.bin";

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
	std::vector<std::unique_ptr<FrameResource>> mComputeFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	FrameResource* mCurrComputeFrameResource = nullptr;
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
	KeyboardWrapper keyboard;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mSSAORootSignature = nullptr;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> shaders;
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> defaultPSO = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> ssaoPSO = nullptr;

	PerPassConstants mainPassCB;
	PerObjectConstants perObjCB;
	PerObjectConstants perMeshletObjCB;
	VrsConstants vrsCB;
	SSAOConstants ssaoConstantCB;
	LightData lightDataCB;

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
		PipeLineStageDesc rasterDesc;
		rasterDesc.name = "Forward Pass";

		rasterDesc.constantBufferJobs.push_back(ConstantBufferJob("PerObjectConstants", new PerObjectConstants(), 0));
		rasterDesc.constantBufferJobs.push_back(ConstantBufferJob("PerObjectConstants", new PerObjectConstants(), 1));
		rasterDesc.constantBufferJobs.push_back(ConstantBufferJob("PerObjectConstants", new PerObjectConstants(), 2));
		rasterDesc.constantBufferJobs.push_back(ConstantBufferJob("PerObjectConstants", new PerObjectConstants(), 3));
		rasterDesc.constantBufferJobs.push_back(ConstantBufferJob("PerObjectConstants", new PerObjectConstants(), 4));
		rasterDesc.constantBufferJobs.push_back(ConstantBufferJob("PerObjectConstants", new PerObjectConstants(), 5));
		rasterDesc.constantBufferJobs.push_back(ConstantBufferJob("PerObjectConstantsMeshlet", new PerObjectConstants(), 0));
		rasterDesc.constantBufferJobs.push_back(ConstantBufferJob("PerPassConstants", new PerPassConstants(), 0));

		rasterDesc.descriptorJobs.push_back(DescriptorJob("outTexDesc[0]", "outTexArray[0]", DESCRIPTOR_TYPE_RTV));
		rasterDesc.descriptorJobs.push_back(DescriptorJob("outTexDesc[1]", "outTexArray[1]", DESCRIPTOR_TYPE_RTV));
		rasterDesc.descriptorJobs.push_back(DescriptorJob("outTexDesc[2]", "outTexArray[2]", DESCRIPTOR_TYPE_RTV));
		rasterDesc.descriptorJobs.push_back(DescriptorJob("outTexDesc[3]", "outTexArray[3]", DESCRIPTOR_TYPE_RTV));
		rasterDesc.descriptorJobs.push_back(DescriptorJob("outTexDesc[4]", "outTexArray[4]", DESCRIPTOR_TYPE_RTV));
		rasterDesc.descriptorJobs.push_back(DescriptorJob("outTexDesc[5]", "outTexArray[5]", DESCRIPTOR_TYPE_RTV));
		rasterDesc.descriptorJobs.push_back(DescriptorJob("depthStencilView", "depthTex", DESCRIPTOR_TYPE_DSV, false));
		rasterDesc.descriptorJobs.back().view.dsvDesc = DEFAULT_DSV_DESC();

		rasterDesc.renderTargets.push_back(RenderTargetDesc("outTexDesc[0]", 0));
		rasterDesc.renderTargets.push_back(RenderTargetDesc("outTexDesc[1]", 0));
		rasterDesc.renderTargets.push_back(RenderTargetDesc("outTexDesc[2]", 0));
		rasterDesc.renderTargets.push_back(RenderTargetDesc("outTexDesc[3]", 0));
		rasterDesc.renderTargets.push_back(RenderTargetDesc("outTexDesc[4]", 0));
		rasterDesc.renderTargets.push_back(RenderTargetDesc("outTexDesc[5]", 0));

		rasterDesc.resourceJobs.push_back(ResourceJob("VRS", DESCRIPTOR_TYPE_UAV, DXGI_FORMAT_R8_UINT, DivRoundUp(SCREEN_HEIGHT, vrsSupport.ShadingRateImageTileSize), DivRoundUp(SCREEN_WIDTH, vrsSupport.ShadingRateImageTileSize)));
		rasterDesc.resourceJobs.push_back(ResourceJob("outTexArray[0]", DESCRIPTOR_TYPE_SRV | DESCRIPTOR_TYPE_RTV | DESCRIPTOR_TYPE_FLAG_SIMULTANEOUS_ACCESS));
		rasterDesc.resourceJobs.push_back(ResourceJob("outTexArray[1]", DESCRIPTOR_TYPE_SRV | DESCRIPTOR_TYPE_RTV | DESCRIPTOR_TYPE_FLAG_SIMULTANEOUS_ACCESS));
		rasterDesc.resourceJobs.push_back(ResourceJob("outTexArray[2]", DESCRIPTOR_TYPE_SRV | DESCRIPTOR_TYPE_RTV | DESCRIPTOR_TYPE_FLAG_SIMULTANEOUS_ACCESS));
		rasterDesc.resourceJobs.push_back(ResourceJob("outTexArray[3]", DESCRIPTOR_TYPE_SRV | DESCRIPTOR_TYPE_RTV | DESCRIPTOR_TYPE_FLAG_SIMULTANEOUS_ACCESS));
		rasterDesc.resourceJobs.push_back(ResourceJob("outTexArray[4]", DESCRIPTOR_TYPE_SRV | DESCRIPTOR_TYPE_RTV | DESCRIPTOR_TYPE_FLAG_SIMULTANEOUS_ACCESS));
		rasterDesc.resourceJobs.push_back(ResourceJob("outTexArray[5]", DESCRIPTOR_TYPE_SRV | DESCRIPTOR_TYPE_RTV | DESCRIPTOR_TYPE_FLAG_SIMULTANEOUS_ACCESS));
		rasterDesc.resourceJobs.push_back(ResourceJob("depthTex", DESCRIPTOR_TYPE_SRV | DESCRIPTOR_TYPE_DSV, DEPTH_TEXTURE_FORMAT));

		rasterDesc.rootSigDesc.push_back(RootParamDesc("PerObjectConstants", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER, 0, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, DESCRIPTOR_USAGE_PER_OBJECT));
		rasterDesc.rootSigDesc.push_back(RootParamDesc("texture_diffuse", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, DESCRIPTOR_USAGE_PER_MESH));
		rasterDesc.rootSigDesc.push_back(RootParamDesc("PerPassConstants", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER, 2, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, DESCRIPTOR_USAGE_PER_PASS));

		std::vector<DXDefine> defines = {
			{L"VRS", L""},
			{L"VRS_4X4", L""} };

		// Shaders are binding order dependent/bound by unique names. This needs to be fixed. Probably add a usage context for shaders?
		rasterDesc.shaderFiles.push_back(ShaderDesc("MeshletAS.hlsl", "Amplification Shader", "AS", SHADER_TYPE_AS, {}));
		rasterDesc.shaderFiles.push_back(ShaderDesc("MeshletMS.hlsl", "Mesh Shader", "MS", SHADER_TYPE_MS, {}));
		rasterDesc.shaderFiles.push_back(ShaderDesc("MeshletMS.hlsl", "Mesh Pixel Shader", "PS", SHADER_TYPE_PS, {}));
		rasterDesc.shaderFiles.push_back(ShaderDesc("Default.hlsl", "Vertex Shader", "VS", SHADER_TYPE_VS, defines));
		rasterDesc.shaderFiles.push_back(ShaderDesc("Default.hlsl", "Pixel Shader", "PS", SHADER_TYPE_PS, defines));

		rasterDesc.textureFiles.push_back(std::make_pair("default_normal", "default_bump.dds"));
		rasterDesc.textureFiles.push_back(std::make_pair("default_spec", "default_spec.dds"));
		rasterDesc.textureFiles.push_back(std::make_pair("default_diff", "default_diff.dds"));
		rasterDesc.textureFiles.push_back(std::make_pair("default_alpha", "default_alpha.dds"));

		RenderPipelineDesc rDesc;
		rDesc.usesMeshlets = true;
		rDesc.supportsCulling = true;
		rDesc.supportsVRS = true;

		rDesc.defaultTextures[MODEL_FORMAT_DIFFUSE_TEX] = "default_diff";
		rDesc.defaultTextures[MODEL_FORMAT_SPECULAR_TEX] = "default_spec";
		rDesc.defaultTextures[MODEL_FORMAT_NORMAL_TEX] = "default_normal";
		rDesc.defaultTextures[MODEL_FORMAT_OPACITY_TEX] = "default_alpha";

		// Have to generate meshlet root signature
		rDesc.meshletRootSignature.push_back(RootParamDesc("MeshInfo", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER, 0, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, DESCRIPTOR_USAGE_PER_MESHLET));
		rDesc.meshletRootSignature.push_back(RootParamDesc("Vertices", ROOT_PARAMETER_TYPE_SRV, 1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, DESCRIPTOR_USAGE_PER_MESHLET));
		rDesc.meshletRootSignature.push_back(RootParamDesc("Meshlets", ROOT_PARAMETER_TYPE_SRV, 2, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, DESCRIPTOR_USAGE_PER_MESHLET));
		rDesc.meshletRootSignature.push_back(RootParamDesc("UniqueVertexIndices", ROOT_PARAMETER_TYPE_SRV, 3, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, DESCRIPTOR_USAGE_PER_MESHLET));
		rDesc.meshletRootSignature.push_back(RootParamDesc("PrimitiveIndices", ROOT_PARAMETER_TYPE_SRV, 4, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, DESCRIPTOR_USAGE_PER_MESHLET));
		rDesc.meshletRootSignature.push_back(RootParamDesc("MeshletCullData", ROOT_PARAMETER_TYPE_SRV, 5, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, DESCRIPTOR_USAGE_PER_MESHLET));
		rDesc.meshletRootSignature.push_back(RootParamDesc("PerObjectConstantsMeshlet", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER, 6, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, DESCRIPTOR_USAGE_PER_OBJECT));
		rDesc.meshletRootSignature.push_back(RootParamDesc("mesh_texture_diffuse", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 7, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, DESCRIPTOR_USAGE_PER_MESH));
		rDesc.meshletRootSignature.push_back(RootParamDesc("PerPassConstants", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER, 8, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, DESCRIPTOR_USAGE_PER_PASS));

		rDesc.textureToDescriptor.emplace_back(MODEL_FORMAT_DIFFUSE_TEX, "texture_diffuse");
		rDesc.textureToDescriptor.emplace_back(MODEL_FORMAT_SPECULAR_TEX, "texture_spec");
		rDesc.textureToDescriptor.emplace_back(MODEL_FORMAT_NORMAL_TEX, "texture_normal");
		rDesc.textureToDescriptor.emplace_back(MODEL_FORMAT_OPACITY_TEX, "texture_alpha");

		rDesc.meshletTextureToDescriptor.emplace_back(MODEL_FORMAT_DIFFUSE_TEX, "mesh_texture_diffuse");
		rDesc.meshletTextureToDescriptor.emplace_back(MODEL_FORMAT_SPECULAR_TEX, "mesh_texture_specular");
		rDesc.meshletTextureToDescriptor.emplace_back(MODEL_FORMAT_NORMAL_TEX, "mesh_texture_normal");
		rDesc.meshletTextureToDescriptor.emplace_back(MODEL_FORMAT_OPACITY_TEX, "mesh_texture_alpha");

		renderStage = new RenderPipelineStage(md3dDevice, rDesc, DEFAULT_VIEW_PORT(), mScissorRect);
		renderStage->deferSetup(rasterDesc);
		WaitOnFenceForever(renderStage->getFence(), renderStage->triggerFence());
	}
	cpuWaitHandles.push_back(renderStage->deferSetCpuEvent());
	// Perform deferred shading.
	{
		std::vector<DXDefine> defines = {
			{L"MAX_LIGHTS", std::to_wstring(MAX_LIGHTS)}
		};

		PipeLineStageDesc stageDesc;
		stageDesc.name = "Deferred Shading";

		stageDesc.constantBufferJobs.push_back(ConstantBufferJob("LightData", new LightData()));
		stageDesc.constantBufferJobs.push_back(ConstantBufferJob("SSAOConstants", new SSAOConstants()));

		stageDesc.descriptorJobs.push_back(DescriptorJob("inputDepth", "renderDepthTex", DESCRIPTOR_TYPE_SRV, false));
		stageDesc.descriptorJobs.back().view.srvDesc = DEFAULT_SRV_DESC();
		stageDesc.descriptorJobs.back().view.srvDesc.Format = DEPTH_TEXTURE_SRV_FORMAT;
		stageDesc.descriptorJobs.push_back(DescriptorJob("colorTexDesc", "colorTex", DESCRIPTOR_TYPE_SRV));
		stageDesc.descriptorJobs.push_back(DescriptorJob("specTexDesc", "specularTex", DESCRIPTOR_TYPE_SRV));
		stageDesc.descriptorJobs.push_back(DescriptorJob("normalTexDesc", "normalTex", DESCRIPTOR_TYPE_SRV));
		stageDesc.descriptorJobs.push_back(DescriptorJob("tangentTexDesc", "tangentTex", DESCRIPTOR_TYPE_SRV));
		stageDesc.descriptorJobs.push_back(DescriptorJob("biNormalTexDesc", "biNormalTex", DESCRIPTOR_TYPE_SRV));
		stageDesc.descriptorJobs.push_back(DescriptorJob("worldTexDesc", "worldTex", DESCRIPTOR_TYPE_SRV));
		stageDesc.descriptorJobs.push_back(DescriptorJob("deferTexDesc", "deferTex", DESCRIPTOR_TYPE_RTV));

		stageDesc.externalResources.push_back(std::make_pair("renderDepthTex", renderStage->getResource("depthTex")));
		stageDesc.externalResources.push_back(std::make_pair("colorTex", renderStage->getResource("outTexArray[0]")));
		stageDesc.externalResources.push_back(std::make_pair("specularTex", renderStage->getResource("outTexArray[1]")));
		stageDesc.externalResources.push_back(std::make_pair("normalTex", renderStage->getResource("outTexArray[2]")));
		stageDesc.externalResources.push_back(std::make_pair("tangentTex", renderStage->getResource("outTexArray[3]")));
		stageDesc.externalResources.push_back(std::make_pair("biNormalTex", renderStage->getResource("outTexArray[4]")));
		stageDesc.externalResources.push_back(std::make_pair("worldTex", renderStage->getResource("outTexArray[5]")));
		stageDesc.externalResources.push_back(std::make_pair("VRS", renderStage->getResource("VRS")));

		stageDesc.renderTargets.push_back(RenderTargetDesc("deferTexDesc", 0));

		stageDesc.resourceJobs.push_back(ResourceJob("deferTex", DESCRIPTOR_TYPE_RTV));

		stageDesc.rootSigDesc.push_back(RootParamDesc("LightData", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER, 0, D3D12_DESCRIPTOR_RANGE_TYPE_CBV));
		stageDesc.rootSigDesc.push_back(RootParamDesc("SSAOConstants", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER, 1, D3D12_DESCRIPTOR_RANGE_TYPE_CBV));
		stageDesc.rootSigDesc.push_back(RootParamDesc("inputDepth", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 2, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 7));
		stageDesc.rootSigDesc.push_back(RootParamDesc("TLAS", ROOT_PARAMETER_TYPE_SRV, 3));

		stageDesc.shaderFiles.push_back(ShaderDesc("DeferShading.hlsl", "Defer Shader VS", "DeferVS", SHADER_TYPE_VS, defines));
		stageDesc.shaderFiles.push_back(ShaderDesc("DeferShading.hlsl", "Defer Shader PS", "DeferPS", SHADER_TYPE_PS, defines));

		stageDesc.textureFiles.push_back(std::make_pair("default_normal", "default_bump.dds"));
		stageDesc.textureFiles.push_back(std::make_pair("default_spec", "default_spec.dds"));
		stageDesc.textureFiles.push_back(std::make_pair("default_diff", "default_diff.dds"));

		RenderPipelineDesc mergeRDesc;
		mergeRDesc.supportsVRS = true;
		mergeRDesc.supportsCulling = false;
		mergeRDesc.usesMeshlets = false;
		mergeRDesc.usesDepthTex = false;
		mergeRDesc.supportsRT = true;
		mergeRDesc.tlasResourceName = "TLAS";
		deferStage = new RenderPipelineStage(md3dDevice, mergeRDesc, DEFAULT_VIEW_PORT(), mScissorRect);
		deferStage->deferSetup(stageDesc);
		WaitOnFenceForever(deferStage->getFence(), deferStage->triggerFence());
		deferStage->frustrumCull = false;
	}
	cpuWaitHandles.push_back(deferStage->deferSetCpuEvent());
	// Create SSAO/Screen Space Shadow Pass.
	{
		std::vector<DXDefine> defines = {
			{L"MAX_LIGHTS", std::to_wstring(MAX_LIGHTS)}
		};

		PipeLineStageDesc stageDesc;
		stageDesc.name = "SSAO/Shadows";

		stageDesc.descriptorJobs.push_back(DescriptorJob("noise_texDesc", "noise_tex", DESCRIPTOR_TYPE_SRV));
		stageDesc.descriptorJobs.push_back(DescriptorJob("inputDepth", "renderOutputTex", DESCRIPTOR_TYPE_SRV, false));
		stageDesc.descriptorJobs.back().view.srvDesc = DEFAULT_SRV_DESC();
		stageDesc.descriptorJobs.back().view.srvDesc.Format = DEPTH_TEXTURE_SRV_FORMAT;
		stageDesc.descriptorJobs.push_back(DescriptorJob("normalTex", "renderOutputNormals", DESCRIPTOR_TYPE_SRV));
		stageDesc.descriptorJobs.push_back(DescriptorJob("tangentTex", "renderOutputTangents", DESCRIPTOR_TYPE_SRV));
		stageDesc.descriptorJobs.push_back(DescriptorJob("binormalTex", "renderOutputBinormals", DESCRIPTOR_TYPE_SRV));
		stageDesc.descriptorJobs.push_back(DescriptorJob("worldTex", "renderOutputPosition", DESCRIPTOR_TYPE_SRV));
		stageDesc.descriptorJobs.push_back(DescriptorJob("SSAOOut", "SSAOOutTexture", DESCRIPTOR_TYPE_UAV));

		stageDesc.externalConstantBuffers.push_back(std::make_pair(IndexedName("LightData", 0), deferStage->getConstantBuffer(IndexedName("LightData", 0))));
		stageDesc.externalConstantBuffers.push_back(std::make_pair(IndexedName("SSAOConstants", 0), deferStage->getConstantBuffer(IndexedName("SSAOConstants", 0))));

		stageDesc.externalResources.push_back(std::make_pair("renderOutputTex", renderStage->getResource("depthTex")));
		stageDesc.externalResources.push_back(std::make_pair("renderOutputColor", renderStage->getResource("outTexArray[0]")));
		stageDesc.externalResources.push_back(std::make_pair("renderOutputSpec", renderStage->getResource("outTexArray[1]")));
		stageDesc.externalResources.push_back(std::make_pair("renderOutputNormals", renderStage->getResource("outTexArray[2]")));
		stageDesc.externalResources.push_back(std::make_pair("renderOutputTangents", renderStage->getResource("outTexArray[3]")));
		stageDesc.externalResources.push_back(std::make_pair("renderOutputBinormals", renderStage->getResource("outTexArray[4]")));
		stageDesc.externalResources.push_back(std::make_pair("renderOutputPosition", renderStage->getResource("outTexArray[5]")));

		stageDesc.resourceJobs.push_back(ResourceJob("SSAOOutTexture", DESCRIPTOR_TYPE_UAV, DXGI_FORMAT_R32_FLOAT));

		stageDesc.rootSigDesc.push_back(RootParamDesc("LightData", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER, 0, D3D12_DESCRIPTOR_RANGE_TYPE_CBV));
		stageDesc.rootSigDesc.push_back(RootParamDesc("SSAOConstants", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER, 1, D3D12_DESCRIPTOR_RANGE_TYPE_CBV));
		stageDesc.rootSigDesc.push_back(RootParamDesc("noise_texDesc", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 2, D3D12_DESCRIPTOR_RANGE_TYPE_SRV));
		stageDesc.rootSigDesc.push_back(RootParamDesc("inputDepth", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 3, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5));
		stageDesc.rootSigDesc.push_back(RootParamDesc("SSAOOut", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 4, D3D12_DESCRIPTOR_RANGE_TYPE_UAV));

		stageDesc.shaderFiles.push_back(ShaderDesc("SSAO.hlsl", "SSAO", "SSAO", SHADER_TYPE_CS, defines));

		stageDesc.textureFiles.push_back(std::make_pair("noise_tex", "default_noise.dds"));

		ComputePipelineDesc cDesc;
		cDesc.groupCount[0] = (UINT)ceilf(SCREEN_WIDTH / 8.0f);
		cDesc.groupCount[1] = (UINT)ceilf(SCREEN_HEIGHT / 8.0f);
		cDesc.groupCount[2] = 1;
		computeStage = new ComputePipelineStage(md3dDevice, cDesc);
		computeStage->deferSetup(stageDesc);
		WaitOnFenceForever(computeStage->getFence(), computeStage->triggerFence());
	}
	cpuWaitHandles.push_back(computeStage->deferSetCpuEvent());
	// Merge deferred shading with SSAO/Shadows
	{
		PipeLineStageDesc stageDesc;
		stageDesc.name = "Merge";
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
		std::vector<DXDefine> defines = {
			// Processing in square wavefronts, so have to round down.
			DXDefine(L"N", std::to_wstring(waveSupport.WaveLaneCountMax)),
			DXDefine(L"TILE_SIZE", std::to_wstring(vrsSupport.ShadingRateImageTileSize)),
			DXDefine(L"EXTRA_SAMPLES", L"0") };

		PipeLineStageDesc stageDesc;
		stageDesc.name = "VRS Compute";

		stageDesc.constantBufferJobs.push_back(ConstantBufferJob("VrsConstants", new VrsConstants()));

		stageDesc.descriptorJobs.push_back(DescriptorJob("inputColor", "renderOutputTex", DESCRIPTOR_TYPE_SRV));
		stageDesc.descriptorJobs.push_back(DescriptorJob("VrsOut", "VrsOutTexture", DESCRIPTOR_TYPE_UAV));

		stageDesc.externalResources.push_back(std::make_pair("renderOutputTex", mergeStage->getResource("mergedTex")));
		stageDesc.externalResources.push_back(std::make_pair("VrsOutTexture", renderStage->getResource("VRS")));

		stageDesc.rootSigDesc.push_back(RootParamDesc("inputColor", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV));
		stageDesc.rootSigDesc.push_back(RootParamDesc("VrsOut", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 1, D3D12_DESCRIPTOR_RANGE_TYPE_UAV));
		stageDesc.rootSigDesc.push_back(RootParamDesc("VrsConstants", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER, 2, D3D12_DESCRIPTOR_RANGE_TYPE_CBV));

		stageDesc.shaderFiles.push_back(ShaderDesc("VRSCompute.hlsl", "VRS Compute", "VRSOut", SHADER_TYPE_CS, defines));

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
	renderStage->LoadMeshletModel(modelLoader, armorMeshlet, armorDir, true);
	renderStage->LoadModel(modelLoader, headFile, headDir, true);
	renderStage->LoadModel(modelLoader, sponzaFile, sponzaDir, true);

	// Have to have a copy of the armor file loaded so the meshlet copy can use it for a BLAS
	modelLoader->loadModel(armorFile, armorDir, false);

	//Can't really update at runtime due to the limitations of the RT generation at the moment.
	renderStage->updateInstanceCount(0, 5);
	perObjCB.data.World[0] = Identity();
	DirectX::XMStoreFloat4x4(&perObjCB.data.World[1], DirectX::XMMatrixTranspose(DirectX::XMMatrixTranslation(-100.0f, 0.0f, 0.0f)));
	renderStage->updateInstanceTransform(0, 1, (DirectX::XMMatrixTranslation(-100.0f, 0.0f, 0.0f)));
	DirectX::XMStoreFloat4x4(&perObjCB.data.World[2], DirectX::XMMatrixTranspose(DirectX::XMMatrixTranslation(100.0f, 0.0f, 0.0f)));
	renderStage->updateInstanceTransform(0, 2, (DirectX::XMMatrixTranslation(100.0f, 0.0f, 0.0f)));
	DirectX::XMStoreFloat4x4(&perObjCB.data.World[3], DirectX::XMMatrixTranspose(DirectX::XMMatrixTranslation(-200.0f, 0.0f, 0.0f)));
	renderStage->updateInstanceTransform(0, 3, (DirectX::XMMatrixTranslation(-200.0f, 0.0f, 0.0f)));
	DirectX::XMStoreFloat4x4(&perObjCB.data.World[4], DirectX::XMMatrixTranspose(DirectX::XMMatrixTranslation(200.0f, 0.0f, 0.0f)));
	renderStage->updateInstanceTransform(0, 4, (DirectX::XMMatrixTranslation(200.0f, 0.0f, 0.0f)));

	DirectX::XMStoreFloat4x4(&perMeshletObjCB.data.World[0], DirectX::XMMatrixTranspose(DirectX::XMMatrixTranslation(0.0f, 0.0f, 100.0f)));
	renderStage->updateMeshletTransform(0, (DirectX::XMMatrixTranslation(0.0f, 0.0f, 100.0f)));

	lightDataCB.data.numPointLights = 3;
	lightDataCB.data.lights[0].color = { 0.0f, 1.0f, 0.0f };
	lightDataCB.data.lights[0].pos = { 0.0f, 200.0f, 40.0f };
	lightDataCB.data.lights[0].strength = 800.0f;
	lightDataCB.data.lights[1].color = { 0.0f, 0.0f, 1.0f };
	lightDataCB.data.lights[1].pos = { -400.0f, 200.0f, 40.0f };
	lightDataCB.data.lights[1].strength = 800.0f;
	lightDataCB.data.lights[2].color = { 1.0f, 0.0f, 0.0f };
	lightDataCB.data.lights[2].pos = { 400.0f, 200.0f, 40.0f };
	lightDataCB.data.lights[2].strength = 800.0f;

	mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr);

	std::vector<AccelerationStructureBuffers> scratchBuffers;
	cpuWaitHandles.push_back(modelLoader->buildRTAccelerationStructureDeferred(mCommandList.Get(), scratchBuffers));

	BuildFrameResources();

	// All CPU side setup work must be somewhat complete to resolve the transitions between stages.
	WaitForMultipleObjects(cpuWaitHandles.size(), cpuWaitHandles.data(), TRUE, INFINITE);
	std::vector<CD3DX12_RESOURCE_BARRIER> initialTransitions = PipelineStage::setupResourceTransitions({ {renderStage}, {computeStage, deferStage}, {mergeStage}, {vrsComputeStage} });
	mCommandList->ResourceBarrier(initialTransitions.size(), initialTransitions.data());

	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	FlushCommandQueue();

	deferStage->importResource("TLAS", modelLoader->TLAS.Get());

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
	mCurrComputeFrameResource = mComputeFrameResources[mCurrFrameResourceIndex].get();

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
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdComputeListAlloc = mCurrComputeFrameResource->CmdListAlloc;

	cmdListAlloc->Reset();
	cmdComputeListAlloc->Reset();

	mCommandList->Reset(cmdListAlloc.Get(), defaultPSO.Get());
	// Compute doesn't really need a command list.

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
	eventHandles.push_back(modelLoader->updateRTAccelerationStructureDeferred(mCommandList.Get()));

	WaitForMultipleObjects(eventHandles.size(), eventHandles.data(), TRUE, INFINITE);

	PIXBeginEvent(mCommandList.Get(), PIX_COLOR(0.0, 0.0, 1.0), "Copy and Show");

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

	std::vector<ID3D12CommandList*> cmdList = { renderStage->mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(cmdList.size(), cmdList.data());
	mCommandQueue->Signal(mFence.Get(), ++mCurrentFence);


	cmdList = { computeStage->mCommandList.Get() };
	mComputeCommandQueue->Wait(mFence.Get(), mCurrentFence);
	mComputeCommandQueue->ExecuteCommandLists(cmdList.size(), cmdList.data());
	mComputeCommandQueue->Signal(mAuxFences[0].Get(), ++mCurrentAuxFence[0]);


	cmdList = { deferStage->mCommandList.Get() };
	mCommandQueue->Wait(mFence.Get(), mCurrentFence);
	mCommandQueue->ExecuteCommandLists(cmdList.size(), cmdList.data());
	mCommandQueue->Signal(mAuxFences[1].Get(), ++mCurrentAuxFence[1]);


	cmdList = { mergeStage->mCommandList.Get() };
	mCommandQueue->Wait(mAuxFences[0].Get(), mCurrentAuxFence[0]);
	mCommandQueue->Wait(mAuxFences[1].Get(), mCurrentAuxFence[1]);
	mCommandQueue->ExecuteCommandLists(cmdList.size(), cmdList.data());
	mCommandQueue->Signal(mFence.Get(), ++mCurrentFence);

	cmdList = { vrsComputeStage->mCommandList.Get() };
	mComputeCommandQueue->Wait(mFence.Get(), mCurrentFence);
	mComputeCommandQueue->ExecuteCommandLists(cmdList.size(), cmdList.data());
	mComputeCommandQueue->Signal(mFence.Get(), ++mCurrentFence);

	cmdList = { mCommandList.Get() };
	mCommandQueue->Wait(mFence.Get(), mCurrentFence);
	mCommandQueue->ExecuteCommandLists(cmdList.size(), cmdList.data());

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
		ImGui::SliderFloat("Shadow Step Size", &ssaoConstantCB.data.shadowStepSize, 0.0f, 10.0f);
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
		ImGui::SliderFloat3("Light Pos", (float*)&lightDataCB.data.lights[0].pos, -1000.0f, 1000.0f);
		ImGui::SliderFloat("Light Strength", &lightDataCB.data.lights[0].strength, 0.0f, 5000.0f);
		ImGui::ColorEdit3("Light Color", (float*)&lightDataCB.data.lights[0].color);
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
		mComputeFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(), D3D12_COMMAND_LIST_TYPE_COMPUTE));
		mComputeFrameResources[i].get()->CmdListAlloc->SetName((L"ComputeAlloc" + std::to_wstring(i)).c_str());
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
	ssaoConstantCB.data.viewProj = mainPassCB.data.viewProj;

	deferStage->deferUpdateConstantBuffer("SSAOConstants", ssaoConstantCB);

	lightDataCB.data.viewPos = mainPassCB.data.EyePosW;

	float waveHeight = 20.0f;
	float os = 0.3f;
	DirectX::XMStoreFloat4x4(&perObjCB.data.World[0], DirectX::XMMatrixTranspose(DirectX::XMMatrixTranslation(0.0f, (sin(mainPassCB.data.TotalTime) + 1.0f) * waveHeight, 0.0f)));
	renderStage->updateInstanceTransform(0, 0, (DirectX::XMMatrixTranslation(0.0f, (sin(mainPassCB.data.TotalTime) + 1.0f) * waveHeight, 0.0f)));
	DirectX::XMStoreFloat4x4(&perObjCB.data.World[1], DirectX::XMMatrixTranspose(DirectX::XMMatrixTranslation(-100.0f, (sin(mainPassCB.data.TotalTime - os) + 1.0f) * waveHeight, 0.0f)));
	renderStage->updateInstanceTransform(0, 1, (DirectX::XMMatrixTranslation(-100.0f, (sin(mainPassCB.data.TotalTime - os) + 1.0f) * waveHeight, 0.0f)));
	DirectX::XMStoreFloat4x4(&perObjCB.data.World[2], DirectX::XMMatrixTranspose(DirectX::XMMatrixTranslation(100.0f, (sin(mainPassCB.data.TotalTime + os) + 1.0f) * waveHeight, 0.0f)));
	renderStage->updateInstanceTransform(0, 2, (DirectX::XMMatrixTranslation(100.0f, (sin(mainPassCB.data.TotalTime + os) + 1.0f) * waveHeight, 0.0f)));
	DirectX::XMStoreFloat4x4(&perObjCB.data.World[3], DirectX::XMMatrixTranspose(DirectX::XMMatrixTranslation(-200.0f, (sin(mainPassCB.data.TotalTime - 2 * os) + 1.0f) * waveHeight, 0.0f)));
	renderStage->updateInstanceTransform(0, 3, (DirectX::XMMatrixTranslation(-200.0f, (sin(mainPassCB.data.TotalTime - 2 * os) + 1.0f) * waveHeight, 0.0f)));
	DirectX::XMStoreFloat4x4(&perObjCB.data.World[4], DirectX::XMMatrixTranspose(DirectX::XMMatrixTranslation(200.0f, (sin(mainPassCB.data.TotalTime + 2 * os) + 1.0f) * waveHeight, 0.0f)));
	renderStage->updateInstanceTransform(0, 4, (DirectX::XMMatrixTranslation(200.0f, (sin(mainPassCB.data.TotalTime + 2 * os) + 1.0f) * waveHeight, 0.0f)));

	vrsComputeStage->deferUpdateConstantBuffer("VrsConstants", vrsCB);
	deferStage->deferUpdateConstantBuffer("LightData", lightDataCB);
	renderStage->deferUpdateConstantBuffer("PerPassConstants", mainPassCB);
	renderStage->deferUpdateConstantBuffer("PerObjectConstants", perObjCB);
	renderStage->deferUpdateConstantBuffer("PerObjectConstantsMeshlet", perMeshletObjCB);
	if (!freezeCull) {
		renderStage->frustrum = DirectX::BoundingFrustum(proj);
		renderStage->frustrum.Transform(renderStage->frustrum, invView);
	}
	renderStage->eyePos = DirectX::XMFLOAT3(eyePos.x, eyePos.y, eyePos.z);
	renderStage->VRS = VRS;
	deferStage->VRS = VRS;
}