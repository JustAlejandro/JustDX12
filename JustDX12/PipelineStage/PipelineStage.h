#pragma once
#define NOMINMAX
#include <vector>
#include <wrl.h>
#include <d3d12.h>
#include <dxcapi.h>

#include "FrameResource.h"

#include "Tasks\TaskQueueThread.h"
#include "DescriptorClasses\DescriptorManager.h"
#include "ResourceClasses\ResourceManager.h"
#include "ConstantBufferManager.h"

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

// Describes a root parameter to automate resource/descriptor binding
struct RootParamDesc {
	RootParamDesc() = default;
	RootParamDesc(std::string name, ROOT_PARAMETER_TYPE type, int slot = 0, D3D12_DESCRIPTOR_RANGE_TYPE rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV, int numConstants = 1, DESCRIPTOR_USAGE usagePattern = DESCRIPTOR_USAGE_ALL, UINT space = 0) {
		this->name = name;
		this->type = type;
		this->slot = slot;
		this->rangeType = rangeType;
		this->numConstants = numConstants;
		this->usagePattern = usagePattern;
		this->space = space;
	}

	// Name of the descriptor/resource that this descriptor will bind to
	std::string name;
	ROOT_PARAMETER_TYPE type;
	int slot = 0;
	// If the RootParam is a descriptor table, what range type is it? (Only single type allowed)
	D3D12_DESCRIPTOR_RANGE_TYPE rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	// Set to -1 for unbounded, just ensure you use a different space if you have multiple HLSL resources in the same shader register type
	int numConstants = 1;
	// How often should this Root Paramter be rebound?
	DESCRIPTOR_USAGE usagePattern = DESCRIPTOR_USAGE_ALL;
	// Register space that this resource occupies
	UINT space = 0;
};

// Tells the render passes done by a RenderPipelineStage where to find RTV descriptors
// TODO: refactor this out of the PipelineStage and into RenderPipelineStage
struct RenderTargetDesc {
	// The RTV descriptorName that maps to the resource that should be the render target
	std::string descriptorName;
	int slot;
	RenderTargetDesc() = default;
	RenderTargetDesc(std::string descriptorName, int slot) {
		this->descriptorName = descriptorName;
		this->slot = slot;
	}
};

// Describes steps needed for shader compilation
// External includes will try to be found automatically
struct ShaderDesc {
	ShaderDesc() = default;
	ShaderDesc(std::string fileName, std::string shaderName, std::string methodName, SHADER_TYPE type, std::vector<DXDefine> defines) {
		this->fileName = fileName;
		this->shaderName = shaderName;
		this->methodName = methodName;
		this->type = type;
		this->defines = defines;
	}

	std::string fileName;
	// User friendly name, useful for debugging, will be used in usage index->shader binding in future
	std::string shaderName;
	// Method within shader that should be compiled
	std::string methodName;
	SHADER_TYPE type;
	// Vector of any '#define' lines that should be added to the shader: ex: { "RT_PATH", "ON" }
	std::vector<DXDefine> defines;
};

struct PipeLineStageDesc {
	// Useful for debugging/thread naming, no other purpose
	std::string name;
	// Vector of RootParamDescs that should map fairly directly to the Root Signature expected by your shaders
	std::vector<RootParamDesc> rootSigDesc;
	// All descriptors needed by the root signature to successfully bind all resources
	std::vector<DescriptorJob> descriptorJobs;
	// Resources to create that are pointed at by Descriptors (specifically at the moment, textures)
	std::vector<ResourceJob> resourceJobs;
	// A subtype of a Resource, used specifically for shader ConstantBuffer data.
	std::vector<ConstantBufferJob> constantBufferJobs;
	std::vector<ShaderDesc> shaderFiles;
	// ConstantBuffers from other PipelineStages that this stage would like to see the contents of
	std::vector<std::pair<IndexedName, DX12ConstantBuffer*>> externalConstantBuffers;
	// Similar to externalConstantBuffers, typical use case: want to read from a resource that was a render target in a previous stage
	std::vector<std::pair<std::string, DX12Resource*>> externalResources;
	// Rather than creating blank resources with a resource job, this creates a resource from a texture file. 
	// Format: first string is the name of the resource to use when binding a descriptor to it, second string is the texture path relative to the Model directory 
	std::vector<std::pair<std::string, std::string>> textureFiles;
};

// Base class that represents a set of actions to be performed on the GPU (render/compute subclasses atm)
// Almost all actions performed on this object are in a 'deferred' manner, meaning that the user enqueues
// commands and a thread that belongs to this object will process all commands (see TaskQueueThread for more)
// TODO: possibly use a threadpool for this purpose, so we don't have a thread for each PipelineStage
class PipelineStage : public TaskQueueThread {
protected:
	PipelineStage(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice, D3D12_COMMAND_LIST_TYPE cmdListType = D3D12_COMMAND_LIST_TYPE_DIRECT);

public:
	// Once all the PipelineStages that will be a part of this application are setup, this method will resolve all the state transitions
	// that have to occur between shared resources. This method will throw an error if the transition required wouldn't be possible
	// ex: Resource 1 goes from state A->B from Stage X to Stage Y, where X and Y cannot perform transitions
	// Format expected is a vector of vectors, where each vector represents work that can be performed together (compute and render) without any shared dependency
	// ex: { { Raster }, { Defer Shading, SSAO }, { Output Merge } }
	static std::vector<CD3DX12_RESOURCE_BARRIER> setupResourceTransitions(std::vector<std::vector<PipelineStage*>> stages);

	// Enqueues a 'setup' action, so initialization can be multithreaded
	// Issue is that since most PipelineStages rely on resources/constant buffers created by another stage, this is usually still done in a fairly single-threaded manner
	// so for now best to just call 'setup' directly.
	void deferSetup(PipeLineStageDesc stageDesc);
	virtual void setup(PipeLineStageDesc stageDesc);

	// Builds a commands into 'mCommandList' by calling the 'execute' method on the worker thread and triggers the event from the returned HANDLE when completed
	// Issued commands are based on the type of PipelineStage subclass
	HANDLE deferExecute();
	virtual void execute() = 0;

	// Methods used to retrieve resources, typically used to import resources between PipelineStages
	DX12ConstantBuffer* getConstantBuffer(IndexedName indexName);
	DX12Resource* getResource(std::string name);

	// Update the constant buffer associated with the name and index supplied, but only occurs on the worker thread, to ensure that this command isn't executed while an 'execute' is in flight
	void deferUpdateConstantBuffer(std::string name, ConstantBufferData& data, int usageIndex = 0);
	void updateConstantBuffer(IndexedName indexName);

	// Enqueues a CPU action to trigger the fence owned by this PipelineStage. Used for CPU action synchronization.
	// (Should be avoided when possible and replaced with CPU side Windows event HANDLEs)
	int triggerFence();
	// Allows forcing the worker thread to wait on a fence before continuing execution
	void deferWaitOnFence(Microsoft::WRL::ComPtr<ID3D12Fence> fence, int val);

protected:
	void buildRootSignature(Microsoft::WRL::ComPtr<ID3D12RootSignature>& rootSig, std::vector<RootParamDesc> rootSigDescs, std::vector<RootParamDesc> targetRootParamDescs[DESCRIPTOR_USAGE_MAX] = nullptr);
	void buildDescriptors(std::vector<DescriptorJob>& descriptorJobs);
	void buildConstantBuffers(std::vector<ConstantBufferJob>& constantBufferJobs);
	void buildResources(std::vector<ResourceJob>& resourceJobs);
	void buildShaders(std::vector<ShaderDesc> shaderDescs);
	virtual void buildInputLayout();
	virtual void buildPSO();
	// first string is the name this texture will have as a DX12Resource, second string is the file path relative to the Models directory.
	void loadTextures(std::vector<std::pair<std::string, std::string>> textureFiles);

	void importResource(std::string name, DX12Resource* resource);
	void importResource(std::string name, ID3D12Resource* resource);

	void resetCommandList();
	void bindDescriptorHeaps();
	virtual void bindDescriptorsToRoot(DESCRIPTOR_USAGE usage = DESCRIPTOR_USAGE_PER_PASS, int usageIndex = 0, std::vector<RootParamDesc> curRootParamDescs[DESCRIPTOR_USAGE_MAX] = nullptr) = 0;
	void setResourceStates();

	virtual bool performsTransitions();
	virtual void performTransitionsIn();
	virtual void performTransitionsOut();
	virtual void addTransitionIn(DX12Resource* res, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);
	virtual void addTransitionOut(DX12Resource* res, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);

	void initRootParameterFromType(CD3DX12_ROOT_PARAMETER& param, RootParamDesc desc, std::vector<std::vector<int>>& registers, CD3DX12_DESCRIPTOR_RANGE& table);
	std::wstring getCompileTargetFromType(SHADER_TYPE type);
	ROOT_PARAMETER_TYPE getRootParamTypeFromRangeType(D3D12_DESCRIPTOR_RANGE_TYPE range);
	DESCRIPTOR_TYPE getDescriptorTypeFromRootParameterDesc(RootParamDesc desc);
	virtual std::vector<std::pair<D3D12_RESOURCE_STATES, DX12Resource*>> getRequiredResourceStates();

	PipeLineStageDesc stageDesc;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> PSO = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<IDxcBlob>> shaders;
	std::unordered_map<SHADER_TYPE, Microsoft::WRL::ComPtr<IDxcBlob>> shadersByType;
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;

	std::vector<RootParamDesc> rootParameterDescs[DESCRIPTOR_USAGE_MAX];

	ResourceManager resourceManager;
	DescriptorManager descriptorManager;
	ConstantBufferManager constantBufferManager;
};

