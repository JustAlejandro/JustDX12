#pragma once
#include "PipelineStage\PipelineStage.h"
#include <array>
#include <DirectXCollision.h>
class Model;
class Mesh;
class ModelLoader;
class MeshletModel;

struct RenderPipelineDesc {
	std::vector<RenderTargetDesc> renderTargets;
	std::vector<std::pair<MODEL_FORMAT, std::string>> textureToDescriptor;
	std::unordered_map<MODEL_FORMAT, std::string> defaultTextures;
	bool usesDepthTex = true;
	bool supportsCulling = false;
	bool supportsVRS = false;
	int rtTlasSlot = -1;
	int rtTlasMeshletSlot = -1;
	ID3D12Resource** tlasPtr = nullptr;
	std::string VrsTextureName = "VRS";
	int perObjTransformCBSlot = -1;
	int perMeshTransformCBSlot = -1;
	int perMeshTextureSlot = -1;
	int perObjTransformCBMeshletSlot = -1;
	int perObjTextureMeshletSlot = -1;
	bool usesMeshlets = false;
	std::vector<RootParamDesc> meshletRootSignature;
	std::vector<std::pair<MODEL_FORMAT, std::string>> meshletTextureToDescriptor;
};

class RenderPipelineStage : public PipelineStage {
public:
	RenderPipelineStage(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice, RenderPipelineDesc renderDesc, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect);
	void setup(PipeLineStageDesc stageDesc) override;
	void Execute() override;
	void LoadModel(ModelLoader* loader, std::string referenceName, std::string fileName, std::string dirName, bool usesRT = false);
	void LoadMeshletModel(ModelLoader* loader, std::string fileName, std::string dirName, bool usesRT = false);
	void UnloadModel(ModelLoader* loader, std::string friendlyName);
	void updateInstanceCount(std::string referenceName, UINT instanceCount);
	void updateInstanceTransform(std::string referenceName, UINT instanceIndex, DirectX::XMFLOAT4X4 transform);
	void updateMeshletTransform(UINT modelIndex, DirectX::XMFLOAT4X4 transform);
	void setTLAS(Microsoft::WRL::ComPtr<ID3D12Resource> TLAS);
	~RenderPipelineStage();

	DirectX::BoundingFrustum frustrum;
	bool frustrumCull = false;
	DirectX::XMFLOAT3 eyePos = {};
	bool VRS = false;
	bool occlusionCull = true;
protected:
	RenderPipelineDesc renderStageDesc;
	void BuildPSO() override;
	std::vector<std::pair<D3D12_RESOURCE_STATES, DX12Resource*>> getRequiredResourceStates() override;

	bool PerformsTransitions() override;
	void PerformTransitionsIn() override;
	void PerformTransitionsOut() override;
	void AddTransitionIn(DX12Resource* res, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter) override;
	void AddTransitionOut(DX12Resource* res, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter) override;

	void BuildQueryHeap();
	void bindDescriptorsToRoot(DESCRIPTOR_USAGE usage = DESCRIPTOR_USAGE_PER_PASS, int usageIndex = 0, std::vector<RootParamDesc> curRootParamDescs[DESCRIPTOR_USAGE_MAX] = nullptr) override;
	void bindRenderTarget();
	void drawRenderObjects();
	void drawMeshletRenderObjects();
	void drawOcclusionQuery();
	void buildMeshTexturesDescriptors(Mesh* m);
	void buildMeshletTexturesDescriptors(MeshletModel* m, int usageIndex);
	void BuildInputLayout() override;
	bool setupRenderObjects();
	void setupOcclusionBoundingBoxes();

	Microsoft::WRL::ComPtr<ID3D12Resource> TLAS;

	std::unordered_map<std::string, std::weak_ptr<Model>> nameToModel;
	std::vector<std::weak_ptr<Model>> loadingRenderObjects;
	std::vector<std::weak_ptr<Model>> renderObjects;
	std::vector<MeshletModel*> meshletRenderObjects;

	Microsoft::WRL::ComPtr<ID3D12Resource> occlusionQueryResultBuffer;
	Microsoft::WRL::ComPtr<ID3D12QueryHeap> occlusionQueryHeap;
	std::vector<DescriptorJob> renderingDescriptorJobs;

	std::vector<CD3DX12_RESOURCE_BARRIER> transitionsIn;
	std::vector<CD3DX12_RESOURCE_BARRIER> transitionsOut;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> meshletPSO = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> meshletShader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> occlusionPSO = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> occlusionVS = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> occlusionGS = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> occlusionPS = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> occlusionBoundingBoxBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> occlusionBoundingBoxBufferGPUUploader = nullptr;
	D3D12_VERTEX_BUFFER_VIEW occlusionBoundingBoxBufferView = { 0 };
	std::vector<D3D12_INPUT_ELEMENT_DESC> occlusionInputLayout;

	// Have to keep seperate rootsig for Meshes because vertex data is now a bound object.
	Microsoft::WRL::ComPtr<ID3D12RootSignature> meshRootSignature = nullptr;
	std::vector<RootParamDesc> meshRootParameterDescs[DESCRIPTOR_USAGE_MAX];

	D3D12_VIEWPORT viewport;
	D3D12_RECT scissorRect;
};

