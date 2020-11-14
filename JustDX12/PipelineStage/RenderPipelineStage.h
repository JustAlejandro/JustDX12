#pragma once
#include "PipelineStage\PipelineStage.h"
#include <array>
#include <DirectXCollision.h>
class Model;
class Mesh;
class ModelLoader;
class MeshletModel;

struct RenderPipelineDesc {
	bool supportsCulling = false;
};

class RenderPipelineStage : public PipelineStage {
public:
	RenderPipelineStage(Microsoft::WRL::ComPtr<ID3D12Device2> d3dDevice, RenderPipelineDesc renderDesc, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect);
	void Execute() override;
	void LoadModel(ModelLoader* loader, std::string fileName, std::string dirName);
	void LoadMeshletModel(ModelLoader* loader, std::string fileName, std::string dirName);
	~RenderPipelineStage();

	DirectX::BoundingFrustum frustrum;
	bool frustrumCull;
	DirectX::XMFLOAT3 eyePos;
	bool VRS;
	bool occlusionCull = true;
protected:
	RenderPipelineDesc renderStageDesc;
	void BuildPSO() override;
	void BuildQueryHeap();
	void bindDescriptorsToRoot(DESCRIPTOR_USAGE usage = DESCRIPTOR_USAGE_PER_PASS, int usageIndex = 0, std::vector<RootParamDesc> curRootParamDescs[DESCRIPTOR_USAGE_MAX] = nullptr) override;
	void bindRenderTarget();
	void drawRenderObjects();
	void drawOcclusionQuery();
	void importMeshTextures(Mesh* m, int usageIndex);
	void buildMeshTexturesDescriptors(Mesh* m, int usageIndex);
	void setupRenderObjects();

	void addDescriptorJob(DescriptorJob j);

	std::vector<Model*> renderObjects;
	std::vector<MeshletModel*> meshletRenderObjects;
	Microsoft::WRL::ComPtr<ID3D12Resource> occlusionQueryResultBuffer;
	Microsoft::WRL::ComPtr<ID3D12QueryHeap> occlusionQueryHeap;

	bool allRenderObjectsSetup = false;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> meshletPSO = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> meshletShader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> occlusionPSO = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> occlusionVS = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> occlusionGS = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> occlusionPS = nullptr;

	// Have to keep seperate rootsig for Meshes because vertex data is now a bound object.
	Microsoft::WRL::ComPtr<ID3D12RootSignature> meshRootSignature = nullptr;
	std::vector<RootParamDesc> meshRootParameterDescs[DESCRIPTOR_USAGE_MAX];

	D3D12_VIEWPORT viewport;
	D3D12_RECT scissorRect;
};

