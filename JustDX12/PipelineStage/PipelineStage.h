#pragma once
#define NOMINMAX
#include <wrl.h>
#include <d3d12.h>
#include "Tasks\TaskQueueThread.h"
#include "DescriptorClasses\DescriptorManager.h"
#include "ResourceClasses\ResourceManager.h"
#include "ConstantBufferManager.h"
#include <dxcapi.h>
#include <vector>
#include "FrameResource.h"

enum ROOT_PARAMETER_TYPE {
	ROOT_PARAMETER_TYPE_CONSTANTS = 0,
	ROOT_PARAMETER_TYPE_CONSTANT_BUFFER = 1,
	ROOT_PARAMETER_TYPE_SRV = 2,
	ROOT_PARAMETER_TYPE_UAV = 3,
	ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE = 4,
	ROOT_PARAMETER_TYPE_MAX_LENGTH = 5
};

enum SHADER_TYPE {
	SHADER_TYPE_VS = 0,
	SHADER_TYPE_PS,
	SHADER_TYPE_CS,
	SHADER_TYPE_GS,
	SHADER_TYPE_AS,
	SHADER_TYPE_MS
};

struct RootParamDesc {
	std::string name;
	ROOT_PARAMETER_TYPE type;
	int slot = 0;
	D3D12_DESCRIPTOR_RANGE_TYPE rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	int numConstants = 1;
	DESCRIPTOR_USAGE usagePattern = DESCRIPTOR_USAGE_ALL;
	RootParamDesc() = default;
	RootParamDesc(std::string name, ROOT_PARAMETER_TYPE type, int slot = 0, D3D12_DESCRIPTOR_RANGE_TYPE rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV, int numConstants = 1, DESCRIPTOR_USAGE usagePattern = DESCRIPTOR_USAGE_ALL) {
		this->name = name;
		this->type = type;
		this->slot = slot;
		this->rangeType = rangeType;
		this->numConstants = numConstants;
		this->usagePattern = usagePattern;
	}
};

struct RenderTargetDesc {
	std::string descriptorName;
	int slot;
	RenderTargetDesc() = default;
	RenderTargetDesc(std::string descriptorName, int slot) {
		this->descriptorName = descriptorName;
		this->slot = slot;
	}
};

struct ShaderDesc {
	std::string fileName;
	std::string shaderName;
	std::string methodName;
	SHADER_TYPE type;
	std::vector<DXDefine> defines;
	ShaderDesc() = default;
	ShaderDesc(std::string fileName, std::string shaderName, std::string methodName, SHADER_TYPE type, std::vector<DXDefine> defines) {
		this->fileName = fileName;
		this->shaderName = shaderName;
		this->methodName = methodName;
		this->type = type;
		this->defines = defines;
	}
};

struct PipeLineStageDesc {
	std::string name;
	std::vector<RootParamDesc> rootSigDesc;
	std::vector<DescriptorJob> descriptorJobs;
	std::vector<ResourceJob> resourceJobs;
	std::vector<ConstantBufferJob> constantBufferJobs;
	std::vector<ShaderDesc> shaderFiles;
	std::vector<std::pair<IndexedName, DX12ConstantBuffer*>> externalConstantBuffers;
	std::vector<std::pair<std::string, DX12Resource*>> externalResources;
	std::vector<std::pair<std::string, std::string>> textureFiles;
	std::vector<RenderTargetDesc> renderTargets;
};

class PipelineStage : public TaskQueueThread {
public:
	void deferSetup(PipeLineStageDesc stageDesc);
	HANDLE deferExecute();
	void deferUpdateConstantBuffer(std::string name, ConstantBufferData& data, int usageIndex = 0);
	void updateConstantBuffer(IndexedName indexName);
	int triggerFence();
	void nextFrame();
	void deferWaitOnFence(Microsoft::WRL::ComPtr<ID3D12Fence> fence, int val);
	DX12ConstantBuffer* getConstantBuffer(IndexedName indexName);
	DX12Resource* getResource(std::string name);
	void importResource(std::string name, DX12Resource* resource);
	void importResource(std::string name, ID3D12Resource* resource);

	static std::vector<CD3DX12_RESOURCE_BARRIER> setupResourceTransitions(std::vector<std::vector<PipelineStage*>> stages);

	virtual void setup(PipeLineStageDesc stageDesc);
	virtual void Execute() = 0;

protected:
	PipelineStage(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice, D3D12_COMMAND_LIST_TYPE cmdListType = D3D12_COMMAND_LIST_TYPE_DIRECT);
	void LoadTextures(std::vector<std::pair<std::string, std::string>> textureFiles);
	void BuildRootSignature(Microsoft::WRL::ComPtr<ID3D12RootSignature>& rootSig, std::vector<RootParamDesc> rootSigDescs, std::vector<RootParamDesc> targetRootParamDescs[DESCRIPTOR_USAGE_MAX] = nullptr);
	void BuildDescriptors(std::vector<DescriptorJob>& descriptorJobs);
	void BuildConstantBuffers(std::vector<ConstantBufferJob>& constantBufferJobs);
	void BuildResources(std::vector<ResourceJob>& resourceJobs);
	void BuildShaders(std::vector<ShaderDesc> shaderDescs);
	virtual void BuildInputLayout();
	virtual void BuildPSO();

	virtual bool PerformsTransitions();
	virtual void PerformTransitionsIn();
	virtual void PerformTransitionsOut();
	virtual void AddTransitionIn(DX12Resource* res, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);
	virtual void AddTransitionOut(DX12Resource* res, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);

	void initRootParameterFromType(CD3DX12_ROOT_PARAMETER& param, RootParamDesc desc, std::vector<int>& registers, CD3DX12_DESCRIPTOR_RANGE& table);
	std::wstring getCompileTargetFromType(SHADER_TYPE type);
	ROOT_PARAMETER_TYPE getRootParamTypeFromRangeType(D3D12_DESCRIPTOR_RANGE_TYPE range);
	DESCRIPTOR_TYPE getDescriptorTypeFromRootParameterDesc(RootParamDesc desc);
	void resetCommandList();
	void bindDescriptorHeaps();
	virtual void bindDescriptorsToRoot(DESCRIPTOR_USAGE usage = DESCRIPTOR_USAGE_PER_PASS, int usageIndex = 0, std::vector<RootParamDesc> curRootParamDescs[DESCRIPTOR_USAGE_MAX] = nullptr)=0;
	virtual std::vector<std::pair<D3D12_RESOURCE_STATES, DX12Resource*>> getRequiredResourceStates();
	void setResourceStates();
protected:
	int frameIndex = 0;

	PipeLineStageDesc stageDesc;

	std::vector<std::unique_ptr<FrameResource>> frameResourceArray;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> PSO = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<IDxcBlob>> shaders;
	std::unordered_map<SHADER_TYPE, Microsoft::WRL::ComPtr<IDxcBlob>> shadersByType;
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;

	std::vector<RootParamDesc> rootParameterDescs[DESCRIPTOR_USAGE_MAX];
	std::vector<RenderTargetDesc> renderTargetDescs;

	DX12Resource* output = nullptr;

	ResourceManager resourceManager;
	DescriptorManager descriptorManager;
	ConstantBufferManager constantBufferManager;
};

