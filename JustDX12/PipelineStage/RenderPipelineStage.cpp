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
	if (!allRenderObjectsSetup) {
		setupRenderObjects();
		return;
	}

	resetCommandList();

	//PIXBeginEvent(mCommandList.GetAddressOf(), PIX_COLOR(0.0, 1.0, 0.0), "Forward Pass");

	bindDescriptorHeaps();
	setResourceStates();

	mCommandList->RSSetViewports(1, &viewport);
	mCommandList->RSSetScissorRects(1, &scissorRect);

	mCommandList->RSSetShadingRate(D3D12_SHADING_RATE_4X4, nullptr);

	mCommandList->SetGraphicsRootSignature(rootSignature.Get());
	
	bindDescriptorsToRoot(DESCRIPTOR_USAGE_PER_PASS);

	bindRenderTarget();

	drawRenderObjects();

	//PIXEndEvent(mCommandList.GetAddressOf());

	mCommandList->Close();

	ID3D12CommandList* cmdList[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdList), cmdList);
}

void RenderPipelineStage::LoadModel(ModelLoader* loader, std::string fileName, std::string dirName) {
	renderObjects.push_back(loader->loadModel(fileName, dirName));
}

RenderPipelineStage::~RenderPipelineStage() {
	for (Model* m : renderObjects) {
		delete m;
	}
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
		graphicsPSO.RTVFormats[i] = COLOR_TEXTURE_FORMAT;
	}
	graphicsPSO.SampleDesc.Count = 1;
	graphicsPSO.SampleDesc.Quality = 0;
	graphicsPSO.DSVFormat = DEPTH_TEXTURE_DSV_FORMAT;
	if (md3dDevice->CreateGraphicsPipelineState(&graphicsPSO, IID_PPV_ARGS(&PSO)) < 0) {
		OutputDebugStringA("PSO Setup Failed");
		throw "PSO FAIL";
	}
}

void RenderPipelineStage::bindDescriptorsToRoot(DESCRIPTOR_USAGE usage, int usageIndex) {
	for (int i = 0; i < rootParameterDescs[usage].size(); i++) {
		DESCRIPTOR_TYPE descriptorType = getDescriptorTypeFromRootParameterDesc(rootParameterDescs[usage][i]);
		DX12Descriptor* descriptor = descriptorManager.getDescriptor(rootParameterDescs[usage][i].name + std::to_string(usageIndex), descriptorType);
		if (descriptor == nullptr) {
			// For now just ignoring because if a texture doesn't exist we'll just assume it'll be fine.
			// Different PSOs for different texturing would fix this.
			continue;
		}
		switch (descriptorType) {
		case DESCRIPTOR_TYPE_NONE:
			OutputDebugStringA("Not sure what this is");
		case DESCRIPTOR_TYPE_SRV:
			mCommandList->SetGraphicsRootDescriptorTable(rootParameterDescs[usage][i].slot,
				descriptor->gpuHandle);
			break;
		case DESCRIPTOR_TYPE_UAV:
			mCommandList->SetGraphicsRootDescriptorTable(rootParameterDescs[usage][i].slot,
				descriptor->gpuHandle);
			break;
		case DESCRIPTOR_TYPE_CBV:
			mCommandList->SetGraphicsRootDescriptorTable(rootParameterDescs[usage][i].slot,
				descriptor->gpuHandle);
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

#ifdef DEBUG
	float clear[4] = { 0.0f,0.0f,0.0f,0.0f };
	mCommandList->ClearRenderTargetView(descriptorManager.getDescriptor(renderTargetDescs[0].descriptorName + "0", DESCRIPTOR_TYPE_RTV)->cpuHandle,
		clear, 0, nullptr);
#endif // DEBUG

	mCommandList->OMSetRenderTargets(renderTargetDescs.size(),
		&descriptorManager.getDescriptor(renderTargetDescs[0].descriptorName + "0", DESCRIPTOR_TYPE_RTV)->cpuHandle,
		true,
		&descriptorManager.getAllDescriptorsOfType(DESCRIPTOR_TYPE_DSV)->at(0)->cpuHandle);
}

void RenderPipelineStage::drawRenderObjects() {
	//PIXScopedEvent(mCommandList.Get(), PIX_COLOR(0.0, 1.0, 0.0), "Draw Calls");
	int meshIndex = 0;
	for (int i = 0; i < renderObjects.size(); i++) {
		Model* model = renderObjects[i];

		bindDescriptorsToRoot(DESCRIPTOR_USAGE_PER_OBJECT, i);

		mCommandList->IASetVertexBuffers(0, 1, &model->vertexBufferView());
		mCommandList->IASetIndexBuffer(&model->indexBufferView());
		mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		for (Mesh& m : model->meshes) {
			bindDescriptorsToRoot(DESCRIPTOR_USAGE_PER_MESH, meshIndex);

			mCommandList->DrawIndexedInstanced(m.indexCount,
				1, m.startIndexLocation, m.baseVertexLocation, 0);

			meshIndex++;
		}
	}
}

void RenderPipelineStage::importMeshTextures(Mesh* m, int usageIndex) {
	for (auto textureArray : m->textures) {
		for (auto texture : textureArray.second) {
			// Assuming only 1 texture per type...
			resourceManager.importResource(textureArray.first + std::to_string(usageIndex), texture);
		}
	}
}

void RenderPipelineStage::buildMeshTexturesDescriptors(Mesh* m, int usageIndex) {
	if ((m->typeFlags & MODEL_FORMAT_DIFFUSE_TEX) == 0) {
		// Can't bind a texture if it doesn't exist.
		m->texturesBound = true;
		return;
	}
	DescriptorJob j;
	j.name = "texture_diffuse";
	j.target = "texture_diffuse" + std::to_string(usageIndex);
	j.type = DESCRIPTOR_TYPE_SRV;
	j.usage = DESCRIPTOR_USAGE_PER_MESH;
	j.usageIndex = usageIndex;
	j.srvDesc.Format = m->textures.at("texture_diffuse")[0]->MetaData.Format;
	j.srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	j.srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	j.srvDesc.Texture2D.MipLevels = -1;
	j.srvDesc.Texture2D.MostDetailedMip = 0;
	j.srvDesc.Texture2D.PlaneSlice = 0;
	j.srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	// Here's where we'd put more textures, but for now, this will be it.
	addDescriptorJob(j);

	// Flag them all as bound.
	m->texturesBound = true;
}

void RenderPipelineStage::setupRenderObjects() {
	int index = 0;
	for (auto& renderObj : renderObjects) {
		if (!renderObj->loaded) {
			return;
		}
		for (auto& mesh : renderObj->meshes) {
			if (!mesh.allTexturesLoaded()) {
				return;
			}
			if (!mesh.texturesBound) {
				importMeshTextures(&mesh, index);
				buildMeshTexturesDescriptors(&mesh, index);
				return;
			}
			index++;
		}
	}
	BuildDescriptors(stageDesc.descriptorJobs);
	allRenderObjectsSetup = true;
}

void RenderPipelineStage::addDescriptorJob(DescriptorJob j) {
	stageDesc.descriptorJobs.push_back(j);
}