#include "PipelineStage\PipelineStage.h"
#include "Tasks/PipelineStageTask.h"
#include <cassert>
#include "DX12Helper.h"
#include "ModelLoading\TextureLoader.h"

using namespace Microsoft::WRL;

PipelineStage::PipelineStage(Microsoft::WRL::ComPtr<ID3D12Device2> d3dDevice)
	: TaskQueueThread(d3dDevice), resourceManager(d3dDevice), descriptorManager(d3dDevice), constantBufferManager(d3dDevice) {
}

void PipelineStage::LoadTextures(std::vector<std::pair<std::string,std::string>> textureFiles) {
	TextureLoader& tloader = TextureLoader::getInstance();
	for (const auto& file : textureFiles) {
		 resourceManager.importResource(file.first, tloader.deferLoad(file.second, "..\\Models\\"));
	}
}

void PipelineStage::deferSetup(PipeLineStageDesc stageDesc) {
	enqueue(new	PipelineStageTaskSetup(this, stageDesc));
}

int PipelineStage::deferExecute() {
	enqueue(new PipelineStageTaskRun(this));
	return triggerFence();
}

void PipelineStage::deferUpdateConstantBuffer(std::string name, ConstantBufferData& data) {
	constantBufferManager.getConstantBuffer(name)->prepareUpdateBuffer(&data);
	enqueue(new PipelineStageTaskUpdateConstantBuffer(this, name));
}

void PipelineStage::updateConstantBuffer(std::string name) {
	constantBufferManager.getConstantBuffer(name)->updateBuffer();
}

int PipelineStage::triggerFence() {
	int dest = getFenceValue() + 1;
	enqueue(new PipelineStageTaskFence(this, dest));
	return dest;
}

void PipelineStage::deferWaitOnFence(Microsoft::WRL::ComPtr<ID3D12Fence> fence, int val) {
	enqueue(new PipelineStageTaskWaitFence(this, val, fence));
}

DX12Resource* PipelineStage::getResource(std::string name) {
	return resourceManager.getResource(name);
}

void PipelineStage::setup(PipeLineStageDesc stageDesc) {
	//This causes a race condition, if the TextureLoader doesn't finish before the setup() ends.
	LoadTextures(stageDesc.textureFiles);
	for (std::pair<std::string, DX12Resource*>& res : stageDesc.externalResources) {
		resourceManager.importResource(res.first, res.second);
	}
	renderTargetDescs = stageDesc.renderTargets;
	BuildRootSignature(rootSignature, stageDesc.rootSigDesc, rootParameterDescs);
	BuildResources(stageDesc.resourceJobs);
	BuildConstantBuffers(stageDesc.constantBufferJobs);
	BuildShaders(stageDesc.shaderFiles);
	BuildInputLayout();
	BuildPSO();
	this->stageDesc = stageDesc;
}

void PipelineStage::BuildRootSignature(Microsoft::WRL::ComPtr<ID3D12RootSignature>& rootSig, std::vector<RootParamDesc> rootSigDescs, std::vector<RootParamDesc> targetRootParamDescs[DESCRIPTOR_USAGE_MAX]) {
	std::vector<CD3DX12_ROOT_PARAMETER> rootParameters(rootSigDescs.size(), CD3DX12_ROOT_PARAMETER());
	std::vector<int> shaderRegisters(ROOT_PARAMETER_TYPE_MAX_LENGTH, 0);
	std::vector<CD3DX12_DESCRIPTOR_RANGE> tables(rootSigDescs.size(), CD3DX12_DESCRIPTOR_RANGE());

	for (int i = 0; i < rootSigDescs.size(); i++) {
		initRootParameterFromType(rootParameters[i], rootSigDescs[i], shaderRegisters, tables[i]);
	}

	auto samplers = GetStaticSamplers();

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		rootSigDescs.size(),
		rootParameters.data(),
		(UINT)samplers.size(),
		samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr) {
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		throw "Couldn't init root Sig";
	}

	md3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(), IID_PPV_ARGS(rootSig.GetAddressOf()));

	for (RootParamDesc& desc : rootSigDescs) {
		targetRootParamDescs[desc.usagePattern].push_back(desc);
	}
}

void PipelineStage::BuildDescriptors(std::vector<DescriptorJob>& descriptorJobs) {
	descriptorManager.makeDescriptors(descriptorJobs, &resourceManager, &constantBufferManager);
}

void PipelineStage::BuildConstantBuffers(std::vector<ConstantBufferJob>& constantBufferJobs) {
	for (ConstantBufferJob& job : constantBufferJobs) {
		constantBufferManager.makeConstantBuffer(job);
		resourceManager.makeFromExisting(job.name, DESCRIPTOR_TYPE_NONE, constantBufferManager.getConstantBuffer(job.name)->get(), D3D12_RESOURCE_STATE_GENERIC_READ);
		delete job.initialData;
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
		std::vector<DxcDefine> defines = DXDefine::DXDefineToDxcDefine(shaderDesc.defines);
		shaders[shaderDesc.shaderName] = compileShader(std::wstring(shaderFileString.begin(), shaderFileString.end()),
			defines, std::wstring(shaderDesc.methodName.begin(), shaderDesc.methodName.end()),
			getCompileTargetFromType(shaderDesc.type));
		shadersByType[shaderDesc.type] = shaders[shaderDesc.shaderName];
	}
}

void PipelineStage::BuildInputLayout() {
	inputLayout = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0 , 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
		{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0 , 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
		{"BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0 , 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA}
	};
}

void PipelineStage::BuildPSO() {
	throw "Can't call BuildPSO on PipelineStage object";
}

void PipelineStage::initRootParameterFromType(CD3DX12_ROOT_PARAMETER& param, RootParamDesc desc, std::vector<int>& registers, CD3DX12_DESCRIPTOR_RANGE& table) {
	switch (desc.type) {
	case ROOT_PARAMETER_TYPE_CONSTANTS:
		param.InitAsConstants(desc.numConstants, registers[ROOT_PARAMETER_TYPE_CONSTANTS]++);
		registers[ROOT_PARAMETER_TYPE_CONSTANT_BUFFER]++;
		break;
	case ROOT_PARAMETER_TYPE_CONSTANT_BUFFER:
		param.InitAsConstantBufferView(registers[ROOT_PARAMETER_TYPE_CONSTANT_BUFFER]++);
		registers[ROOT_PARAMETER_TYPE_CONSTANTS]++;
		break;
	case ROOT_PARAMETER_TYPE_SRV:
		param.InitAsShaderResourceView(registers[ROOT_PARAMETER_TYPE_SRV]++);
		break;
	case ROOT_PARAMETER_TYPE_UAV:
		param.InitAsUnorderedAccessView(registers[ROOT_PARAMETER_TYPE_UAV]++);
		break;
	case ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
		{
			int& startIndex = registers.at(getRootParamTypeFromRangeType(desc.rangeType));
			table.Init(desc.rangeType, desc.numConstants, startIndex);
			param.InitAsDescriptorTable(1, &table);
			// Here's where we'd init the other descriptor table types if we had them, but for now, 1 type per entry
			startIndex += desc.numConstants;
		}		
		break;
	default:
		break;
	}
}

std::wstring PipelineStage::getCompileTargetFromType(SHADER_TYPE type) {
	switch (type) {
	case SHADER_TYPE_VS:
		return L"vs_6_4";
	case SHADER_TYPE_PS:
		return L"ps_6_4";
	case SHADER_TYPE_CS:
		return L"cs_6_4";
	case SHADER_TYPE_GS:
		return L"gs_6_4";
	case SHADER_TYPE_MS:
		return L"ms_6_5";
	default:
		return L"";
		break;
	}
}

ROOT_PARAMETER_TYPE PipelineStage::getRootParamTypeFromRangeType(D3D12_DESCRIPTOR_RANGE_TYPE range) {
	switch (range) {
	case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
		return ROOT_PARAMETER_TYPE_SRV;
	case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
		return ROOT_PARAMETER_TYPE_UAV;
	case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
		return ROOT_PARAMETER_TYPE_CONSTANT_BUFFER;
	case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
		throw "Unhandled";
	default:
		throw "unhandled";
	}
}

DESCRIPTOR_TYPE PipelineStage::getDescriptorTypeFromRootParameterDesc(RootParamDesc desc) {
	ROOT_PARAMETER_TYPE type = desc.type;
	if (type == ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
		type = getRootParamTypeFromRangeType(desc.rangeType);
	}
	switch (type) {
	case ROOT_PARAMETER_TYPE_CONSTANTS:
		throw "Can't do constants";
	case ROOT_PARAMETER_TYPE_CONSTANT_BUFFER:
		return DESCRIPTOR_TYPE_CBV;
	case ROOT_PARAMETER_TYPE_SRV:
		return DESCRIPTOR_TYPE_SRV;
	case ROOT_PARAMETER_TYPE_UAV:
		return DESCRIPTOR_TYPE_UAV;
	default:
		throw "Invalid Type Given";
	}
}

void PipelineStage::resetCommandList() {
	mDirectCmdListAlloc->Reset();
	mCommandList->Reset(mDirectCmdListAlloc.Get(), PSO.Get());
}

void PipelineStage::bindDescriptorHeaps() {
	std::vector<ID3D12DescriptorHeap*> descHeaps = descriptorManager.getAllBindableHeaps();
	mCommandList->SetDescriptorHeaps(descHeaps.size(), descHeaps.data());
}

void PipelineStage::setResourceStates() {
	auto stateResourcePairVector = descriptorManager.requiredResourceStates();
	for (auto& stateResourcePair : stateResourcePairVector) {
		stateResourcePair.second->changeState(mCommandList, stateResourcePair.first);
	}
}
