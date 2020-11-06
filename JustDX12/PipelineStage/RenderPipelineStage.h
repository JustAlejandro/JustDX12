#pragma once
#include "PipelineStage\PipelineStage.h"
#include <array>
#include <DirectXCollision.h>
class Model;
class Mesh;
class ModelLoader;

struct RenderPipelineDesc {

};

class RenderPipelineStage : public PipelineStage {
public:
	RenderPipelineStage(Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice, RenderPipelineDesc renderDesc, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect);
	void Execute() override;
	void LoadModel(ModelLoader* loader, std::string fileName, std::string dirName);
	~RenderPipelineStage();

	DirectX::BoundingFrustum frustrum;
	bool frustrumCull;
	DirectX::XMFLOAT3 eyePos;
	bool VRS;
protected:
	RenderPipelineDesc renderStageDesc;
	void BuildPSO() override;
	void bindDescriptorsToRoot(DESCRIPTOR_USAGE usage = DESCRIPTOR_USAGE_PER_PASS, int usageIndex = 0) override;
	void bindRenderTarget();
	void drawRenderObjects();
	void importMeshTextures(Mesh* m, int usageIndex);
	void buildMeshTexturesDescriptors(Mesh* m, int usageIndex);
	void setupRenderObjects();

	void addDescriptorJob(DescriptorJob j);

	std::vector<Model*> renderObjects;
	bool allRenderObjectsSetup = false;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> occlusionPSO = nullptr;

	D3D12_VIEWPORT viewport;
	D3D12_RECT scissorRect;
};

