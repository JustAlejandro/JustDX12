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
	SHADER_TYPE_MS
};

struct RootParamDesc {
	std::string name;
	ROOT_PARAMETER_TYPE type;
	int slot = 0;
	D3D12_DESCRIPTOR_RANGE_TYPE rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	int numConstants = 1;
	DESCRIPTOR_USAGE usagePattern = DESCRIPTOR_USAGE_ALL;
};

struct RenderTargetDesc {
	std::string descriptorName;
	int slot;
};

struct ShaderDesc {
	std::string fileName;
	std::string shaderName;
	std::string methodName;
	SHADER_TYPE type;
	std::vector<DXDefine> defines;
};

struct PipeLineStageDesc {
	std::vector<RootParamDesc> rootSigDesc;
	std::vector<DescriptorJob> descriptorJobs;
	std::vector<ResourceJob> resourceJobs;
	std::vector<ConstantBufferJob> constantBufferJobs;
	std::vector<ShaderDesc> shaderFiles;
	std::vector<std::pair<std::string, DX12Resource*>> externalResources;
	std::vector<std::pair<std::string, std::string>> textureFiles;
	std::vector<RenderTargetDesc> renderTargets;
};

class PipelineStage : public TaskQueueThread {
public:
	void deferSetup(PipeLineStageDesc stageDesc);
	int deferExecute();
	void deferUpdateConstantBuffer(std::string name, ConstantBufferData& data);
	void updateConstantBuffer(std::string name);
	int triggerFence();
	void deferWaitOnFence(Microsoft::WRL::ComPtr<ID3D12Fence> fence, int val);
	DX12Resource* getResource(std::string name);

	virtual void setup(PipeLineStageDesc stageDesc);
	virtual void Execute() = 0;

protected:
	PipelineStage(Microsoft::WRL::ComPtr<ID3D12Device2> d3dDevice);
	void LoadTextures(std::vector<std::pair<std::string, std::string>> textureFiles);
	void BuildRootSignature(Microsoft::WRL::ComPtr<ID3D12RootSignature>& rootSig, std::vector<RootParamDesc> rootSigDescs, std::vector<RootParamDesc> targetRootParamDescs[DESCRIPTOR_USAGE_MAX] = nullptr);
	void BuildDescriptors(std::vector<DescriptorJob>& descriptorJobs);
	void BuildConstantBuffers(std::vector<ConstantBufferJob>& constantBufferJobs);
	void BuildResources(std::vector<ResourceJob>& resourceJobs);
	void BuildShaders(std::vector<ShaderDesc> shaderDescs);
	virtual void BuildInputLayout();
	virtual void BuildPSO();

	void initRootParameterFromType(CD3DX12_ROOT_PARAMETER& param, RootParamDesc desc, std::vector<int>& registers, CD3DX12_DESCRIPTOR_RANGE& table);
	std::wstring getCompileTargetFromType(SHADER_TYPE type);
	ROOT_PARAMETER_TYPE getRootParamTypeFromRangeType(D3D12_DESCRIPTOR_RANGE_TYPE range);
	DESCRIPTOR_TYPE getDescriptorTypeFromRootParameterDesc(RootParamDesc desc);
	void resetCommandList();
	void bindDescriptorHeaps();
	virtual void bindDescriptorsToRoot(DESCRIPTOR_USAGE usage = DESCRIPTOR_USAGE_PER_PASS, int usageIndex = 0, std::vector<RootParamDesc> curRootParamDescs[DESCRIPTOR_USAGE_MAX] = nullptr)=0;
	void setResourceStates();
protected:
	PipeLineStageDesc stageDesc;

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

