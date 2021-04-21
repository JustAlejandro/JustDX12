#pragma once
#include <array>
#include <DirectXCollision.h>

#include "PipelineStage\PipelineStage.h"
#include "ModelListener.h"

class BasicModel;
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
	// Not used in meshlets.
	int perMeshTransformCBSlot = -1;
	// What root parameter 'slot' does the HLSL shader expect the SRV range associated with textures to be in?
	int perMeshTextureSlot = -1;
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
	std::vector<DescriptorJob> buildMeshTexturesDescriptorJobs(Mesh* m);
	virtual void buildInputLayout() override;
	bool setupRenderObjects();

	void bindDescriptorsToRoot(DESCRIPTOR_USAGE usage = DESCRIPTOR_USAGE_PER_PASS, int usageIndex = 0, std::vector<RootParamDesc> curRootParamDescs[DESCRIPTOR_USAGE_MAX] = nullptr) override;
	void bindRenderTarget();

	bool performsTransitions() override;
	void performTransitionsIn() override;
	void performTransitionsOut() override;
	void addTransitionIn(DX12Resource* res, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter) override;
	void addTransitionOut(DX12Resource* res, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter) override;

	virtual void draw() = 0;

	std::vector<std::pair<D3D12_RESOURCE_STATES, DX12Resource*>> getRequiredResourceStates() override;

	RenderPipelineDesc renderStageDesc;

	// State transitions that this PipelineStage needs to perform to keep resource state flow smooth
	// This is filled out by the state transition delegation in PipelineStage::setupResourceTransitions
	std::vector<CD3DX12_RESOURCE_BARRIER> transitionsIn;
	std::vector<CD3DX12_RESOURCE_BARRIER> transitionsOut;

	D3D12_VIEWPORT viewport;
	D3D12_RECT scissorRect;
};

