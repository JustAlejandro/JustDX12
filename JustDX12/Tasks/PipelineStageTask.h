#pragma once
#include "Tasks\Task.h"
#include "PipelineStage\PipelineStage.h"

class PipelineStageTask : public Task {
public:
	virtual void execute()=0;
protected:
	PipelineStageTask(PipelineStage* stage) { this->stage = stage; }
	PipelineStage* stage;
};

class PipelineStageTaskSetup : public PipelineStageTask {
public:
	PipelineStageTaskSetup(PipelineStage* stage, PipeLineStageDesc desc) : PipelineStageTask(stage) {
		this->desc = desc;
	}
	void execute() override { stage->setup(desc); }
protected:
	PipeLineStageDesc desc;
};

class PipelineStageTaskFence : public PipelineStageTask {
public:
	PipelineStageTaskFence(PipelineStage* stage, int val) : PipelineStageTask(stage) {
		this->val = val;
	}
	void execute() override { stage->setFence(val); };
protected:
	int val;
};

class PipelineStageTaskWaitFence : public PipelineStageTask {
public:
	PipelineStageTaskWaitFence(PipelineStage* stage, int val, Microsoft::WRL::ComPtr<ID3D12Fence> fence) : PipelineStageTask(stage) {
		this->val = val;
		this->fence = fence;
	}
	void execute() override { WaitOnFenceForever(fence, val); }
protected:
	int val;
	Microsoft::WRL::ComPtr<ID3D12Fence> fence;
};

class PipelineStageTaskRun : public PipelineStageTask {
public:
	PipelineStageTaskRun(PipelineStage* stage) : PipelineStageTask(stage) {}
	void execute() override { stage->Execute(); }
};

class PipelineStageTaskUpdateConstantBuffer : public PipelineStageTask {
public:
	PipelineStageTaskUpdateConstantBuffer(PipelineStage* stage, std::string name) : PipelineStageTask(stage) {
		this->name = name;
	}
	void execute() override { stage->updateConstantBuffer(name); }
private:
	std::string name;
};