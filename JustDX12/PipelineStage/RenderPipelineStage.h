#pragma once
#include <array>
#include <DirectXCollision.h>

#include "PipelineStage\PipelineStage.h"
#include "ModelListener.h"

class Model;
class Mesh;
class ModelLoader;
class MeshletModel;

// Provides extra details needed for a RenderPipelineStage that aren't included in the normal PipelineStageDesc
struct RenderPipelineDesc {
	std::vector<RenderTargetDesc> renderTargets;
	// Mapping from MODEL_FORMAT to the name of the descriptor that represents the texure
	// ex: RootParamDesc asks for "texture_diffuse", so an entry in 'textureToDescriptor' would contain { MODEL_FORMAT_DIFFUSE_TEX, "texture_diffuse" }
	std::vector<std::pair<MODEL_FORMAT, std::string>> textureToDescriptor;
	// if a mesh or model that was loaded doesn't contain a texture mapping specfied in 'textureToDescriptor', here's where you should point to some defualt
	// fallback texture (use the same name used to load the texture in 'textureFiles' from PipelineStageDesc
	std::unordered_map<MODEL_FORMAT, std::string> defaultTextures;
	bool usesDepthTex = true;
	// 'Culling' here means using bounding boxes and occlusion predication culling to accelerate CPU/GPU rendering
	bool supportsCulling = false;
	// If set to true, the resource specified by 'VrsTextureName' will be bound (assuming VRS tier 2 support)
	bool supportsVRS = false;
	std::string VrsTextureName = "VRS";
	// What root parameter 'slot' does the HLSL shader expect PerObject transform data to be in?
	int perObjTransformCBSlot = -1;
	// What root parameter 'slot' does the HLSL shader expect PerMesh transform data to be in?
	// Used in combination with PerObject transform to find a final toWorld transform
	int perMeshTransformCBSlot = -1;
	// What root parameter 'slot' does the HLSL shader expect the SRV range associated with textures to be in?
	int perMeshTextureSlot = -1;
	// What root parameter 'slot' does the HLSL shader expect PerObject transform data to be in for meshlets?
	int perObjTransformCBMeshletSlot = -1;
	// What root parameter 'slot' does the HLSL shader expect the SRV range associated with textures for meshlets to be in?
	int perObjTextureMeshletSlot = -1;
	bool usesMeshlets = false;
	// RootSig specific to meshlets. Expects a few specific formats for a few slots, see JustDX12.cpp for a use case
	// TODO: make this specified binding absolute, or find a way to make it user configurable
	std::vector<RootParamDesc> meshletRootSignature;
	// Same as 'textureToDescriptor', but for Meshlets
	std::vector<std::pair<MODEL_FORMAT, std::string>> meshletTextureToDescriptor;
};

class RenderPipelineStage : public PipelineStage {
public:
	RenderPipelineStage(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice, RenderPipelineDesc renderDesc, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect);
	~RenderPipelineStage();

	void setup(PipeLineStageDesc stageDesc) override;

	void execute() override;

	void loadMeshletModel(std::string fileName, std::string dirName, bool usesRT = false);
	void updateMeshletTransform(UINT modelIndex, DirectX::XMFLOAT4X4 transform);

	// Data associated with culling
	// TODO: give a better interface to this data
	DirectX::BoundingFrustum frustrum;
	bool frustrumCull = false;
	DirectX::XMFLOAT3 eyePos = {};
	bool VRS = false;
	bool occlusionCull = true;
protected:
	virtual void buildPSO() override;
	void buildQueryHeap();
	std::vector<DescriptorJob> buildMeshTexturesDescriptorJobs(Mesh* m);
	void buildMeshletTexturesDescriptors(MeshletModel* m, int usageIndex);
	virtual void buildInputLayout() override;
	bool setupRenderObjects();

	void bindDescriptorsToRoot(DESCRIPTOR_USAGE usage = DESCRIPTOR_USAGE_PER_PASS, int usageIndex = 0, std::vector<RootParamDesc> curRootParamDescs[DESCRIPTOR_USAGE_MAX] = nullptr) override;
	void bindRenderTarget();

	bool performsTransitions() override;
	void performTransitionsIn() override;
	void performTransitionsOut() override;
	void addTransitionIn(DX12Resource* res, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter) override;
	void addTransitionOut(DX12Resource* res, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter) override;

	virtual void draw();
	void drawMeshletRenderObjects();

	std::vector<std::pair<D3D12_RESOURCE_STATES, DX12Resource*>> getRequiredResourceStates() override;

	RenderPipelineDesc renderStageDesc;

	// ModelLoader still 'owns' models, so as long as we process all the unloads in a thread-safe way
	// the RenderPipelineStage should be aware of when a renderObject is no longer available
	std::vector<std::weak_ptr<Model>> renderObjects;
	std::vector<MeshletModel*> meshletRenderObjects;

	Microsoft::WRL::ComPtr<ID3D12Resource> occlusionQueryResultBuffer;
	Microsoft::WRL::ComPtr<ID3D12QueryHeap> occlusionQueryHeap;

	// State transitions that this PipelineStage needs to perform to keep resource state flow smooth
	// This is filled out by the state transition delegation in PipelineStage::setupResourceTransitions
	std::vector<CD3DX12_RESOURCE_BARRIER> transitionsIn;
	std::vector<CD3DX12_RESOURCE_BARRIER> transitionsOut;

	// TODO: make meshlet rendering it's own PipelineStage, seperate from RenderPipelineStage
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

