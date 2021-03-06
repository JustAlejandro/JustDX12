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
#include "ThreadPool.h"
#include "PipelineStage/ComputePipelineStage.h"
#include "PipelineStage\RenderPipelineStage.h"
#include "ScreenRenderPipelineStage.h"
#include "RtRenderPipelineStage.h"
#include "ModelRenderPipelineStage.h"
#include "MeshletRenderPipelineStage.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include "KeyboardWrapper.h"
#include "ModelLoading/TextureLoader.h"
#include "SceneCsv.h"
#include "FileSelect.h"

#include <random>
#include <ctime>
#include <ResourceDecay.h>

std::string baseDir = "..\\Models";

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
	struct ModelData {
		std::string name;
		std::vector<InstanceData> instances;
		std::weak_ptr<Model> model;
	};

	void BuildFrameResources();

	void loadScene(SceneCsv scene);
	void unloadScene(std::string fileName);
	void updateSceneClass(SceneCsv& scene);

	void ApplyModelDataUpdate(ModelData* data);
	void loadModel(std::string name, std::string fileName, std::string dirName, std::vector<InstanceData> instances);
	void unloadModel(std::string name);


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

	std::vector<SceneCsv> loadedScenes;

	bool showImgui = true;
	bool freezeCull = false;
	bool VRS = true;
	bool renderVRS = false;
	bool occlusionCull = true;
	std::deque<float> frametime;
	std::deque<float> cpuFrametime;

	std::unique_ptr<ComputePipelineStage> computeStage = nullptr;
	std::unique_ptr<ComputePipelineStage> hBlurStage = nullptr;
	std::unique_ptr<ComputePipelineStage> vBlurStage = nullptr;
	std::unique_ptr<ComputePipelineStage> vrsComputeStage = nullptr;
	std::unique_ptr<ModelRenderPipelineStage> renderStage = nullptr;
	std::unique_ptr<MeshletRenderPipelineStage> meshletStage = nullptr;
	std::unique_ptr<RtRenderPipelineStage> deferStage = nullptr;
	std::unique_ptr<ScreenRenderPipelineStage> mergeStage = nullptr;
	KeyboardWrapper keyboard;

	PerPassConstants mainPassCB;
	PerObjectConstants perObjCB;
	PerObjectConstants perMeshletObjCB;
	VrsConstants vrsCB;
	SSAOConstants ssaoConstantCB;
	LightData lightDataCB;

	std::unordered_map<std::string, ModelData> activeModels;

	DirectX::XMFLOAT4 eyePos = { -10000.0f,2500.0f,7000.0f, 1.0f };
	DirectX::XMFLOAT4X4 view = Identity();
	DirectX::XMFLOAT4X4 proj = Identity();

	float lookAngle[3] = { 0.0f, DirectX::XM_PI, 0.0f };

	float mTheta = 1.3f * DirectX::XM_PI;
	float mPhi = 0.4f * DirectX::XM_PI;
	float mRadius = 10.0f;

	POINT lastMousePos;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd) {
#if defined(DEBUG) | defined(_DEBUG) || GPU_DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	Microsoft::WRL::ComPtr<ID3D12Device> debugDev;
	try {
		DemoApp app(hInstance);
		if (!app.initialize())
			return 0;
		debugDev = app.getDevice();
		// Loop
		app.run();
	}
	catch (const HrException& hrEx) {
		MessageBoxA(nullptr, hrEx.what(), "HR Exception", MB_OK);
	}
	catch (const std::string& ex) {
		MessageBoxA(nullptr, ex.c_str(), "String Exception", MB_OK);
	}
	catch (...) {
		MessageBoxA(nullptr, "Not Sure What", "Oof, something broke.", MB_OK);
		return 0;
	}
#if false
	ID3D12DebugDevice* debugInterface;
	debugDev->QueryInterface(&debugInterface);
	debugInterface->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
	debugInterface->Release();
#endif // DEBUG
	return 0;
}

DemoApp::DemoApp(HINSTANCE hInstance) : DX12App(hInstance) {
}

DemoApp::~DemoApp() {
	if (md3dDevice != nullptr)
		flushCommandQueue();

	ModelLoader::getInstance().clearQueue();
	WaitForSingleObject(ModelLoader::getInstance().deferSetCpuEvent(), INFINITE);
	std::vector<HANDLE> threadsOngoing = ThreadPool::prepareQuit();
	WaitForMultipleObjects(threadsOngoing.size(), threadsOngoing.data(), true, INFINITE);
	// Have to explicitly call ModelLoader clear first since it dumps resources into ResourceDecay.
	ModelLoader::destroyAll();
	ResourceDecay::destroyAll();
	TextureLoader::getInstance().destroyAll();
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

bool DemoApp::initialize() {
	if (!DX12App::initialize()) {
		return false;
	}

	ModelLoader& modelLoader = ModelLoader::getInstance();
	std::vector<HANDLE> cpuWaitHandles;
	// Create Render stage first because of dependencies later.
	{
		PipeLineStageDesc rasterDesc;
		rasterDesc.name = "Forward Pass";

		rasterDesc.constantBufferJobs.push_back(ConstantBufferJob("PerPassConstants", new PerPassConstants(), 0));

		rasterDesc.descriptorJobs.push_back(DescriptorJob("albedoDesc", "albedo", DESCRIPTOR_TYPE_RTV));
		rasterDesc.descriptorJobs.push_back(DescriptorJob("specularDesc", "specular", DESCRIPTOR_TYPE_RTV));
		rasterDesc.descriptorJobs.push_back(DescriptorJob("normalDesc", "normal", DESCRIPTOR_TYPE_RTV));
		rasterDesc.descriptorJobs.push_back(DescriptorJob("tangentDesc", "tangent", DESCRIPTOR_TYPE_RTV));
		rasterDesc.descriptorJobs.push_back(DescriptorJob("emissiveDesc", "emissive", DESCRIPTOR_TYPE_RTV));
		rasterDesc.descriptorJobs.push_back(DescriptorJob("depthStencilView", "depthTex", DESCRIPTOR_TYPE_DSV, false));
		rasterDesc.descriptorJobs.back().view.dsvDesc = DEFAULT_DSV_DESC();

		rasterDesc.resourceJobs.push_back(ResourceJob("VRS", DESCRIPTOR_TYPE_UAV, DXGI_FORMAT_R8_UINT, DivRoundUp(gScreenHeight, vrsSupport.ShadingRateImageTileSize), DivRoundUp(gScreenWidth, vrsSupport.ShadingRateImageTileSize)));
		rasterDesc.resourceJobs.push_back(ResourceJob("albedo", DESCRIPTOR_TYPE_SRV | DESCRIPTOR_TYPE_RTV | DESCRIPTOR_TYPE_FLAG_SIMULTANEOUS_ACCESS));
		rasterDesc.resourceJobs.push_back(ResourceJob("specular", DESCRIPTOR_TYPE_SRV | DESCRIPTOR_TYPE_RTV | DESCRIPTOR_TYPE_FLAG_SIMULTANEOUS_ACCESS));
		rasterDesc.resourceJobs.push_back(ResourceJob("normal", DESCRIPTOR_TYPE_SRV | DESCRIPTOR_TYPE_RTV | DESCRIPTOR_TYPE_FLAG_SIMULTANEOUS_ACCESS));
		rasterDesc.resourceJobs.push_back(ResourceJob("tangent", DESCRIPTOR_TYPE_SRV | DESCRIPTOR_TYPE_RTV | DESCRIPTOR_TYPE_FLAG_SIMULTANEOUS_ACCESS));
		rasterDesc.resourceJobs.push_back(ResourceJob("emissive", DESCRIPTOR_TYPE_SRV | DESCRIPTOR_TYPE_RTV | DESCRIPTOR_TYPE_FLAG_SIMULTANEOUS_ACCESS));
		rasterDesc.resourceJobs.push_back(ResourceJob("depthTex", DESCRIPTOR_TYPE_SRV | DESCRIPTOR_TYPE_DSV, DEPTH_TEXTURE_FORMAT));

		rasterDesc.rootSigDesc.push_back(RootParamDesc("PerObjectConstants", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER, 0, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, DESCRIPTOR_USAGE_SYSTEM_DEFINED));
		rasterDesc.rootSigDesc.push_back(RootParamDesc("PerMeshConstants", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER, 1, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, DESCRIPTOR_USAGE_SYSTEM_DEFINED));
		rasterDesc.rootSigDesc.push_back(RootParamDesc("texture_diffuse", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 2, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, DESCRIPTOR_USAGE_SYSTEM_DEFINED));
		rasterDesc.rootSigDesc.push_back(RootParamDesc("PerPassConstants", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER, 3, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, DESCRIPTOR_USAGE_PER_PASS));

		std::vector<DXDefine> defines;
		defines.push_back(DXDefine(L"VRS", L""));
		if (vrsSupport.AdditionalShadingRatesSupported == true) {
			defines.push_back(DXDefine(L"VRS_4X4", L""));
		}

		rasterDesc.shaderFiles.push_back(ShaderDesc("Default.hlsl", "Vertex Shader", "VS", SHADER_TYPE_VS, defines));
		rasterDesc.shaderFiles.push_back(ShaderDesc("Default.hlsl", "Pixel Shader", "PS", SHADER_TYPE_PS, defines));

		rasterDesc.textureFiles.push_back(std::make_pair("default_normal", "default_bump.dds"));
		rasterDesc.textureFiles.push_back(std::make_pair("default_spec", "default_spec_pbr.dds"));
		rasterDesc.textureFiles.push_back(std::make_pair("default_diff", "test_tex.dds"));
		rasterDesc.textureFiles.push_back(std::make_pair("default_emissive", "default_emissive.dds"));

		RenderPipelineDesc rDesc;
		rDesc.renderTargets.push_back(RenderTargetDesc("albedoDesc", 0));
		rDesc.renderTargets.push_back(RenderTargetDesc("specularDesc", 0));
		rDesc.renderTargets.push_back(RenderTargetDesc("normalDesc", 0));
		rDesc.renderTargets.push_back(RenderTargetDesc("tangentDesc", 0));
		rDesc.renderTargets.push_back(RenderTargetDesc("emissiveDesc", 0));
		rDesc.perObjTransformCBSlot = 0;
		rDesc.perMeshTransformCBSlot = 1;
		rDesc.perMeshTextureSlot = 2;
		rDesc.supportsCulling = true;
		rDesc.supportsVRS = true;

		rDesc.defaultTextures[MODEL_FORMAT_DIFFUSE_TEX] = "default_diff";
		rDesc.defaultTextures[MODEL_FORMAT_SPECULAR_TEX] = "default_spec";
		rDesc.defaultTextures[MODEL_FORMAT_NORMAL_TEX] = "default_normal";
		rDesc.defaultTextures[MODEL_FORMAT_EMMISIVE_TEX] = "default_emissive";

		rDesc.textureToDescriptor.emplace_back(MODEL_FORMAT_DIFFUSE_TEX, "texture_diffuse");
		rDesc.textureToDescriptor.emplace_back(MODEL_FORMAT_SPECULAR_TEX, "texture_spec");
		rDesc.textureToDescriptor.emplace_back(MODEL_FORMAT_NORMAL_TEX, "texture_normal");
		rDesc.textureToDescriptor.emplace_back(MODEL_FORMAT_EMMISIVE_TEX, "texture_emissive");

		renderStage = std::make_unique<ModelRenderPipelineStage>(md3dDevice, rDesc, DEFAULT_VIEW_PORT(), mScissorRect);
		renderStage->deferSetup(rasterDesc);
		WaitOnFenceForever(renderStage->getFence(), renderStage->triggerFence());
	}
	cpuWaitHandles.push_back(renderStage->deferSetCpuEvent());
	// Seperate pass for Meshlet rendering.
	{
		PipeLineStageDesc rasterDesc;
		rasterDesc.name = "Meshlet Forward Pass";

		rasterDesc.constantBufferJobs.push_back(ConstantBufferJob("PerObjectConstantsMeshlet", new PerObjectConstants(), 0));

		rasterDesc.externalConstantBuffers.push_back(std::make_pair(IndexedName("PerPassConstants", 0), renderStage->getConstantBuffer(IndexedName("PerPassConstants", 0))));

		rasterDesc.externalResources.push_back(std::make_pair("depthTex", renderStage->getResource("depthTex")));
		rasterDesc.externalResources.push_back(std::make_pair("albedo", renderStage->getResource("albedo")));
		rasterDesc.externalResources.push_back(std::make_pair("specular", renderStage->getResource("specular")));
		rasterDesc.externalResources.push_back(std::make_pair("normal", renderStage->getResource("normal")));
		rasterDesc.externalResources.push_back(std::make_pair("tangent", renderStage->getResource("tangent")));
		rasterDesc.externalResources.push_back(std::make_pair("emissive", renderStage->getResource("emissive")));
		rasterDesc.externalResources.push_back(std::make_pair("VRS", renderStage->getResource("VRS")));

		rasterDesc.descriptorJobs.push_back(DescriptorJob("albedoDesc", "albedo", DESCRIPTOR_TYPE_RTV));
		rasterDesc.descriptorJobs.push_back(DescriptorJob("specularDesc", "specular", DESCRIPTOR_TYPE_RTV));
		rasterDesc.descriptorJobs.push_back(DescriptorJob("normalDesc", "normal", DESCRIPTOR_TYPE_RTV));
		rasterDesc.descriptorJobs.push_back(DescriptorJob("tangentDesc", "tangent", DESCRIPTOR_TYPE_RTV));
		rasterDesc.descriptorJobs.push_back(DescriptorJob("emissiveDesc", "emissive", DESCRIPTOR_TYPE_RTV));
		rasterDesc.descriptorJobs.push_back(DescriptorJob("depthStencilView", "depthTex", DESCRIPTOR_TYPE_DSV, false));
		rasterDesc.descriptorJobs.back().view.dsvDesc = DEFAULT_DSV_DESC();

		// Have to generate meshlet root signature
		rasterDesc.rootSigDesc.push_back(RootParamDesc("MeshInfo", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER, 0, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, DESCRIPTOR_USAGE_PER_MESHLET));
		rasterDesc.rootSigDesc.push_back(RootParamDesc("Vertices", ROOT_PARAMETER_TYPE_SRV, 1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, DESCRIPTOR_USAGE_PER_MESHLET));
		rasterDesc.rootSigDesc.push_back(RootParamDesc("Meshlets", ROOT_PARAMETER_TYPE_SRV, 2, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, DESCRIPTOR_USAGE_PER_MESHLET));
		rasterDesc.rootSigDesc.push_back(RootParamDesc("UniqueVertexIndices", ROOT_PARAMETER_TYPE_SRV, 3, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, DESCRIPTOR_USAGE_PER_MESHLET));
		rasterDesc.rootSigDesc.push_back(RootParamDesc("PrimitiveIndices", ROOT_PARAMETER_TYPE_SRV, 4, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, DESCRIPTOR_USAGE_PER_MESHLET));
		rasterDesc.rootSigDesc.push_back(RootParamDesc("MeshletCullData", ROOT_PARAMETER_TYPE_SRV, 5, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, DESCRIPTOR_USAGE_PER_MESHLET));
		rasterDesc.rootSigDesc.push_back(RootParamDesc("PerObjectConstantsMeshlet", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER, 6, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, DESCRIPTOR_USAGE_PER_OBJECT));
		rasterDesc.rootSigDesc.push_back(RootParamDesc("mesh_texture_diffuse", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 7, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, DESCRIPTOR_USAGE_PER_OBJECT));
		rasterDesc.rootSigDesc.push_back(RootParamDesc("PerPassConstants", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER, 8, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, DESCRIPTOR_USAGE_PER_PASS));

		std::vector<DXDefine> defines;
		defines.push_back(DXDefine(L"VRS", L""));
		if (vrsSupport.AdditionalShadingRatesSupported == true) {
			defines.push_back(DXDefine(L"VRS_4X4", L""));
		}

		rasterDesc.shaderFiles.push_back(ShaderDesc("MeshletAS.hlsl", "Amplification Shader", "AS", SHADER_TYPE_AS, {}));
		rasterDesc.shaderFiles.push_back(ShaderDesc("MeshletMS.hlsl", "Mesh Shader", "MS", SHADER_TYPE_MS, {}));
		rasterDesc.shaderFiles.push_back(ShaderDesc("MeshletMS.hlsl", "Mesh Pixel Shader", "PS", SHADER_TYPE_PS, {}));

		rasterDesc.textureFiles.push_back(std::make_pair("default_normal", "default_bump.dds"));
		rasterDesc.textureFiles.push_back(std::make_pair("default_spec", "default_spec_pbr.dds"));
		rasterDesc.textureFiles.push_back(std::make_pair("default_diff", "test_tex.dds"));
		rasterDesc.textureFiles.push_back(std::make_pair("default_emissive", "default_emissive.dds"));
		rasterDesc.textureFiles.push_back(std::make_pair("default_emissive", "default_emissive.dds"));

		RenderPipelineDesc rDesc;
		rDesc.renderTargets.push_back(RenderTargetDesc("albedoDesc", 0));
		rDesc.renderTargets.push_back(RenderTargetDesc("specularDesc", 0));
		rDesc.renderTargets.push_back(RenderTargetDesc("normalDesc", 0));
		rDesc.renderTargets.push_back(RenderTargetDesc("tangentDesc", 0));
		rDesc.renderTargets.push_back(RenderTargetDesc("emissiveDesc", 0));
		rDesc.perObjTransformCBSlot = 6;
		rDesc.perMeshTransformCBSlot = -1;
		rDesc.perMeshTextureSlot = 7;
		rDesc.clearDepthTex = false;
		rDesc.clearRenderTargets = false;
		rDesc.supportsCulling = true;
		rDesc.supportsVRS = true;

		rDesc.defaultTextures[MODEL_FORMAT_DIFFUSE_TEX] = "default_diff";
		rDesc.defaultTextures[MODEL_FORMAT_SPECULAR_TEX] = "default_spec";
		rDesc.defaultTextures[MODEL_FORMAT_NORMAL_TEX] = "default_normal";
		rDesc.defaultTextures[MODEL_FORMAT_EMMISIVE_TEX] = "default_emissive";

		rDesc.textureToDescriptor.emplace_back(MODEL_FORMAT_DIFFUSE_TEX, "mesh_texture_diffuse");
		rDesc.textureToDescriptor.emplace_back(MODEL_FORMAT_SPECULAR_TEX, "mesh_texture_specular");
		rDesc.textureToDescriptor.emplace_back(MODEL_FORMAT_NORMAL_TEX, "mesh_texture_normal");
		rDesc.textureToDescriptor.emplace_back(MODEL_FORMAT_EMMISIVE_TEX, "texture_emissive");

		meshletStage = std::make_unique<MeshletRenderPipelineStage>(md3dDevice, rDesc, DEFAULT_VIEW_PORT(), mScissorRect);
		meshletStage->deferSetup(rasterDesc);
		WaitOnFenceForever(meshletStage->getFence(), meshletStage->triggerFence());
	}
	cpuWaitHandles.push_back(meshletStage->deferSetCpuEvent());
	// Perform deferred shading.
	{
		std::vector<DXDefine> defines = {
			DXDefine(L"MAX_LIGHTS", std::to_wstring(MAX_LIGHTS)),
			DXDefine(L"RT_SUPPORT", std::to_wstring((int)supportsRt()))
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
		stageDesc.descriptorJobs.push_back(DescriptorJob("emissiveTexDesc", "emissiveTex", DESCRIPTOR_TYPE_SRV));
		stageDesc.descriptorJobs.push_back(DescriptorJob("deferTexDesc", "deferTex", DESCRIPTOR_TYPE_RTV));
		stageDesc.descriptorJobs.push_back(DescriptorJob("brdfLutDesc", "brdfLut", DESCRIPTOR_TYPE_SRV));

		stageDesc.externalConstantBuffers.push_back(std::make_pair(IndexedName("PerPassConstants", 0), renderStage->getConstantBuffer(IndexedName("PerPassConstants", 0))));

		stageDesc.externalResources.push_back(std::make_pair("renderDepthTex", renderStage->getResource("depthTex")));
		stageDesc.externalResources.push_back(std::make_pair("colorTex", renderStage->getResource("albedo")));
		stageDesc.externalResources.push_back(std::make_pair("specularTex", renderStage->getResource("specular")));
		stageDesc.externalResources.push_back(std::make_pair("normalTex", renderStage->getResource("normal")));
		stageDesc.externalResources.push_back(std::make_pair("tangentTex", renderStage->getResource("tangent")));
		stageDesc.externalResources.push_back(std::make_pair("emissiveTex", renderStage->getResource("emissive")));
		stageDesc.externalResources.push_back(std::make_pair("VRS", renderStage->getResource("VRS")));

		stageDesc.resourceJobs.push_back(ResourceJob("deferTex", DESCRIPTOR_TYPE_RTV, COLOR_TEXTURE_FORMAT));

		stageDesc.rootSigDesc.push_back(RootParamDesc("LightData", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER, 0, D3D12_DESCRIPTOR_RANGE_TYPE_CBV));
		stageDesc.rootSigDesc.push_back(RootParamDesc("SSAOConstants", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER, 1, D3D12_DESCRIPTOR_RANGE_TYPE_CBV));
		stageDesc.rootSigDesc.push_back(RootParamDesc("PerPassConstants", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER, 2, D3D12_DESCRIPTOR_RANGE_TYPE_CBV));
		stageDesc.rootSigDesc.push_back(RootParamDesc("inputDepth", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 3, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6));
		stageDesc.rootSigDesc.push_back(RootParamDesc("TLAS", ROOT_PARAMETER_TYPE_SRV, 4));
		stageDesc.rootSigDesc.push_back(RootParamDesc("IndexBuffers", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 5, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, -1, DESCRIPTOR_USAGE_ALL, 1));
		stageDesc.rootSigDesc.push_back(RootParamDesc("VertexBuffers", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 6, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, -1, DESCRIPTOR_USAGE_ALL, 2));
		stageDesc.rootSigDesc.push_back(RootParamDesc("Textures", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 7, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, -1, DESCRIPTOR_USAGE_ALL, 3));
		stageDesc.rootSigDesc.push_back(RootParamDesc("Transforms", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 8, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, -1, DESCRIPTOR_USAGE_ALL, 4));
		stageDesc.rootSigDesc.push_back(RootParamDesc("brdfLutDesc", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 9, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, DESCRIPTOR_USAGE_ALL, 5));

		stageDesc.shaderFiles.push_back(ShaderDesc("DeferShading.hlsl", "Defer Shader VS", "DeferVS", SHADER_TYPE_VS, defines));
		stageDesc.shaderFiles.push_back(ShaderDesc("DeferShading.hlsl", "Defer Shader PS", "DeferPS", SHADER_TYPE_PS, defines));

		stageDesc.textureFiles.push_back(std::make_pair("brdfLut", "BrdfLut.dds"));
		stageDesc.textureFiles.push_back(std::make_pair("default_normal", "default_bump.dds"));
		stageDesc.textureFiles.push_back(std::make_pair("default_spec", "default_spec_pbr.dds"));
		stageDesc.textureFiles.push_back(std::make_pair("default_diff", "test_tex.dds"));
		stageDesc.textureFiles.push_back(std::make_pair("default_emissive", "default_emissive.dds"));

		RenderPipelineDesc mergeRDesc;
		mergeRDesc.renderTargets.push_back(RenderTargetDesc("deferTexDesc", 0));
		mergeRDesc.supportsVRS = true;
		mergeRDesc.supportsCulling = false;
		mergeRDesc.usesDepthTex = false;

		mergeRDesc.defaultTextures[MODEL_FORMAT_DIFFUSE_TEX] = "default_diff";
		mergeRDesc.defaultTextures[MODEL_FORMAT_SPECULAR_TEX] = "default_spec";
		mergeRDesc.defaultTextures[MODEL_FORMAT_NORMAL_TEX] = "default_normal";
		mergeRDesc.defaultTextures[MODEL_FORMAT_EMMISIVE_TEX] = "default_emissive";

		mergeRDesc.textureToDescriptor.emplace_back(MODEL_FORMAT_DIFFUSE_TEX, "texture_diffuse");
		mergeRDesc.textureToDescriptor.emplace_back(MODEL_FORMAT_SPECULAR_TEX, "texture_spec");
		mergeRDesc.textureToDescriptor.emplace_back(MODEL_FORMAT_NORMAL_TEX, "texture_normal");
		mergeRDesc.textureToDescriptor.emplace_back(MODEL_FORMAT_EMMISIVE_TEX, "texture_emissive");

		RtRenderPipelineStageDesc rtDesc;
		rtDesc.rtTlasSlot = 4;
		rtDesc.tlasPtr = modelLoader.TLAS.GetAddressOf();
		rtDesc.rtIndexBufferSlot = 5;
		rtDesc.rtVertexBufferSlot = 6;
		rtDesc.rtTexturesSlot = 7;
		rtDesc.rtTransformCbvSlot = 8;

		deferStage = std::make_unique<RtRenderPipelineStage>(md3dDevice, rtDesc, mergeRDesc, DEFAULT_VIEW_PORT(), mScissorRect);
		deferStage->deferSetup(stageDesc);
		WaitOnFenceForever(deferStage->getFence(), deferStage->triggerFence());
		deferStage->frustrumCull = false;
	}
	cpuWaitHandles.push_back(deferStage->deferSetCpuEvent());
	// Create SSAO/Screen Space Shadow Pass.
	{
		std::vector<DXDefine> defines = {
			DXDefine(L"MAX_LIGHTS", std::to_wstring(MAX_LIGHTS))
		};

		PipeLineStageDesc stageDesc;
		stageDesc.name = "SSAO/Shadows";

		stageDesc.descriptorJobs.push_back(DescriptorJob("noise_texDesc", "noise_tex", DESCRIPTOR_TYPE_SRV));
		stageDesc.descriptorJobs.push_back(DescriptorJob("inputDepth", "renderOutputTex", DESCRIPTOR_TYPE_SRV, false));
		stageDesc.descriptorJobs.back().view.srvDesc = DEFAULT_SRV_DESC();
		stageDesc.descriptorJobs.back().view.srvDesc.Format = DEPTH_TEXTURE_SRV_FORMAT;
		stageDesc.descriptorJobs.push_back(DescriptorJob("normalTex", "renderOutputNormals", DESCRIPTOR_TYPE_SRV));
		stageDesc.descriptorJobs.push_back(DescriptorJob("tangentTex", "renderOutputTangents", DESCRIPTOR_TYPE_SRV));
		stageDesc.descriptorJobs.push_back(DescriptorJob("SSAOOut", "SSAOOutTexture", DESCRIPTOR_TYPE_UAV));

		stageDesc.externalConstantBuffers.push_back(std::make_pair(IndexedName("LightData", 0), deferStage->getConstantBuffer(IndexedName("LightData", 0))));
		stageDesc.externalConstantBuffers.push_back(std::make_pair(IndexedName("SSAOConstants", 0), deferStage->getConstantBuffer(IndexedName("SSAOConstants", 0))));
		stageDesc.externalConstantBuffers.push_back(std::make_pair(IndexedName("PerPassConstants", 0), renderStage->getConstantBuffer(IndexedName("PerPassConstants", 0))));

		stageDesc.externalResources.push_back(std::make_pair("renderOutputTex", renderStage->getResource("depthTex")));
		stageDesc.externalResources.push_back(std::make_pair("renderOutputColor", renderStage->getResource("albedo")));
		stageDesc.externalResources.push_back(std::make_pair("renderOutputSpec", renderStage->getResource("specular")));
		stageDesc.externalResources.push_back(std::make_pair("renderOutputNormals", renderStage->getResource("normal")));
		stageDesc.externalResources.push_back(std::make_pair("renderOutputTangents", renderStage->getResource("tangent")));

		stageDesc.resourceJobs.push_back(ResourceJob("SSAOOutTexture", DESCRIPTOR_TYPE_UAV, DXGI_FORMAT_R8_UNORM));

		stageDesc.rootSigDesc.push_back(RootParamDesc("LightData", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER, 0, D3D12_DESCRIPTOR_RANGE_TYPE_CBV));
		stageDesc.rootSigDesc.push_back(RootParamDesc("SSAOConstants", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER, 1, D3D12_DESCRIPTOR_RANGE_TYPE_CBV));
		stageDesc.rootSigDesc.push_back(RootParamDesc("PerPassConstants", ROOT_PARAMETER_TYPE_CONSTANT_BUFFER, 2, D3D12_DESCRIPTOR_RANGE_TYPE_CBV));
		stageDesc.rootSigDesc.push_back(RootParamDesc("noise_texDesc", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 3, D3D12_DESCRIPTOR_RANGE_TYPE_SRV));
		stageDesc.rootSigDesc.push_back(RootParamDesc("inputDepth", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 4, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4));
		stageDesc.rootSigDesc.push_back(RootParamDesc("SSAOOut", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 5, D3D12_DESCRIPTOR_RANGE_TYPE_UAV));

		stageDesc.shaderFiles.push_back(ShaderDesc("SSAO.hlsl", "SSAO", "SSAO", SHADER_TYPE_CS, defines));

		stageDesc.textureFiles.push_back(std::make_pair("noise_tex", "default_noise.dds"));

		ComputePipelineDesc cDesc;
		cDesc.groupCount[0] = (UINT)ceilf(gScreenWidth / 8.0f);
		cDesc.groupCount[1] = (UINT)ceilf(gScreenHeight / 8.0f);
		cDesc.groupCount[2] = 1;
		computeStage = std::make_unique<ComputePipelineStage>(md3dDevice, cDesc);
		computeStage->deferSetup(stageDesc);
		WaitOnFenceForever(computeStage->getFence(), computeStage->triggerFence());
	}
	cpuWaitHandles.push_back(computeStage->deferSetCpuEvent());
	// HBlur SSAO Pass
	{
		PipeLineStageDesc desc;
		desc.name = "Blur";

		desc.descriptorJobs.push_back(DescriptorJob("SSAOOutDesc", "SSAOOutTexture", DESCRIPTOR_TYPE_SRV));
		desc.descriptorJobs.push_back(DescriptorJob("HBlurredSSAODesc", "HBlurredSSAOTexture", DESCRIPTOR_TYPE_UAV));

		desc.externalResources.push_back(std::make_pair("SSAOOutTexture", computeStage->getResource("SSAOOutTexture")));

		desc.resourceJobs.push_back(ResourceJob("HBlurredSSAOTexture", DESCRIPTOR_TYPE_UAV | DESCRIPTOR_TYPE_SRV, DXGI_FORMAT_R8_UNORM));

		desc.rootSigDesc.push_back(RootParamDesc("SSAOOutDesc", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 0));
		desc.rootSigDesc.push_back(RootParamDesc("HBlurredSSAODesc", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 1, D3D12_DESCRIPTOR_RANGE_TYPE_UAV));

		desc.shaderFiles.push_back(ShaderDesc("Blur.hlsl", "Horizontal Blur", "HBlur", SHADER_TYPE_CS, { DXDefine(L"HBLUR",L"") }));

		ComputePipelineDesc cDesc;
		cDesc.groupCount[0] = (UINT)ceilf(gScreenWidth / 32.0f);
		cDesc.groupCount[1] = (UINT)ceilf(gScreenHeight);
		cDesc.groupCount[2] = 1;
		hBlurStage = std::make_unique<ComputePipelineStage>(md3dDevice, cDesc);
		hBlurStage->deferSetup(desc);
		WaitOnFenceForever(hBlurStage->getFence(), hBlurStage->triggerFence());
	}
	cpuWaitHandles.push_back(hBlurStage->deferSetCpuEvent());
	// VBlur SSAO Pass
	{
		PipeLineStageDesc desc;
		desc.name = "Blur";

		desc.descriptorJobs.push_back(DescriptorJob("HBlurredSSAODesc", "HBlurredSSAOTexture", DESCRIPTOR_TYPE_UAV));
		desc.descriptorJobs.push_back(DescriptorJob("FullBlurredSSAODesc", "FullBlurredSSAOTexture", DESCRIPTOR_TYPE_UAV));

		desc.externalResources.push_back(std::make_pair("HBlurredSSAOTexture", hBlurStage->getResource("HBlurredSSAOTexture")));

		desc.resourceJobs.push_back(ResourceJob("FullBlurredSSAOTexture", DESCRIPTOR_TYPE_UAV | DESCRIPTOR_TYPE_SRV, DXGI_FORMAT_R8_UNORM));

		desc.rootSigDesc.push_back(RootParamDesc("HBlurredSSAODesc", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 0, D3D12_DESCRIPTOR_RANGE_TYPE_UAV));
		desc.rootSigDesc.push_back(RootParamDesc("FullBlurredSSAODesc", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 1, D3D12_DESCRIPTOR_RANGE_TYPE_UAV));

		desc.shaderFiles.push_back(ShaderDesc("Blur.hlsl", "Vertical Blur", "VBlur", SHADER_TYPE_CS, { DXDefine(L"VBLUR",L"") }));

		ComputePipelineDesc cDesc;
		cDesc.groupCount[0] = (UINT)ceilf(gScreenWidth);
		cDesc.groupCount[1] = (UINT)ceilf(gScreenHeight / 32.0f);
		cDesc.groupCount[2] = 1;
		vBlurStage = std::make_unique<ComputePipelineStage>(md3dDevice, cDesc);
		vBlurStage->deferSetup(desc);
		WaitOnFenceForever(vBlurStage->getFence(), vBlurStage->triggerFence());
	}
	cpuWaitHandles.push_back(vBlurStage->deferSetCpuEvent());
	// Merge deferred shading with SSAO/Shadows
	{
		PipeLineStageDesc stageDesc;
		stageDesc.name = "Merge";
		stageDesc.descriptorJobs.push_back(DescriptorJob("SSAOTexDesc", "SSAOTex", DESCRIPTOR_TYPE_SRV));
		stageDesc.descriptorJobs.push_back(DescriptorJob("colorTexDesc", "colorTex", DESCRIPTOR_TYPE_SRV));
		stageDesc.descriptorJobs.push_back(DescriptorJob("inputDepth", "renderDepthTex", DESCRIPTOR_TYPE_SRV, false));
		stageDesc.descriptorJobs.back().view.srvDesc = DEFAULT_SRV_DESC();
		stageDesc.descriptorJobs.back().view.srvDesc.Format = DEPTH_TEXTURE_SRV_FORMAT;
		stageDesc.descriptorJobs.push_back(DescriptorJob("mergedTexDesc", "mergedTex", DESCRIPTOR_TYPE_RTV));

		stageDesc.externalResources.push_back(std::make_pair("renderDepthTex", renderStage->getResource("depthTex")));
		stageDesc.externalResources.push_back(std::make_pair("colorTex", deferStage->getResource("deferTex")));
		stageDesc.externalResources.push_back(std::make_pair("SSAOTex", vBlurStage->getResource("FullBlurredSSAOTexture")));

		stageDesc.resourceJobs.push_back(ResourceJob("mergedTex", DESCRIPTOR_TYPE_RTV, COLOR_TEXTURE_FORMAT));

		stageDesc.rootSigDesc.push_back(RootParamDesc("SSAOTexDesc", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2));
		stageDesc.rootSigDesc.push_back(RootParamDesc("inputDepth", ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1));

		stageDesc.shaderFiles.push_back(ShaderDesc("Merge.hlsl", "Merge Shader VS", "MergeVS", SHADER_TYPE_VS, { DXDefine(L"MAX_LIGHTS", std::to_wstring(MAX_LIGHTS)) }));
		stageDesc.shaderFiles.push_back(ShaderDesc("Merge.hlsl", "Merge Shader PS", "MergePS", SHADER_TYPE_PS, { DXDefine(L"MAX_LIGHTS", std::to_wstring(MAX_LIGHTS)) }));

		RenderPipelineDesc rDesc;
		rDesc.renderTargets.push_back(RenderTargetDesc("mergedTexDesc", 0));
		rDesc.supportsCulling = false;
		rDesc.supportsVRS = false;
		rDesc.usesDepthTex = false;

		mergeStage = std::make_unique<ScreenRenderPipelineStage>(md3dDevice, rDesc, DEFAULT_VIEW_PORT(), mScissorRect);
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
		cDesc.groupCount[0] = DivRoundUp(gScreenWidth, vrsSupport.ShadingRateImageTileSize);
		cDesc.groupCount[1] = DivRoundUp(gScreenHeight, vrsSupport.ShadingRateImageTileSize);
		cDesc.groupCount[2] = 1;

		vrsComputeStage = std::make_unique<ComputePipelineStage>(md3dDevice, cDesc);
		vrsComputeStage->deferSetup(stageDesc);
	}
	cpuWaitHandles.push_back(vrsComputeStage->deferSetCpuEvent());
	//renderStage->loadMeshletModel(modelLoader, armorMeshlet, armorDir, true);

	SceneCsv scene("blankScene.csv", baseDir);
	loadScene(scene);
	loadedScenes.push_back(scene);

	// Have to have a copy of the armor file loaded so the meshlet copy can use it for a BLAS
	//modelLoader->loadModel(armorFile, armorDir, false);

	// Have to wait for at least one model to be in each RenderPipelineStage or some issues arise.
	while (modelLoader.isEmpty()) {
		ResourceDecay::checkDestroy();
	}

	mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr);

	std::vector<AccelerationStructureBuffers> scratchBuffers;
	cpuWaitHandles.push_back(modelLoader.buildRTAccelerationStructureDeferred(mCommandList.Get(), scratchBuffers));

	BuildFrameResources();

	// All CPU side setup work must be somewhat complete to resolve the transitions between stages.
	WaitForMultipleObjects((DWORD)cpuWaitHandles.size(), cpuWaitHandles.data(), TRUE, INFINITE);
	std::vector<CD3DX12_RESOURCE_BARRIER> initialTransitions = PipelineStage::setupResourceTransitions({ {renderStage.get()},
		{meshletStage.get()},
		{computeStage.get(), deferStage.get()},
		{hBlurStage.get()},
		{vBlurStage.get()}, 
		{mergeStage.get()}, 
		{vrsComputeStage.get()} });
	mCommandList->ResourceBarrier((UINT)initialTransitions.size(), initialTransitions.data());

	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	flushCommandQueue();

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

	gFrame++;
	gFrameIndex = gFrame % CPU_FRAME_COUNT;

	mCurrFrameResourceIndex = gFrameIndex;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();
	mCurrComputeFrameResource = mComputeFrameResources[mCurrFrameResourceIndex].get();

	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence) {
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		assert(eventHandle != 0);
		mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle);
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	ResourceDecay::checkDestroy();
	ModelLoader::getInstance().isEmpty();

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

	mCommandList->Reset(cmdListAlloc.Get(), nullptr);
	// Compute doesn't really need a command list.

	SetName(mCommandList.Get(), L"Meta Command List");

	auto transToCopy = CD3DX12_RESOURCE_BARRIER::Transition(currentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
	mCommandList->ResourceBarrier(1, &transToCopy);

	std::vector<HANDLE> eventHandles;
	eventHandles.push_back(renderStage->deferExecute());
	eventHandles.push_back(meshletStage->deferExecute());
	eventHandles.push_back(computeStage->deferExecute());
	eventHandles.push_back(deferStage->deferExecute());
	eventHandles.push_back(hBlurStage->deferExecute());
	eventHandles.push_back(vBlurStage->deferExecute());
	eventHandles.push_back(mergeStage->deferExecute());
	eventHandles.push_back(vrsComputeStage->deferExecute());
	// Update done on main thread since modelLoader thread could be busy loading.
	ModelLoader::getInstance().updateRTAccelerationStructure(mCommandList.Get());

	WaitForMultipleObjects((DWORD)eventHandles.size(), eventHandles.data(), TRUE, INFINITE);

	PIXBeginEvent(mCommandList.Get(), PIX_COLOR(0, 0, 255), "Copy and Show");

	// TODO: FIND A WAY TO MAKE SURE THIS RESOURCE ALWAYS GOES BACK TO THE CORRECT STATE
	DX12Resource* mergeOut = mergeStage->getResource("mergedTex");
	auto mergeTransitionIn = CD3DX12_RESOURCE_BARRIER::Transition(mergeOut->get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_SOURCE);
	mCommandList->ResourceBarrier(1, &mergeTransitionIn);
	mCommandList->CopyResource(currentBackBuffer(), mergeOut->get());
	auto mergeTransitionOut = CD3DX12_RESOURCE_BARRIER::Transition(mergeOut->get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
	mCommandList->ResourceBarrier(1, &mergeTransitionOut);

	auto transToRender = CD3DX12_RESOURCE_BARRIER::Transition(currentBackBuffer(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mCommandList->ResourceBarrier(1, &transToRender);

	mCommandList->SetDescriptorHeaps(1, mImguiHeap.GetAddressOf());

	auto backBufferView = currentBackBufferView();
	mCommandList->OMSetRenderTargets(1, &backBufferView, true, nullptr);

	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mCommandList.Get());

	auto transToPresent = CD3DX12_RESOURCE_BARRIER::Transition(currentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	mCommandList->ResourceBarrier(1, &transToPresent);

	PIXEndEvent(mCommandList.Get());

	mCommandList->Close();

	std::vector<ID3D12CommandList*> cmdList = { renderStage->mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists((UINT)cmdList.size(), cmdList.data());
	mCommandQueue->Signal(mFence.Get(), ++mCurrentFence);

	cmdList = { meshletStage->mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists((UINT)cmdList.size(), cmdList.data());
	mCommandQueue->Signal(mAuxFences[3].Get(), ++mCurrentAuxFence[3]);

	cmdList = { computeStage->mCommandList.Get() };
	mComputeCommandQueue->Wait(mFence.Get(), mCurrentFence);
	mComputeCommandQueue->Wait(mAuxFences[3].Get(), mCurrentAuxFence[3]);
	mComputeCommandQueue->ExecuteCommandLists((UINT)cmdList.size(), cmdList.data());
	mComputeCommandQueue->Signal(mAuxFences[0].Get(), ++mCurrentAuxFence[0]);


	cmdList = { deferStage->mCommandList.Get() };
	mCommandQueue->Wait(mFence.Get(), mCurrentFence);
	mCommandQueue->ExecuteCommandLists((UINT)cmdList.size(), cmdList.data());
	mCommandQueue->Signal(mAuxFences[1].Get(), ++mCurrentAuxFence[1]);

	cmdList = { hBlurStage->mCommandList.Get() };
	mComputeCommandQueue->Wait(mAuxFences[0].Get(), mCurrentAuxFence[0]);
	mComputeCommandQueue->ExecuteCommandLists((UINT)cmdList.size(), cmdList.data());
	mComputeCommandQueue->Signal(mAuxFences[2].Get(), ++mCurrentAuxFence[2]);

	cmdList = { vBlurStage->mCommandList.Get() };
	mComputeCommandQueue->Wait(mAuxFences[2].Get(), mCurrentAuxFence[2]);
	mComputeCommandQueue->ExecuteCommandLists((UINT)cmdList.size(), cmdList.data());
	mComputeCommandQueue->Signal(mAuxFences[2].Get(), ++mCurrentAuxFence[2]);

	cmdList = { mergeStage->mCommandList.Get() };
	mCommandQueue->Wait(mAuxFences[2].Get(), mCurrentAuxFence[2]);
	mCommandQueue->Wait(mAuxFences[1].Get(), mCurrentAuxFence[1]);
	mCommandQueue->ExecuteCommandLists((UINT)cmdList.size(), cmdList.data());
	mCommandQueue->Signal(mFence.Get(), ++mCurrentFence);

	cmdList = { vrsComputeStage->mCommandList.Get() };
	mComputeCommandQueue->Wait(mFence.Get(), mCurrentFence);
	mComputeCommandQueue->ExecuteCommandLists((UINT)cmdList.size(), cmdList.data());
	mComputeCommandQueue->Signal(mFence.Get(), ++mCurrentFence);

	cmdList = { mCommandList.Get() };
	mCommandQueue->Wait(mFence.Get(), mCurrentFence);
	mCommandQueue->ExecuteCommandLists((UINT)cmdList.size(), cmdList.data());

	mSwapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	mCurrFrameResource->Fence = ++mCurrentFence;

	mCommandQueue->Signal(mFence.Get(), mCurrentFence);

	std::chrono::high_resolution_clock::time_point endFrameTime = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double, std::milli> time_span = endFrameTime - startFrameTime;
	cpuFrametime.push_back((FLOAT)time_span.count());
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
	ImGui::PlotLines("Frame Times (ms)", frametimeVec.data(), (int)frametimeVec.size(), 0, "Frame Times (ms)", 0.0f, 8.0f, ImVec2(ImGui::GetWindowWidth(), 100));
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	ImGui::Text("Last 1000 Frame Average %.3f ms/frame", AverageVector(frametimeVec));
	ImGui::PlotLines("CPU Frame Times (ms)", cpuFrametimeVec.data(), (int)cpuFrametimeVec.size(), 0, "CPU Frame Times (ms)", 0.0f, 8.0f, ImVec2(ImGui::GetWindowWidth(), 100));
	ImGui::Text("Last 1000 CPU Frame Average %.3f ms/frame", AverageVector(cpuFrametimeVec));
	ImGui::Text("Position: %.3f %.3f %.3f", eyePos.x, eyePos.y, eyePos.z);
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
		ImGui::SliderFloat("SSAO Ray Length", &ssaoConstantCB.data.rayLength, 0.0f, 1000.0f);
		ImGui::SliderInt("Shadow Steps", &ssaoConstantCB.data.shadowSteps, 1, 100);
		ImGui::SliderFloat("Shadow Step Size", &ssaoConstantCB.data.shadowStepSize, 0.0f, 1000.0f);
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem("VRS Ranges")) {
		ImGui::SliderFloat("VRS Short", &mainPassCB.data.VrsShort, 0.0f, 2000.0f);
		ImGui::SliderFloat("VRS Medium", &mainPassCB.data.VrsMedium, 0.0f, 2000.0f);
		ImGui::SliderFloat("VRS Long", &mainPassCB.data.VrsLong, 0.0f, 2000.0f);
		ImGui::SliderFloat("VRS Avg Lum Cutoff", &vrsCB.data.vrsAvgCut, 0.0f, 0.1f);
		ImGui::SliderFloat("VRS Lum Variance", &vrsCB.data.vrsVarianceCut, 0.0f, 0.1f);
		ImGui::SliderInt("VRS Lum Variance Voters", &vrsCB.data.vrsVarianceVotes, 0, vrsSupport.ShadingRateImageTileSize * vrsSupport.ShadingRateImageTileSize);
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem("Light Options")) {
		ImGui::SliderFloat("Exposure", &lightDataCB.data.exposure, 0.0f, 5.0f);
		ImGui::SliderFloat("Gamma", &lightDataCB.data.gamma, 0.0f, 5.0f);
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem("Model Options")) {
		if (ImGui::Button("Load Scene")) {
			auto scenePath = fileSelect();
			if (scenePath.first != "") {
				SceneCsv scene(scenePath.first, scenePath.second);
				loadScene(scene);
				loadedScenes.push_back(scene);
			}
		}
		for (auto& scene : loadedScenes) {
			if (ImGui::Button(("Save " + scene.getFileName()).c_str())) {
				updateSceneClass(scene);
				scene.saveSceneToDisk();
			}
			ImGui::SameLine();
			if (ImGui::Button(("Unload " + scene.getFileName()).c_str())) {
				unloadScene(scene.getFileName());
			}
		}
		for (auto& data : activeModels) {
			ImGui::Text(("Name: " + data.first).c_str());
			bool requiresUpdate = false;
			int instanceCount = data.second.instances.size();
			requiresUpdate |= ImGui::SliderInt((data.first + " Instances: ").c_str(), &instanceCount, 1, MAX_INSTANCES);
			ImGui::BeginTabBar(("Instance Params" + data.first).c_str());
			if (requiresUpdate) {
				data.second.instances.resize(instanceCount);
				ModelLoader::getInstance().instanceCountChanged = true;
			}
			for (int i = 0; i < data.second.instances.size(); i++) {
				if (ImGui::BeginTabItem(("Instance: " + std::to_string(i)).c_str())) {
					requiresUpdate |= ImGui::DragFloat3("Scale: ", (float*)&data.second.instances[i].scale, 0.01f);
					requiresUpdate |= ImGui::DragFloat3("Translate: ", (float*)&data.second.instances[i].pos, 1.0f);
					requiresUpdate |= ImGui::DragFloat3("Rotation: ", (float*)&data.second.instances[i].rot, 0.01f);
					ImGui::EndTabItem();
				}
			}
			if (requiresUpdate) {
				ApplyModelDataUpdate(&data.second);
			}
			ImGui::EndTabBar();
		}
		ImGui::EndTabItem();
	}
	ImGui::EndTabBar();
	ImGui::End();
}

void DemoApp::onResize() {
	DX12App::onResize();

	DirectX::XMMATRIX P = DirectX::XMMatrixPerspectiveFovLH(0.25f * DirectX::XM_PI, getAspectRatio(), NEAR_Z, FAR_Z);
	XMStoreFloat4x4(&proj, P);
}


void DemoApp::loadScene(SceneCsv scene) {
	for (const auto& item : scene.getItems()) {
		loadModel(item.modelName, item.fileName, baseDir + "\\" + item.filePath, item.instances);
	}
}

void DemoApp::unloadScene(std::string fileName) {
	int sceneIdx = -1;
	for (size_t i = 0; i < loadedScenes.size(); i++) {
		if (loadedScenes[i].getFileName() == fileName) {
			sceneIdx = i;
		}
	}
	if (sceneIdx != -1) {
		SceneCsv& scene = loadedScenes[sceneIdx];
		for (const auto& item : scene.getItems()) {
			unloadModel(item.modelName);
		}
		loadedScenes.erase(loadedScenes.begin() + sceneIdx);
	}
	else {
		MessageBoxA(nullptr, ("Couldn't unload SceneCsv: " + fileName + "\nNo scene with that file name is loaded.").c_str(), "Couldn't Unload Scene", MB_OK);
	}
}

// Only support saving the first instance, which isn't great, but file format is still early, so unknown.
void DemoApp::updateSceneClass(SceneCsv& scene) {
	const auto& items = scene.getItems();
	for (size_t i = 0; i < items.size(); i++) {
		auto model = activeModels.find(items[i].modelName);
		if (model != activeModels.end()) {
			scene.updateEntry(i, model->second.instances);
		}
	}
}

void DemoApp::ApplyModelDataUpdate(ModelData* data) {
	auto model = activeModels[data->name].model.lock();
	model->setInstanceCount(data->instances.size());

	DirectX::XMFLOAT4X4 transform;
	for (int i = 0; i < data->instances.size(); i++) {
		DirectX::XMStoreFloat4x4(&transform,
			DirectX::XMMatrixTranspose(DirectX::XMMatrixMultiply(DirectX::XMMatrixMultiply(DirectX::XMMatrixScalingFromVector(DirectX::XMLoadFloat3(&data->instances[i].scale)),
				DirectX::XMMatrixRotationRollPitchYawFromVector(DirectX::XMLoadFloat3(&data->instances[i].rot))), DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&data->instances[i].pos)))));
		model->setTransform(i, transform);
	}
}

void DemoApp::loadModel(std::string name, std::string fileName, std::string dirName, std::vector<InstanceData> instances) {
	ModelData defaultInitData;
	defaultInitData.instances = instances;
	defaultInitData.name = name;
	defaultInitData.model = ModelLoader::loadModel(fileName, dirName, true);
	activeModels[name] = defaultInitData;

	auto model = defaultInitData.model.lock();
	model->setInstanceCount(instances.size());

	DirectX::XMFLOAT4X4 transform;
	for (int i = 0; i < instances.size(); i++) {
		DirectX::XMStoreFloat4x4(&transform,
			DirectX::XMMatrixTranspose(DirectX::XMMatrixMultiply(DirectX::XMMatrixMultiply(DirectX::XMMatrixScalingFromVector(DirectX::XMLoadFloat3(&instances[i].scale)),
				DirectX::XMMatrixRotationRollPitchYawFromVector(DirectX::XMLoadFloat3(&instances[i].rot))), DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&instances[i].pos)))));
		model->setTransform(i, transform);
	}
}

void DemoApp::unloadModel(std::string name) {
	auto modelData = activeModels.find(name);
	if (modelData != activeModels.end()) {
		activeModels.erase(modelData);
		auto model = modelData->second.model.lock();
		ModelLoader::unloadModel(model->name, model->dir);
	}
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
		move.z += MOVE_SPEED;
	}
	if (keyboard.getKeyStatus('S') & KEY_STATUS_PRESSED) {
		move.z -= MOVE_SPEED;
	}
	if (keyboard.getKeyStatus('A') & KEY_STATUS_PRESSED) {
		move.x -= MOVE_SPEED;
	}
	if (keyboard.getKeyStatus('D') & KEY_STATUS_PRESSED) {
		move.x += MOVE_SPEED;
	}
	if (keyboard.getKeyStatus(VK_LSHIFT) & KEY_STATUS_PRESSED) {
		move.x = RUN_MULTIPLIER * move.x;
		move.y = RUN_MULTIPLIER * move.y;
		move.z = RUN_MULTIPLIER * move.z;
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
	float dx = float(x - hWindowClientCenter.x);
	float dy = float(y - hWindowClientCenter.y);

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

	mainPassCB.data.RenderTargetSize = DirectX::XMFLOAT2((float)gScreenWidth, (float)gScreenHeight);
	mainPassCB.data.InvRenderTargetSize = DirectX::XMFLOAT2(1.0f / gScreenWidth, 1.0f / gScreenHeight);
	mainPassCB.data.NearZ = NEAR_Z;
	mainPassCB.data.FarZ = FAR_Z;
	mainPassCB.data.DeltaTime = ImGui::GetIO().DeltaTime;
	mainPassCB.data.TotalTime += mainPassCB.data.DeltaTime;
	mainPassCB.data.renderVRS = renderVRS;


	ssaoConstantCB.data.range = mainPassCB.data.FarZ / (mainPassCB.data.FarZ - mainPassCB.data.NearZ);
	ssaoConstantCB.data.rangeXNear = ssaoConstantCB.data.range * mainPassCB.data.NearZ;
	ssaoConstantCB.data.viewProj = mainPassCB.data.viewProj;

	deferStage->deferUpdateConstantBuffer("SSAOConstants", ssaoConstantCB);

	lightDataCB.data.viewPos = mainPassCB.data.EyePosW;

	std::vector<Light> lights = ModelLoader::getAllLights(lightDataCB.data.numPointLights, lightDataCB.data.numDirectionalLights, lightDataCB.data.numPointLights);
	memcpy(lightDataCB.data.lights, lights.data(), lights.size() * sizeof(Light));

	vrsComputeStage->deferUpdateConstantBuffer("VrsConstants", vrsCB);
	deferStage->deferUpdateConstantBuffer("LightData", lightDataCB);
	renderStage->deferUpdateConstantBuffer("PerPassConstants", mainPassCB);

	ModelLoader::updateTransforms();

	if (!freezeCull) {
		renderStage->frustrum = DirectX::BoundingFrustum(proj);
		renderStage->frustrum.Transform(renderStage->frustrum, invView);
	}
	renderStage->eyePos = DirectX::XMFLOAT3(eyePos.x, eyePos.y, eyePos.z);
	renderStage->VRS = VRS;
	deferStage->VRS = VRS;
}