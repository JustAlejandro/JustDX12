#include "PipelineStage\PipelineStage.h"
#include <cassert>
#include "DX12Helper.h"
#include <AtlBase.h>
#include <atlconv.h>
using namespace Microsoft::WRL;

PipelineStage::PipelineStage(Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice)
	: TaskQueueThread(d3dDevice), resourceManager(d3dDevice), descriptorManager(d3dDevice) {
}

void PipelineStage::setup(PipeLineStageDesc stageDesc) {
	for (std::pair<std::string, DX12Resource*>& res : stageDesc.externalResources) {
		resourceManager.importResource(res.first, res.second);
	}
	BuildRootSignature(stageDesc);
	BuildDescriptors(stageDesc.descriptorJobs);
	BuildShaders(stageDesc.shaderFiles);
	BuildInputLayout();
	BuildPSO();
}

void PipelineStage::BuildRootSignature(PipeLineStageDesc stageDesc) {
	assert(stageDesc.samplerDesc.size() == 0);
	std::vector<CD3DX12_ROOT_PARAMETER> rootParameters(stageDesc.rootSigDesc.size(), CD3DX12_ROOT_PARAMETER());
	std::vector<int> shaderRegisters(ROOT_PARAMETER_TYPE_MAX_LENGTH, 0);
	for (int i = 0; i < stageDesc.rootSigDesc.size(); i++) {
		initRootParameterFromType(rootParameters[i], stageDesc.rootSigDesc[i], shaderRegisters);
	}

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		stageDesc.rootSigDesc.size(),
		rootParameters.data(),
		0,
		nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr) {
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}

	device->CreateRootSignature(0, serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(), IID_PPV_ARGS(rootSignature.GetAddressOf()));
	rootParameterDescs = stageDesc.rootSigDesc;
}

void PipelineStage::BuildDescriptors(std::vector<std::vector<DescriptorJob>>& descriptorJobs) {
	for (std::vector<DescriptorJob>& jobVec : descriptorJobs) {
		descriptorManager.makeDescriptorHeap(jobVec);
	}
}

void PipelineStage::BuildResources(std::vector<ResourceJob>& resourceJobs) {
	for (ResourceJob& job : resourceJobs) {
		resourceManager.makeResource(job);
	}
}

void PipelineStage::BuildShaders(std::vector<ShaderDesc> shaderDescs) {
	for (ShaderDesc& shaderDesc : shaderDescs) {
		std::string shaderFileString = ("..\\Shaders\\" + shaderDesc.fileName);
		shaders[shaderDesc.shaderName] = compileShader(std::wstring(shaderFileString.begin(), shaderFileString.end()),
			shaderDesc.defines, shaderDesc.methodName, getCompileTargetFromType(shaderDesc.type));
		shadersByType[shaderDesc.type] = shaders[shaderDesc.shaderName];
	}
}

void PipelineStage::BuildInputLayout() {
	inputLayout = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0 , 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA}
	};
}

void PipelineStage::BuildPSO() {
	throw "Can't call BuildPSO on PipelineStage object";
}

void PipelineStage::initRootParameterFromType(CD3DX12_ROOT_PARAMETER& param, RootParamDesc desc, std::vector<int>& registers) {
	switch (desc.type) {
	case ROOT_PARAMETER_TYPE_CONSTANTS:
		param.InitAsConstants(desc.numConstants, registers[ROOT_PARAMETER_TYPE_CONSTANTS]++);
		break;
	case ROOT_PARAMETER_TYPE_CONSTANT_BUFFER:
		param.InitAsConstantBufferView(registers[ROOT_PARAMETER_TYPE_CONSTANT_BUFFER]++);
		break;
	case ROOT_PARAMETER_TYPE_SRV:
		param.InitAsShaderResourceView(registers[ROOT_PARAMETER_TYPE_SRV]++);
		break;
	case ROOT_PARAMETER_TYPE_UAV:
		param.InitAsUnorderedAccessView(registers[ROOT_PARAMETER_TYPE_UAV]++);
		break;
	case ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
	{
		CD3DX12_DESCRIPTOR_RANGE table;
		table.Init(desc.rangeType, desc.numConstants, registers[ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE]++);
		param.InitAsDescriptorTable(1, &table);
	}
		break;
	default:
		break;
	}
}

std::string PipelineStage::getCompileTargetFromType(SHADER_TYPE type) {
	switch (type) {
	case SHADER_TYPE_VS:
		return "vs_5_0";
	case SHADER_TYPE_PS:
		return "ps_5_0";
	case SHADER_TYPE_CS:
		return "cs_5_0";
	default:
		return "";
		break;
	}
}

DESCRIPTOR_TYPE PipelineStage::getDescriptorTypeFromRootParameterType(ROOT_PARAMETER_TYPE type) {
	switch (type) {
	case ROOT_PARAMETER_TYPE_CONSTANTS:
		throw "Can't do constants";
	case ROOT_PARAMETER_TYPE_CONSTANT_BUFFER:
		throw "can't do constant buffers";
	case ROOT_PARAMETER_TYPE_SRV:
		return DESCRIPTOR_TYPE_SRV;
	case ROOT_PARAMETER_TYPE_UAV:
		return DESCRIPTOR_TYPE_UAV;
	case ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
		throw "Can't do descriptor_table";
	default:
		throw "Invalid Type Given";
	}
}

void PipelineStage::resetCommandList() {
	mDirectCmdListAlloc->Reset();
	mCommandList->Reset(mDirectCmdListAlloc.Get(), PSO.Get());
}

void PipelineStage::bindDescriptorHeaps() {
	std::vector<ID3D12DescriptorHeap*> descHeaps = descriptorManager.getAllHeaps();
	mCommandList->SetDescriptorHeaps(descHeaps.size(), descHeaps.data());
}

void PipelineStage::setResourceStates() {
	auto stateResourcePairVector = descriptorManager.requiredResourceStates();
	for (auto& stateResourcePair : stateResourcePairVector) {
		stateResourcePair.second->changeState(mCommandList, stateResourcePair.first);
	}
}
