#pragma once
#include "PipelineStage\PipelineStage.h"
class Model;
class ModelLoader;


class RenderPipelineStage : public PipelineStage {
public:
	RenderPipelineStage(Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect);
	void Execute() override;
	void LoadModel(ModelLoader* loader, std::string fileName, std::string dirName);

protected:
	void BuildPSO() override;
	void bindDescriptorsToRoot() override;
	void bindRenderTarget();
	void drawRenderObjects();

	std::vector<Model*> renderObjects;

	D3D12_VIEWPORT viewport;
	D3D12_RECT scissorRect;
};

