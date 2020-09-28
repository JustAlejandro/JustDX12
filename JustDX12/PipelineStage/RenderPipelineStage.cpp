#include "PipelineStage\RenderPipelineStage.h"
#include "DescriptorClasses\DescriptorManager.h"
#include "ModelLoading/ModelLoader.h"
#include "RenderPipelineStageTask.h"
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

	mCommandList->SetGraphicsRootSignature(rootSignature.Get());
	
	bindDescriptorsToRoot();

	bindRenderTarget();

	drawRenderObjects();

	mCommandList->Close();

	ID3D12CommandList* cmdList[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdList), cmdList);
}

void RenderPipelineStage::LoadModel(ModelLoader* loader, std::string fileName, std::string dirName) {
	renderObjects.push_back(loader->loadModel(fileName, dirName));
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
	graphicsPSO.NumRenderTargets = renderTargetDescs.size();
	for (int i = 0; i < renderTargetDescs.size(); i++) {
		graphicsPSO.RTVFormats[i] = descriptorManager.getDescriptor(
				renderTargetDescs[i].descriptorName, 
				DESCRIPTOR_TYPE_RTV)->resourceTarget->getFormat();
	}
	graphicsPSO.SampleDesc.Count = 1;
	graphicsPSO.SampleDesc.Quality = 0;
	graphicsPSO.DSVFormat = DEPTH_TEXTURE_DSV_FORMAT;
	if (md3dDevice->CreateGraphicsPipelineState(&graphicsPSO, IID_PPV_ARGS(&PSO)) < 0) {
		OutputDebugStringA("PSO Setup Failed");
		throw "PSO FAIL";
	}
}

void RenderPipelineStage::bindDescriptorsToRoot() {
	for (int i = 0; i < rootParameterDescs.size(); i++) {
		DESCRIPTOR_TYPE descriptorType = getDescriptorTypeFromRootParameterType(rootParameterDescs[i].type);
		switch (descriptorType) {
		case DESCRIPTOR_TYPE_NONE:
			throw "Not sure what this is";
		case DESCRIPTOR_TYPE_SRV:
			mCommandList->SetGraphicsRootDescriptorTable(i,
				descriptorManager.getDescriptor(rootParameterDescs[i].name, descriptorType)->gpuHandle);
			break;
		case DESCRIPTOR_TYPE_UAV:
			mCommandList->SetGraphicsRootDescriptorTable(i,
				descriptorManager.getDescriptor(rootParameterDescs[i].name, descriptorType)->gpuHandle);
			break;
		case DESCRIPTOR_TYPE_CBV:
			mCommandList->SetGraphicsRootDescriptorTable(i,
				descriptorManager.getDescriptor(rootParameterDescs[i].name, descriptorType)->gpuHandle);
			break;
		default:
			throw "Don't know what to do here.";
			break;
		}
	}
}

void RenderPipelineStage::bindRenderTarget() {
	mCommandList->ClearDepthStencilView(
		descriptorManager.getAllDescriptorsOfType(DESCRIPTOR_TYPE_DSV)->at(0)->cpuHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1.0f, 0, 0, nullptr);

	mCommandList->OMSetRenderTargets(renderTargetDescs.size(),
		&descriptorManager.getDescriptor(renderTargetDescs[0].descriptorName, DESCRIPTOR_TYPE_RTV)->cpuHandle,
		true,
		&descriptorManager.getAllDescriptorsOfType(DESCRIPTOR_TYPE_DSV)->at(0)->cpuHandle);
}

void RenderPipelineStage::drawRenderObjects() {
	for (int i = 0; i < renderObjects.size(); i++) {
		Model* model = renderObjects[i];
		if (!model->loaded) continue;

		mCommandList->IASetVertexBuffers(0, 1, &model->vertexBufferView());
		mCommandList->IASetIndexBuffer(&model->indexBufferView());
		mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		//Here's where we need some constant buffer setups...

		for (const Mesh& m : model->meshes) {
			mCommandList->DrawIndexedInstanced(m.indexCount,
				1, m.startIndexLocation, m.baseVertexLocation, 0);
		}
	}
}
