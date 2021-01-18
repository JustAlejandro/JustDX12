#pragma once
#include "Tasks\Task.h"
#include "ModelLoading\Model.h"
#include "Tasks\TaskQueueThread.h"
#include "MeshletModel.h"
#include "ModelLoading/ModelLoader.h"

#include <iostream>
#include <assimp/Importer.hpp>		// C++ importer interface
#include <assimp/scene.h>			// Output data structure
#include <assimp/postprocess.h>		// Post processing flags

class ModelLoader;

class ModelLoadTask : public Task {
public:
	ModelLoadTask(TaskQueueThread* taskQueueThread, Model* model) {
		this->model = model;
		this->taskQueueThread = taskQueueThread;
	}

	void execute() override {
		taskQueueThread->mDirectCmdListAlloc->Reset();
		taskQueueThread->mCommandList->Reset(taskQueueThread->mDirectCmdListAlloc.Get(), nullptr);

		Assimp::Importer importer;
		OutputDebugStringA(("Starting to Load Model: " + model->name + "\n").c_str());
		const aiScene* scene = importer.ReadFile(model->dir + "\\" + model->name,
			aiProcess_GenUVCoords | aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_ImproveCacheLocality | aiProcess_GenSmoothNormals);
		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
			std::string error = importer.GetErrorString();
			OutputDebugStringA(("ERROR::ASSIMP::" + error).c_str());
		}
		else {
			OutputDebugStringA(("Finished load, beginning processing/upload: " + model->name + "\n").c_str());
			model->setup(taskQueueThread, scene->mRootNode, scene);
			model->loaded = true;
			OutputDebugStringA(("Finished Upload Model: " + model->name + "\n").c_str());
		}
	}

	virtual ~ModelLoadTask() override = default;

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

	void execute() override {
		taskQueueThread->mDirectCmdListAlloc->Reset();
		taskQueueThread->mCommandList->Reset(taskQueueThread->mDirectCmdListAlloc.Get(), nullptr);

		OutputDebugStringA(("Starting to Load Meshlet Model: " + model->name + "\n").c_str());

		model->LoadFromFile(model->dir + "\\" + model->name);

		OutputDebugStringA(("Finished load, beginning processing/upload: " + model->name + "\n").c_str());

		model->UploadGpuResources(taskQueueThread->md3dDevice.Get(), taskQueueThread->mCommandQueue.Get(), taskQueueThread->mDirectCmdListAlloc.Get(), taskQueueThread->mCommandList.Get());

		OutputDebugStringA(("Finished Upload Meshlet Model: " + model->name + "\n").c_str());
	}

	virtual ~MeshletModelLoadTask() override = default;

private:
	MeshletModel* model;
	TaskQueueThread* taskQueueThread;
};

class RTStructureLoadTask : public Task {
public:
	RTStructureLoadTask(ModelLoader* modelLoader, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList, std::vector<AccelerationStructureBuffers>& scratchBuffers) : scratchBuffers(scratchBuffers) {
		this->modelLoader = modelLoader;
		this->cmdList = cmdList;
	}

	void execute() override {
		modelLoader->mDirectCmdListAlloc->Reset();
		modelLoader->mCommandList->Reset(modelLoader->mDirectCmdListAlloc.Get(), nullptr);

		modelLoader->buildRTAccelerationStructure(cmdList, scratchBuffers);
	}

	virtual ~RTStructureLoadTask() override = default;

private:
	ModelLoader* modelLoader;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList;
	std::vector<AccelerationStructureBuffers>& scratchBuffers;
};

