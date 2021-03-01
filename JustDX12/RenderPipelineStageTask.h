#pragma once
#include "Tasks/Task.h"
#include "PipelineStage\RenderPipelineStage.h"

class RenderPipelineStageTask : public Task {
public:
	virtual void execute() = 0;
	virtual ~RenderPipelineStageTask() override = default;
protected:
	RenderPipelineStageTask(RenderPipelineStage* stage) { this->stage = stage; }
	RenderPipelineStage* stage;
};