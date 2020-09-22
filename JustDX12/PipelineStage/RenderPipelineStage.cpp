#include "PipelineStage\RenderPipelineStage.h"
#include "DescriptorClasses\DescriptorManager.h"
#include <string>
#include "Settings.h"

RenderPipelineStage::RenderPipelineStage(Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect)
	: PipelineStage(d3dDevice) {
	this->viewport = viewport;
	this->scissorRect = scissorRect;
}

void RenderPipelineStage::Execute() {
	resetCommandList();
	bindDescriptorHeaps();
	setResourceStates();

	mCommandList->RSSetViewports(1, &viewport);
	mCommandList->RSSetScissorRects(1, &scissorRect);

	resourceManager.getResource("renderTexture")->changeState(mCommandList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	mCommandList->ClearDepthStencilView(
		descriptorManager.getDescriptor("renderTexture", DESCRIPTOR_TYPE_DSV)->cpuHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1.0f, 0, 0, nullptr);

	mCommandList->OMSetRenderTargets(
		descriptorManager.getDescriptor("defaultRTV0", DESCRIPTOR_TYPE_RTV)->descriptorHeap->GetDesc().NumDescriptors,
		&descriptorManager.getDescriptor("defaultRTV0", DESCRIPTOR_TYPE_RTV)->cpuHandle,
		true,
		&descriptorManager.getDescriptor("defualtDSV", DESCRIPTOR_TYPE_DSV)->cpuHandle);

	mCommandList->SetGraphicsRootSignature(rootSignature.Get());
	
	// TODO: Bind things to the root signature

	// TODO: Actually make the draw call


}

void RenderPipelineStage::BuildPSO() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPSO;
	ZeroMemory(&graphicsPSO, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	graphicsPSO.InputLayout = { inputLayout.data(),(UINT)inputLayout.size() };
	graphicsPSO.pRootSignature = rootSignature.Get();
	graphicsPSO.VS = {
		reinterpret_cast<BYTE*>(shadersByType[SHADER_TYPE_VS]->GetBufferPointer()),
		shadersByType[SHADER_TYPE_VS]->GetBufferSize() };
	graphicsPSO.PS = {
		reinterpret_cast<BYTE*>(shadersByType[SHADER_TYPE_PS]->GetBufferPointer()),
		shadersByType[SHADER_TYPE_PS]->GetBufferSize() };
	graphicsPSO.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	graphicsPSO.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	graphicsPSO.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	graphicsPSO.SampleMask = UINT_MAX;
	graphicsPSO.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	graphicsPSO.NumRenderTargets = descriptorManager.getDescriptor("defaultRTV0", DESCRIPTOR_TYPE_RTV)->descriptorHeap->GetDesc().NumDescriptors;
	for (int i = 0; i < graphicsPSO.NumRenderTargets; i++) {
		graphicsPSO.RTVFormats[i] = resourceManager.getResource("defaultRTV" + std::to_string(i))->getFormat();
	}
	graphicsPSO.SampleDesc.Count = 1;
	graphicsPSO.SampleDesc.Quality = 0;
	graphicsPSO.DSVFormat = resourceManager.getResource("defaultDSV")->getFormat();
	if (device->CreateGraphicsPipelineState(&graphicsPSO, IID_PPV_ARGS(&PSO)) < 0) {
		OutputDebugStringA("PSO Setup Failed");
		throw "PSO FAIL";
	}
}
