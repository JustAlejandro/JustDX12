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
		OutputDebugString(L"Starting to Load Model\n");
		const aiScene* scene2 = importer.ReadFile(model->dir + "\\" + model->name,
			aiProcess_Triangulate | aiProcess_FlipUVs);
		const aiScene* scene = importer.ApplyPostProcessing(aiProcess_CalcTangentSpace);
		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
			std::string error = importer.GetErrorString();
			OutputDebugStringA(("ERROR::ASSIMP::" + error).c_str());
		}
		else {
			OutputDebugString(L"Finished load, beginning processing/upload\n");
			model->setup(taskQueueThread, scene->mRootNode, scene);
			model->loaded = true;
			OutputDebugString(L"Finished Upload Model\n");
		}
	}

	virtual ~ModelLoadTask() override = default;

private:
	Model* model;
	TaskQueueThread* taskQueueThread;
};

