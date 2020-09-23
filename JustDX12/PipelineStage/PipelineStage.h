#pragma once
#include <wrl.h>
#include <d3d12.h>
#include "Tasks\TaskQueueThread.h"
#include "DescriptorClasses\DescriptorManager.h"
#include "ResourceClasses\ResourceManager.h"
#include <vector>

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
	SHADER_TYPE_CS
};

struct RootParamDesc {
	std::string name;
	ROOT_PARAMETER_TYPE type;
	int numConstants = 1;
	D3D12_DESCRIPTOR_RANGE_TYPE rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
};

struct SamplerDesc {
	int empty = 0;
};

struct ShaderDesc {
	std::string fileName;
	std::string shaderName;
	std::string methodName;
	SHADER_TYPE type;
	D3D_SHADER_MACRO* defines;
};

struct PipeLineStageDesc {
	std::vector<RootParamDesc> rootSigDesc;
	std::vector<SamplerDesc> samplerDesc;
	std::vector<std::vector<DescriptorJob>> descriptorJobs;
	std::vector<ResourceJob> resourceJobs;
	std::vector<ShaderDesc> shaderFiles;
	std::vector<std::pair<std::string, DX12Resource*>> externalResources;
};

class PipelineStage : public TaskQueueThread {
public:
	void deferSetup(PipeLineStageDesc stageDesc);
	void deferExecute();
	int triggerFence();
	void waitOnFence(Microsoft::WRL::ComPtr<ID3D12Fence> fence, int val);

	void setup(PipeLineStageDesc stageDesc);
	virtual void Execute() = 0;

protected:
	PipelineStage(Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice);
	void BuildRootSignature(PipeLineStageDesc stageDesc);
	void BuildDescriptors(std::vector<std::vector<DescriptorJob>>& descriptorJobs);
	void BuildResources(std::vector<ResourceJob>& resourceJobs);
	void BuildShaders(std::vector<ShaderDesc> shaderDescs);
	void BuildInputLayout();
	virtual void BuildPSO();

	void initRootParameterFromType(CD3DX12_ROOT_PARAMETER& param, RootParamDesc desc, std::vector<int>& registers, CD3DX12_DESCRIPTOR_RANGE& table);
	std::string getCompileTargetFromType(SHADER_TYPE type);
	DESCRIPTOR_TYPE getDescriptorTypeFromRootParameterType(ROOT_PARAMETER_TYPE type);
	void resetCommandList();
	void bindDescriptorHeaps();
	virtual void bindDescriptorsToRoot()=0;
	void setResourceStates();
protected:

	Microsoft::WRL::ComPtr<ID3D12PipelineState> PSO = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> shaders;
	std::unordered_map<SHADER_TYPE, Microsoft::WRL::ComPtr<ID3DBlob>> shadersByType;
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;

	std::vector<RootParamDesc> rootParameterDescs;

	DX12Resource* output = nullptr;
	ResourceManager resourceManager;
	DescriptorManager descriptorManager;
};

