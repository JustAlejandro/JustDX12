#pragma once
#include "Tasks\Task.h"
#include "ModelLoading\Model.h"
#include "Tasks\TaskQueueThread.h"

#include <iostream>
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags

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
		const aiScene* scene2 = importer.ReadFile(model->dir + "\\" + model->name,
			aiProcess_Triangulate | aiProcess_FlipUVs);
		const aiScene* scene = importer.ApplyPostProcessing(aiProcess_CalcTangentSpace);
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

