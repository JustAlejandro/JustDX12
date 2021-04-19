#include "ScreenRenderPipelineStage.h"

class StaticScreenQuad {
private:
	StaticScreenQuad(ScreenRenderPipelineStage* thread) {
		SetName(thread->mCommandList.Get(), L"StaticScreenQuad appropriated Command List");
		struct CompactVertex {
			DirectX::XMFLOAT3 pos;
			DirectX::XMFLOAT2 texPos;
		};
		CompactVertex points[] = {
			{ { -1.0f, 1.0f, 0.5f }, {0.0f, 0.0f } },
			{ { 1.0f, 1.0f, 0.5f }, {1.0f, 0.0f } },
			{ { -1.0f, -1.0f, 0.5f }, {0.0f, 1.0f } },
			{ { 1.0f, -1.0f, 0.5f }, {1.0f, 1.0f } } };
		UINT indices[] = { 0, 1, 2, 1, 3, 2 };

		Microsoft::WRL::ComPtr<ID3D12Resource> vertexUploadBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> indexUploadBuffer;
		vertexBufferGPU = CreateDefaultBuffer(thread->md3dDevice.Get(), thread->mCommandList.Get(), points, sizeof(points), vertexUploadBuffer);
		indexBufferGPU = CreateDefaultBuffer(thread->md3dDevice.Get(), thread->mCommandList.Get(), indices, sizeof(indices), indexUploadBuffer);
		SetName(vertexUploadBuffer.Get(), L"StaticScreenQuad VertexUploadBuffer");
		SetName(indexUploadBuffer.Get(), L"StaticScreenQuad IndexUploadBuffer");
		ThrowIfFailed(thread->mCommandList->Close());
		ID3D12CommandList* cmdLists[] = { thread->mCommandList.Get() };
		thread->mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

		vertexBufferView.BufferLocation = vertexBufferGPU->GetGPUVirtualAddress();
		vertexBufferView.SizeInBytes = sizeof(points);
		vertexBufferView.StrideInBytes = sizeof(CompactVertex);
		
		indexBufferView.BufferLocation = indexBufferGPU->GetGPUVirtualAddress();
		indexBufferView.Format = DXGI_FORMAT_R32_UINT;
		indexBufferView.SizeInBytes = sizeof(indices);
		
		thread->waitOnFence();
	}
	StaticScreenQuad(StaticScreenQuad const&) = delete;
	void operator=(StaticScreenQuad const&) = delete;

public:
	static StaticScreenQuad& getInstance(ScreenRenderPipelineStage* thread) {
		static StaticScreenQuad instance(thread);
		return instance;
	}
	static D3D12_VERTEX_BUFFER_VIEW getVertexBufferView(ScreenRenderPipelineStage* thread) {
		return getInstance(thread).vertexBufferView;
	}
	static D3D12_INDEX_BUFFER_VIEW getIndexBufferView(ScreenRenderPipelineStage* thread) {
		return getInstance(thread).indexBufferView;
	}
private:
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBufferGPU = nullptr;
};

ScreenRenderPipelineStage::ScreenRenderPipelineStage(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice, RenderPipelineDesc renderDesc, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect)
	: RenderPipelineStage(d3dDevice, renderDesc, viewport, scissorRect) {
	ThrowIfFailed(mDirectCmdListAlloc->Reset());
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), PSO.Get()));
	// Have to call in constructor, otherwise we have no awarensess at startup.
	StaticScreenQuad::getInstance(this);
	// StaticScreenQuad may have used the command list, so we close it for consistency even if it didn't
	// this throws a warning, but we can just ignore it since it's a planned one.
	mCommandList->Close();
}

void ScreenRenderPipelineStage::buildInputLayout() {
	inputLayout = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA } };
}

void ScreenRenderPipelineStage::draw() {
	PIXScopedEvent(mCommandList.Get(), PIX_COLOR(0, 255, 0), (stageDesc.name + " Draw Calls").c_str());
	int modelIndex = 0;
	if (renderStageDesc.supportsVRS && VRS && (vrsSupport.VariableShadingRateTier == D3D12_VARIABLE_SHADING_RATE_TIER_2)) {
		D3D12_SHADING_RATE_COMBINER combiners[2] = { D3D12_SHADING_RATE_COMBINER_OVERRIDE, D3D12_SHADING_RATE_COMBINER_OVERRIDE };
		mCommandList->RSSetShadingRate(D3D12_SHADING_RATE_1X1, combiners);
		mCommandList->RSSetShadingRateImage(resourceManager.getResource(renderStageDesc.VrsTextureName)->get());
	}

	auto vertexBufferView = StaticScreenQuad::getVertexBufferView(this);
	auto indexBufferView = StaticScreenQuad::getIndexBufferView(this);

	mCommandList->IASetVertexBuffers(0, 1, &vertexBufferView);
	mCommandList->IASetIndexBuffer(&indexBufferView);
	mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	mCommandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

	modelIndex++;
}
