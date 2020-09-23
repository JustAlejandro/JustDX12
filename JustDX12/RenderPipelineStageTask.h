#pragma once
#include "Tasks/Task.h"
#include "PipelineStage\RenderPipelineStage.h"

class RenderPipelineStageTask : Task {
public:
	virtual void execute() = 0;
protected:
	RenderPipelineStageTask(RenderPipelineStage* stage) { this->stage = stage; }
	RenderPipelineStage* stage;
};

class RenderPipelineStageLoadModel : RenderPipelineStageTask {
public:
	void execute() override {
		stage->LoadModel(loader, name, dir);
	}
	RenderPipelineStageLoadModel(RenderPipelineStage* stage, ModelLoader* loader, std::string name, std::string dir)
	: RenderPipelineStageTask(stage) {
		this->name = name;
		this->dir = dir;
		this->loader = loader;
	}

protected:
	std::string name;
	std::string dir;
	ModelLoader* loader;
};