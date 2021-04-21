#include "PipelineStage\RenderPipelineStage.h"
#include "DescriptorClasses\DescriptorManager.h"
#include "ModelLoading/ModelLoader.h"
#include <string>
#include "Settings.h"
#include "MeshletModel.h"
#include <d3dx12.h>
#include <DirectXCollision.h>
#include "ResourceDecay.h"

RenderPipelineStage::RenderPipelineStage(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice, RenderPipelineDesc renderDesc, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect)
	: PipelineStage(d3dDevice), renderStageDesc(renderDesc) {
	this->viewport = viewport;
	this->scissorRect = scissorRect;
	frustrumCull = true;
}

RenderPipelineStage::~RenderPipelineStage() {
}

void RenderPipelineStage::setup(PipeLineStageDesc stageDesc) {
	PipelineStage::setup(stageDesc);
	buildDescriptors(stageDesc.descriptorJobs);
	buildPSO();
	for (int i = 0; i < DESCRIPTOR_USAGE_MAX; i++) {
		auto iter = rootParameterDescs[i].begin();
		while (iter != rootParameterDescs[i].end()) {
			if (iter->slot == renderStageDesc.perObjTransformCBSlot) {
				iter = rootParameterDescs[i].erase(iter);
			}
			else if (iter->slot == renderStageDesc.perMeshTransformCBSlot) {
				iter = rootParameterDescs[i].erase(iter);
			}
			else if (iter->slot == renderStageDesc.perMeshTextureSlot) {
				iter = rootParameterDescs[i].erase(iter);
			}
			else {
				iter++;
			}
		}
	}
}

void RenderPipelineStage::execute() {
	resetCommandList();

	PIXBeginEvent(mCommandList.Get(), PIX_COLOR(0, 255, 0), stageDesc.name.c_str());

	performTransitionsIn();

	bindDescriptorHeaps();
	setResourceStates();

	mCommandList->RSSetViewports(1, &viewport);
	mCommandList->RSSetScissorRects(1, &scissorRect);

	mCommandList->SetGraphicsRootSignature(rootSignature.Get());

	bindDescriptorsToRoot(DESCRIPTOR_USAGE_ALL);
	bindDescriptorsToRoot(DESCRIPTOR_USAGE_PER_PASS);

	bindRenderTarget();

	draw();

	performTransitionsOut();

	PIXEndEvent(mCommandList.Get());

	ThrowIfFailed(mCommandList->Close());
}

void RenderPipelineStage::buildPSO() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPSO;
	ZeroMemory(&graphicsPSO, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	graphicsPSO.InputLayout = { inputLayout.data(),(UINT)inputLayout.size() };
	graphicsPSO.pRootSignature = rootSignature.Get();
	graphicsPSO.VS.pShaderBytecode = shadersByType[SHADER_TYPE_VS]->GetBufferPointer();
	graphicsPSO.VS.BytecodeLength = shadersByType[SHADER_TYPE_VS]->GetBufferSize();
	graphicsPSO.PS.pShaderBytecode = shadersByType[SHADER_TYPE_PS]->GetBufferPointer();
	graphicsPSO.PS.BytecodeLength = shadersByType[SHADER_TYPE_PS]->GetBufferSize();
	graphicsPSO.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	graphicsPSO.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	graphicsPSO.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	graphicsPSO.DepthStencilState.DepthEnable = renderStageDesc.usesDepthTex;
	graphicsPSO.SampleMask = UINT_MAX;
	graphicsPSO.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	graphicsPSO.NumRenderTargets = (UINT)renderStageDesc.renderTargets.size();
	for (int i = 0; i < renderStageDesc.renderTargets.size(); i++) {
		graphicsPSO.RTVFormats[i] = descriptorManager.getDescriptor(IndexedName(renderStageDesc.renderTargets[i].descriptorName, 0), DESCRIPTOR_TYPE_RTV)->resourceTarget->getFormat();
	}
	graphicsPSO.SampleDesc.Count = 1;
	graphicsPSO.SampleDesc.Quality = 0;
	graphicsPSO.DSVFormat = renderStageDesc.usesDepthTex ? DEPTH_TEXTURE_DSV_FORMAT : DXGI_FORMAT_UNKNOWN;

	if (FAILED(md3dDevice->CreateGraphicsPipelineState(&graphicsPSO, IID_PPV_ARGS(&PSO)))) {
		OutputDebugStringA("PSO Setup Failed");
		throw "PSO FAIL";
	}
}

std::vector<DescriptorJob> RenderPipelineStage::buildMeshTexturesDescriptorJobs(Mesh* m) {
	std::vector<DescriptorJob> descriptorJobs;
	for (const auto& texMap : renderStageDesc.textureToDescriptor) {
		MODEL_FORMAT textureType = texMap.first;
		DX12Resource* texture = nullptr;
		if ((m->typeFlags & textureType) != 0) {
			texture = m->textures.at(textureType).get();
		}
		else {
			texture = resourceManager.getResource(renderStageDesc.defaultTextures.at(textureType));
		}
		DescriptorJob job(texMap.second, texture, DESCRIPTOR_TYPE_SRV, true, 0, DESCRIPTOR_USAGE_SYSTEM_DEFINED);
		descriptorJobs.push_back(job);
	}
	return descriptorJobs;
}

void RenderPipelineStage::buildInputLayout() {
	PipelineStage::buildInputLayout();
}

void RenderPipelineStage::bindDescriptorsToRoot(DESCRIPTOR_USAGE usage, int usageIndex, std::vector<RootParamDesc> curRootParamDescs[DESCRIPTOR_USAGE_MAX]) {
	if (curRootParamDescs == nullptr) {
		curRootParamDescs = rootParameterDescs;
	}

	for (int i = 0; i < curRootParamDescs[usage].size(); i++) {
		DESCRIPTOR_TYPE descriptorType = getDescriptorTypeFromRootParameterDesc(curRootParamDescs[usage][i]);
		if (curRootParamDescs[usage][i].type == ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
			DX12Descriptor* descriptor = descriptorManager.getDescriptor(IndexedName(curRootParamDescs[usage][i].name, usageIndex), descriptorType);
			if (descriptor == nullptr) {
				// For now just ignoring because if a texture doesn't exist we'll just assume it'll be fine.
				// Different PSOs for different texturing would fix this.
				continue;
			}
			switch (descriptorType) {
			case DESCRIPTOR_TYPE_NONE:
				OutputDebugStringA("Not sure what this is");
				break;
			case DESCRIPTOR_TYPE_SRV:
				mCommandList->SetGraphicsRootDescriptorTable(curRootParamDescs[usage][i].slot,
					descriptor->gpuHandle);
				break;
			case DESCRIPTOR_TYPE_UAV:
				mCommandList->SetGraphicsRootDescriptorTable(curRootParamDescs[usage][i].slot,
					descriptor->gpuHandle);
				break;
			case DESCRIPTOR_TYPE_CBV:
				mCommandList->SetGraphicsRootDescriptorTable(curRootParamDescs[usage][i].slot,
					descriptor->gpuHandle);
				break;
			default:
				throw "Don't know what to do here.";
				break;
			}
		}
		else if (descriptorType == DESCRIPTOR_TYPE_CBV) {
			mCommandList->SetGraphicsRootConstantBufferView(curRootParamDescs[usage][i].slot, constantBufferManager.getConstantBuffer(IndexedName(curRootParamDescs[usage][i].name, usageIndex))->get(gFrameIndex)->GetGPUVirtualAddress());
		}
		else {
			DX12Resource* resource = resourceManager.getResource(curRootParamDescs[usage][i].name);
			if (resource == nullptr) {
				continue;
			}
			D3D12_GPU_VIRTUAL_ADDRESS resPtr = resource->get()->GetGPUVirtualAddress();
			switch (descriptorType) {
			case DESCRIPTOR_TYPE_NONE:
				OutputDebugStringA("Not sure what this is");
				break;
			case DESCRIPTOR_TYPE_SRV:
				mCommandList->SetGraphicsRootShaderResourceView(curRootParamDescs[usage][i].slot, resPtr);
				break;
			case DESCRIPTOR_TYPE_UAV:
				mCommandList->SetGraphicsRootUnorderedAccessView(curRootParamDescs[usage][i].slot, resPtr);
				break;
			default:
				throw "Don't know what to do here.";
				break;
			}
		}
	}
}

void RenderPipelineStage::bindRenderTarget() {

	D3D12_CPU_DESCRIPTOR_HANDLE* depthHandle = nullptr;
	if (renderStageDesc.usesDepthTex) {
		depthHandle = &descriptorManager.getAllDescriptorsOfType(DESCRIPTOR_TYPE_DSV)->at(0)->cpuHandle;

		mCommandList->ClearDepthStencilView(
			*depthHandle,
			D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
			1.0f, 0, 0, nullptr);
	}

	float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	for (auto& target : renderStageDesc.renderTargets) {
		mCommandList->ClearRenderTargetView(descriptorManager.getDescriptor(IndexedName(target.descriptorName, 0), DESCRIPTOR_TYPE_RTV)->cpuHandle,
			clearColor,
			0,
			nullptr);
	}

	mCommandList->OMSetRenderTargets((UINT)renderStageDesc.renderTargets.size(),
		&descriptorManager.getDescriptor(IndexedName(renderStageDesc.renderTargets[0].descriptorName, 0), DESCRIPTOR_TYPE_RTV)->cpuHandle,
		true,
		depthHandle);
}

bool RenderPipelineStage::performsTransitions() {
	return true;
}

void RenderPipelineStage::performTransitionsIn() {
	if (transitionsIn.size() > 0) {
		mCommandList->ResourceBarrier((UINT)transitionsIn.size(), transitionsIn.data());
	}
}

void RenderPipelineStage::performTransitionsOut() {
	if (transitionsOut.size() > 0) {
		mCommandList->ResourceBarrier((UINT)transitionsOut.size(), transitionsOut.data());
	}
}

void RenderPipelineStage::addTransitionIn(DX12Resource* res, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter) {
	transitionsIn.push_back(CD3DX12_RESOURCE_BARRIER::Transition(res->get(), stateBefore, stateAfter));
}

void RenderPipelineStage::addTransitionOut(DX12Resource* res, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter) {
	transitionsOut.push_back(CD3DX12_RESOURCE_BARRIER::Transition(res->get(), stateBefore, stateAfter));
}

std::vector<std::pair<D3D12_RESOURCE_STATES, DX12Resource*>> RenderPipelineStage::getRequiredResourceStates() {
	auto requiredStates = PipelineStage::getRequiredResourceStates();
	if (renderStageDesc.supportsVRS) {
		requiredStates.push_back(std::make_pair(D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE, resourceManager.getResource(renderStageDesc.VrsTextureName)));
	}
	return requiredStates;
}