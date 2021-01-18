#pragma once
#include "PipelineStage\PipelineStage.h"
#include <array>
#include <DirectXCollision.h>
class Model;
class Mesh;
class ModelLoader;
class MeshletModel;

struct RenderPipelineDesc {
	std::vector<std::pair<MODEL_FORMAT, std::string>> textureToDescriptor;
	std::unordered_map<MODEL_FORMAT, std::string> defaultTextures;
	bool usesDepthTex = true;
	bool supportsCulling = false;
	bool supportsVRS = false;
	bool supportsRT = false;
	std::string tlasResourceName = "TLAS";
	std::string VrsTextureName = "VRS";
	bool usesMeshlets = false;
	std::vector<RootParamDesc> meshletRootSignature;
	std::vector<std::pair<MODEL_FORMAT, std::string>> meshletTextureToDescriptor;
};

class RenderPipelineStage : public PipelineStage {
public:
	RenderPipelineStage(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice, RenderPipelineDesc renderDesc, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect);
	void setup(PipeLineStageDesc stageDesc) override;
	void Execute() override;
	void LoadModel(ModelLoader* loader, std::string fileName, std::string dirName, bool usesRT = false);
	void LoadMeshletModel(ModelLoader* loader, std::string fileName, std::string dirName);
	void setTLAS(Microsoft::WRL::ComPtr<ID3D12Resource> TLAS);
	~RenderPipelineStage();

	DirectX::BoundingFrustum frustrum;
	bool frustrumCull;
	DirectX::XMFLOAT3 eyePos;
	bool VRS;
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
	void buildMeshTexturesDescriptors(Mesh* m, int usageIndex);
	void buildMeshletTexturesDescriptors(MeshletModel* m, int usageIndex);
	void BuildInputLayout() override;
	void setupRenderObjects();
	void setupOcclusionBoundingBoxes();

	void addDescriptorJob(DescriptorJob j);

	Microsoft::WRL::ComPtr<ID3D12Resource> TLAS;

	std::vector<Model*> renderObjects;
	std::vector<MeshletModel*> meshletRenderObjects;
	Microsoft::WRL::ComPtr<ID3D12Resource> occlusionQueryResultBuffer;
	Microsoft::WRL::ComPtr<ID3D12QueryHeap> occlusionQueryHeap;
	std::vector<DescriptorJob> renderingDescriptorJobs;

	std::vector<CD3DX12_RESOURCE_BARRIER> transitionsIn;
	std::vector<CD3DX12_RESOURCE_BARRIER> transitionsOut;

	bool allRenderObjectsSetup = false;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> meshletPSO = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> meshletShader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> occlusionPSO = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> occlusionVS = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> occlusionGS = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> occlusionPS = nullptr;

	Microsoft::WRL::ComPtr<ID3DBlob> occlusionBoundingBoxBufferCPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> occlusionBoundingBoxBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> occlusionBoundingBoxBufferGPUUploader = nullptr;
	D3D12_VERTEX_BUFFER_VIEW occlusionBoundingBoxBufferView;
	std::vector<D3D12_INPUT_ELEMENT_DESC> occlusionInputLayout;

	// Have to keep seperate rootsig for Meshes because vertex data is now a bound object.
	Microsoft::WRL::ComPtr<ID3D12RootSignature> meshRootSignature = nullptr;
	std::vector<RootParamDesc> meshRootParameterDescs[DESCRIPTOR_USAGE_MAX];

	D3D12_VIEWPORT viewport;
	D3D12_RECT scissorRect;
};

