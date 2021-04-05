#pragma once
#include <assimp/Importer.hpp>		// C++ importer interface
#include <assimp/scene.h>			// Output data structure
#include <assimp/postprocess.h>		// Post processing flags

#include "Tasks\Task.h"
#include "ModelLoading\Model.h"
#include "MeshletModel.h"

#include "Tasks\TaskQueueThread.h"
#include "ModelLoading/ModelLoader.h"


class ModelLoader;

// TODO: Refactor this into being private in ModelLoader, no reason it should be publicly accessible
class ModelLoadTask : public Task {
public:
	ModelLoadTask(TaskQueueThread* taskQueueThread, Model* model) {
		this->model = model;
		this->taskQueueThread = taskQueueThread;
	}

	virtual ~ModelLoadTask() override = default;

	void execute() override {
		// TODO: possibly have multiple allocators for model loading.
		taskQueueThread->waitOnFence();
		taskQueueThread->mDirectCmdListAlloc->Reset();
		taskQueueThread->mCommandList->Reset(taskQueueThread->mDirectCmdListAlloc.Get(), nullptr);
		taskQueueThread->mDirectCmdListAlloc->SetName(L"ModelLoad");

		Assimp::Importer importer;
		OutputDebugStringA(("Starting to Load Model: " + model->name + "\n").c_str());
		const aiScene* scene = importer.ReadFile(model->dir + "\\" + model->name,
			aiProcess_GenUVCoords | aiProcess_Triangulate | aiProcess_ConvertToLeftHanded | 
			aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace | aiProcess_FindInstances |
			aiProcess_SplitLargeMeshes);
		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
			std::string error = importer.GetErrorString();
			OutputDebugStringA(("ERROR::ASSIMP::" + error).c_str());
		}
		else {
			OutputDebugStringA(("Finished load, beginning processing/upload: " + model->name + "\n").c_str());
			model->setup(taskQueueThread, scene->mRootNode, scene);
		}
	}

private:
	Model* model;
	TaskQueueThread* taskQueueThread;
};

class MeshletModelLoadTask : public Task {
public:
	MeshletModelLoadTask(TaskQueueThread* taskQueueThread, MeshletModel* model) {
		this->model = model;
		this->taskQueueThread = taskQueueThread;
	}

	virtual ~MeshletModelLoadTask() override = default;

	void execute() override {
		taskQueueThread->mDirectCmdListAlloc->Reset();
		taskQueueThread->mCommandList->Reset(taskQueueThread->mDirectCmdListAlloc.Get(), nullptr);

		OutputDebugStringA(("Starting to Load Meshlet Model: " + model->name + "\n").c_str());

		model->LoadFromFile(model->dir + "\\" + model->name);

		OutputDebugStringA(("Finished load, beginning processing/upload: " + model->name + "\n").c_str());

		model->UploadGpuResources(taskQueueThread->md3dDevice.Get(), taskQueueThread->mCommandQueue.Get(), taskQueueThread->mDirectCmdListAlloc.Get(), taskQueueThread->mCommandList.Get());

		OutputDebugStringA(("Finished Upload Meshlet Model: " + model->name + "\n").c_str());
	}

private:
	MeshletModel* model;
	TaskQueueThread* taskQueueThread;
};


// TODO: Refactor this into being private in RTPipelineStage, no reason it should be publicly accessible
class RTStructureLoadTask : public Task {
public:
	RTStructureLoadTask(ModelLoader* modelLoader, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList, std::vector<AccelerationStructureBuffers>& scratchBuffers) : scratchBuffers(scratchBuffers) {
		this->modelLoader = modelLoader;
		this->cmdList = cmdList;
	}

	virtual ~RTStructureLoadTask() override = default;

	void execute() override {
		modelLoader->buildRTAccelerationStructure(cmdList, scratchBuffers);
	}

private:
	ModelLoader* modelLoader;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList;
	std::vector<AccelerationStructureBuffers>& scratchBuffers;
};


class RTStructureUpdateTask : public Task {
public:
	RTStructureUpdateTask(ModelLoader* modelLoader, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList) {
		this->modelLoader = modelLoader;
		this->cmdList = cmdList;
	}

	virtual ~RTStructureUpdateTask() override = default;

	void execute() override {
		modelLoader->updateRTAccelerationStructure(cmdList);
	}

private:
	ModelLoader* modelLoader;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList;
};

