#pragma once
#include "PipelineStage/RenderPipelineStage.h"

#include "ModelListener.h"

class ModelRenderPipelineStage : public RenderPipelineStage, public ModelListener {
public:
	ModelRenderPipelineStage(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice, RenderPipelineDesc renderDesc, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect);
	~ModelRenderPipelineStage();

protected:
	virtual void buildPSO() override;
	virtual void buildInputLayout() override;
	void buildQueryHeap();
	// Inherited via ModelListener
	virtual void processModel(std::weak_ptr<Model> model) override;

	virtual void draw() override;
	virtual void drawModels();
	void drawOcclusionQuery();

	void setupOcclusionBoundingBoxes();

	// ModelLoader still 'owns' models, so as long as we process all the unloads in a thread-safe way
	// the RenderPipelineStage should be aware of when a renderObject is no longer available
	std::vector<std::weak_ptr<Model>> renderObjects;

	Microsoft::WRL::ComPtr<ID3D12Resource> occlusionQueryResultBuffer;
	Microsoft::WRL::ComPtr<ID3D12QueryHeap> occlusionQueryHeap;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> occlusionPSO = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> occlusionVS = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> occlusionGS = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> occlusionPS = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> occlusionBoundingBoxBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> occlusionBoundingBoxBufferGPUUploader = nullptr;
	D3D12_VERTEX_BUFFER_VIEW occlusionBoundingBoxBufferView = { 0 };
	std::vector<D3D12_INPUT_ELEMENT_DESC> occlusionInputLayout;
};