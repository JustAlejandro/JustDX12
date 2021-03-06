#include "PipelineStage\PipelineStage.h"
#include "Tasks/PipelineStageTask.h"
#include <cassert>
#include "DX12Helper.h"
#include "ModelLoading\TextureLoader.h"
#include <set>
#include "ResourceDecay.h"

using namespace Microsoft::WRL;

PipelineStage::PipelineStage(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice, D3D12_COMMAND_LIST_TYPE cmdListType)
	: DX12TaskQueueThread(d3dDevice, cmdListType), resourceManager(d3dDevice), descriptorManager(d3dDevice), constantBufferManager(d3dDevice) {
	for (int i = 0; i < CPU_FRAME_COUNT; i++) {
		frameResourceArray.push_back(std::make_unique<FrameResource>(md3dDevice.Get(), cmdListType));
	}
}

// To be able to build multiple command lists at once we need all the stages to agree on what transitions happen when.
// Return the initial transistions needed to put us in the cycle we want.
std::vector<CD3DX12_RESOURCE_BARRIER> PipelineStage::setupResourceTransitions(std::vector<std::vector<PipelineStage*>> stages) {
	std::set<DX12Resource*> resourcesToProcess;
	std::vector<std::unordered_map<DX12Resource*, D3D12_RESOURCE_STATES>> desiredStatesByStageGroup;
	std::vector<std::pair<DX12Resource*, D3D12_RESOURCE_STATES>> requiredInitialStates;

	// Step 1: find all the resources that need to be transitioned across stages.
	for (std::vector<PipelineStage*>& stageList : stages) {
		// Using a set so that we don't have duplicates.
		std::set<std::pair<D3D12_RESOURCE_STATES, DX12Resource*>> requiredStates;
		std::unordered_map<DX12Resource*, D3D12_RESOURCE_STATES> stateMap;
		for (auto& stage : stageList) {
			auto states = stage->getRequiredResourceStates();
			for (auto& state : states) {
				// Check to make sure that if multiple stages want the same resource, they want the same state.
				if (stateMap.find(state.second) != stateMap.end()) {
					if (stateMap[state.second] != state.first) {
						throw "Can't complete this transition";
					}
				}
				else {
					stateMap[state.second] = state.first;
				}
			}
			for (auto& state : states) {
				requiredStates.insert(state);
			}
		}

		std::unordered_map<DX12Resource*, D3D12_RESOURCE_STATES> desiredStates;
		for (auto& state : requiredStates) {
			if (!state.second->local) {
				desiredStates[state.second] = state.first;
				resourcesToProcess.insert(state.second);
			}
		}
		desiredStatesByStageGroup.push_back(desiredStates);
	}

	// Step 2: process each resource and its needed transitions
	for (auto& res : resourcesToProcess) {
		D3D12_RESOURCE_STATES initState;
		// Find the last state that the resource will be in per loop and make that the expected initial state.
		for (auto iter = desiredStatesByStageGroup.rbegin(); iter != desiredStatesByStageGroup.rend(); iter++) {
			if (iter->find(res) != iter->end()) {
				initState = iter->at(res);
				break;
			}
		}
		requiredInitialStates.push_back(std::make_pair(res, initState));

		// Step 2.5: Make each pipeline stage transition into/out of the required states
		D3D12_RESOURCE_STATES prevState = initState;
		for (int i = 0; i < stages.size(); i++) {
			auto desiredState = desiredStatesByStageGroup[i].find(res);
			if (desiredState != desiredStatesByStageGroup[i].end()) {
				if (desiredState->second == prevState) {
					continue;
				}
				// Try to see if a prior stage group can handle the transition. (Unless that stage wants the resource)
				bool found = false;
				// Trips if triggering a transition would break a stage.
				bool conflicts = false;
				for (int j = i - 1; j >= 0; j--) {
					for (auto& stage : stages[j]) {
						if (!found && !conflicts && stage->performsTransitions()) {
							stage->addTransitionOut(res, prevState, desiredState->second);
							prevState = desiredState->second;
							found = true;
						}
					}

					if (desiredStatesByStageGroup[j].find(res) != desiredStatesByStageGroup[j].end()) {
						conflicts = true;
					}
				}

				if (!found) {
					for (auto& stage : stages[i]) {
						if (!stage->performsTransitions()) {
							continue;
						}
						auto requiredStates = stage->getRequiredResourceStates();
						for (auto& states : requiredStates) {
							if (res == states.second) {
								if (found) {
									throw "Multiple stages require same state, synchronization required";
								}
								stage->addTransitionIn(res, prevState, desiredState->second);
								prevState = desiredState->second;
								found = true;
								break;
							}
						}
					}
				}

				if (!found) {
					throw "Couldn't solve this transition";
				}
			}
		}
	}

	std::vector<CD3DX12_RESOURCE_BARRIER> requiredInitialTranisitions;
	for (auto& initialState : requiredInitialStates) {
		initialState.first->changeStateDeferred(initialState.second, requiredInitialTranisitions);
	}
	return requiredInitialTranisitions;
}

void PipelineStage::deferSetup(PipeLineStageDesc stageDesc) {
	enqueue(new	PipelineStageTaskSetup(this, stageDesc));
}

void PipelineStage::setup(PipeLineStageDesc stageDesc) {
	SetThreadDescription(GetCurrentThread(), std::wstring(stageDesc.name.begin(), stageDesc.name.end()).c_str());
	for (auto& frameRes : frameResourceArray) {
		SetName(frameRes->CmdListAlloc.Get(), (std::wstring(stageDesc.name.begin(), stageDesc.name.end()) + L" Command List Allocator").c_str());
	}
	//This causes a race condition, if the TextureLoader doesn't finish before the setup() ends.
	loadTextures(stageDesc.textureFiles);
	for (auto& cb : stageDesc.externalConstantBuffers) {
		constantBufferManager.importConstantBuffer(cb.first, cb.second);
	}
	for (std::pair<std::string, DX12Resource*>& res : stageDesc.externalResources) {
		resourceManager.importResource(res.first, res.second);
	}
	buildRootSignature(rootSignature, stageDesc.rootSigDesc, rootParameterDescs);
	buildResources(stageDesc.resourceJobs);
	buildConstantBuffers(stageDesc.constantBufferJobs);
	buildShaders(stageDesc.shaderFiles);
	buildInputLayout();
	this->stageDesc = stageDesc;
}

HANDLE PipelineStage::deferExecute() {
	enqueue(new PipelineStageTaskRun(this));
	return deferSetCpuEvent();
}

DX12ConstantBuffer* PipelineStage::getConstantBuffer(IndexedName indexName) {
	return constantBufferManager.getConstantBuffer(indexName);
}

DX12Resource* PipelineStage::getResource(std::string name) {
	return resourceManager.getResource(name);
}

void PipelineStage::deferUpdateConstantBuffer(std::string name, ConstantBufferData& data, int usageIndex) {
	constantBufferManager.getConstantBuffer(IndexedName(name, usageIndex))->prepareUpdateBuffer(&data);
	enqueue(new PipelineStageTaskUpdateConstantBuffer(this, IndexedName(name, usageIndex)));
}

void PipelineStage::updateConstantBuffer(IndexedName indexName) {
	constantBufferManager.getConstantBuffer(indexName)->updateBuffer(gFrameIndex);
}

int PipelineStage::triggerFence() {
	int dest = getFenceValue() + 1;
	enqueue(new PipelineStageTaskFence(this, dest));
	return dest;
}

void PipelineStage::deferWaitOnFence(Microsoft::WRL::ComPtr<ID3D12Fence> fence, int val) {
	enqueue(new PipelineStageTaskWaitFence(this, val, fence));
}

void PipelineStage::buildRootSignature(Microsoft::WRL::ComPtr<ID3D12RootSignature>& rootSig, std::vector<RootParamDesc> rootSigDescs, std::vector<RootParamDesc> targetRootParamDescs[DESCRIPTOR_USAGE_MAX]) {
	std::vector<CD3DX12_ROOT_PARAMETER> rootParameters(rootSigDescs.size(), CD3DX12_ROOT_PARAMETER());
	std::vector<std::vector<int>> shaderRegisters;
	for (auto& rootSigDesc : rootSigDescs) {
		if (rootSigDesc.space + 1 > shaderRegisters.size()) {
			shaderRegisters.resize(rootSigDesc.space + 1, std::vector<int>());
		}
	}
	for (auto& registerSpace : shaderRegisters) {
		registerSpace = std::vector<int>(ROOT_PARAMETER_TYPE_MAX_LENGTH, 0);
	}
	std::vector<CD3DX12_DESCRIPTOR_RANGE> tables(rootSigDescs.size(), CD3DX12_DESCRIPTOR_RANGE());

	for (int i = 0; i < rootSigDescs.size(); i++) {
		initRootParameterFromType(rootParameters[i], rootSigDescs[i], shaderRegisters, tables[i]);
	}

	auto samplers = GetStaticSamplers();

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		(UINT)rootSigDescs.size(),
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

void PipelineStage::buildDescriptors(std::vector<DescriptorJob>& descriptorJobs) {
	descriptorManager.makeDescriptors(descriptorJobs, &resourceManager, &constantBufferManager);
}

void PipelineStage::buildConstantBuffers(std::vector<ConstantBufferJob>& constantBufferJobs) {
	for (ConstantBufferJob& job : constantBufferJobs) {
		constantBufferManager.makeConstantBuffer(job);
		delete job.initialData;
	}
}

void PipelineStage::buildResources(std::vector<ResourceJob>& resourceJobs) {
	for (ResourceJob& job : resourceJobs) {
		resourceManager.makeResource(job);
	}
}

void PipelineStage::buildShaders(std::vector<ShaderDesc> shaderDescs) {
	for (ShaderDesc& shaderDesc : shaderDescs) {
		std::string shaderFileString = ("..\\Shaders\\" + shaderDesc.fileName);
		std::vector<DxcDefine> defines = DXDefine::DXDefineToDxcDefine(shaderDesc.defines);
		shaders[shaderDesc.shaderName] = compileShader(std::wstring(shaderFileString.begin(), shaderFileString.end()),
			defines, std::wstring(shaderDesc.methodName.begin(), shaderDesc.methodName.end()),
			getCompileTargetFromType(shaderDesc.type));
		shadersByType[shaderDesc.type] = shaders[shaderDesc.shaderName];
	}
}

void PipelineStage::buildInputLayout() {
	inputLayout = {
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0 , 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
		{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0 , 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
		{"BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0 , 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA}
	};
}

void PipelineStage::buildPSO() {
	throw "Can't call BuildPSO on PipelineStage object";
}

void PipelineStage::loadTextures(std::vector<std::pair<std::string,std::string>> textureFiles) {
	TextureLoader& tloader = TextureLoader::getInstance();
	for (const auto& file : textureFiles) {
		auto tex = tloader.deferLoad(file.second, "..\\Models\\");
		// Hanging here is suboptimal, but if we didn't hang before texture load then we can't perform this PipelineStage's actions
		while (!tex->get()) {
			ResourceDecay::checkDestroy();
		}
		ownedTextures.push_back(tex);
		resourceManager.importResource(file.first, tex.get());
	}
}

void PipelineStage::importResource(std::string name, DX12Resource* resource) {
	resourceManager.importResource(name, resource);
}

void PipelineStage::importResource(std::string name, ID3D12Resource* resource) {
	resourceManager.makeFromExisting(name, DESCRIPTOR_TYPE_FLAG_RT_ACCEL_STRUCT, resource, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
}

void PipelineStage::resetCommandList() {
	mDirectCmdListAlloc = frameResourceArray[gFrameIndex].get()->CmdListAlloc;
	ThrowIfFailed(mDirectCmdListAlloc->Reset());
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), PSO.Get()));
	SetName(mCommandList.Get(), (std::wstring(stageDesc.name.begin(), stageDesc.name.end()) + L" Command List").c_str());
}

void PipelineStage::bindDescriptorHeaps() {
	std::vector<ID3D12DescriptorHeap*> descHeaps = descriptorManager.getAllBindableHeaps();
	mCommandList->SetDescriptorHeaps((UINT)descHeaps.size(), descHeaps.data());
}

void PipelineStage::setResourceStates() {
	auto stateResourcePairVector = getRequiredResourceStates();
	std::vector<CD3DX12_RESOURCE_BARRIER> transitionQueue;
	for (auto& stateResourcePair : stateResourcePairVector) {
		if (stateResourcePair.second->local) {
			stateResourcePair.second->changeStateDeferred(stateResourcePair.first, transitionQueue);
		}
	}
	if (transitionQueue.size() > 0) {
		mCommandList->ResourceBarrier((UINT)transitionQueue.size(), transitionQueue.data());
	}
}

bool PipelineStage::performsTransitions() {
	return false;
}

void PipelineStage::performTransitionsIn() {
	throw "Not Available";
}

void PipelineStage::performTransitionsOut() {
	throw "Not Available";
}

void PipelineStage::addTransitionIn(DX12Resource* res, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter) {
	throw "Not Available";
}

void PipelineStage::addTransitionOut(DX12Resource* res, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter) {
	throw "Not Available";
}

void PipelineStage::initRootParameterFromType(CD3DX12_ROOT_PARAMETER& param, RootParamDesc desc, std::vector<std::vector<int>>& registers, CD3DX12_DESCRIPTOR_RANGE& table) {
	switch (desc.type) {
	case ROOT_PARAMETER_TYPE_CONSTANTS:
		param.InitAsConstants(desc.numConstants, registers[desc.space][ROOT_PARAMETER_TYPE_CONSTANTS]++, desc.space);
		registers[desc.space][ROOT_PARAMETER_TYPE_CONSTANT_BUFFER]++;
		break;
	case ROOT_PARAMETER_TYPE_CONSTANT_BUFFER:
		param.InitAsConstantBufferView(registers[desc.space][ROOT_PARAMETER_TYPE_CONSTANT_BUFFER]++, desc.space);
		registers[desc.space][ROOT_PARAMETER_TYPE_CONSTANTS]++;
		break;
	case ROOT_PARAMETER_TYPE_SRV:
		param.InitAsShaderResourceView(registers[desc.space][ROOT_PARAMETER_TYPE_SRV]++, desc.space);
		break;
	case ROOT_PARAMETER_TYPE_UAV:
		param.InitAsUnorderedAccessView(registers[desc.space][ROOT_PARAMETER_TYPE_UAV]++, desc.space);
		break;
	case ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
		{
			int& startIndex = registers[desc.space].at(getRootParamTypeFromRangeType(desc.rangeType));
			table.Init(desc.rangeType, desc.numConstants, startIndex, desc.space);
			param.InitAsDescriptorTable(1, &table);
			// Here's where we'd init the other descriptor table types if we had them, but for now, 1 type per entry
			startIndex += desc.numConstants;
			if (desc.numConstants == -1) {
				// Unbounded, so trying to bind another descriptor here should cause a crash.
				startIndex = INT_MIN;
			}
		}		
		break;
	default:
		break;
	}
}

std::wstring PipelineStage::getCompileTargetFromType(SHADER_TYPE type) {
	switch (type) {
	case SHADER_TYPE_VS:
		return L"vs_6_5";
	case SHADER_TYPE_PS:
		return L"ps_6_5";
	case SHADER_TYPE_CS:
		return L"cs_6_5";
	case SHADER_TYPE_GS:
		return L"gs_6_5";
	case SHADER_TYPE_AS:
		return L"as_6_5";
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

std::vector<std::pair<D3D12_RESOURCE_STATES, DX12Resource*>> PipelineStage::getRequiredResourceStates() {
	return descriptorManager.getRequiredResourceStates();
}
