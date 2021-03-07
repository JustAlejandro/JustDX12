#include "RtRenderPipelineStage.h"


void RtRenderPipelineStage::DeferRebuildRtData(std::vector<std::shared_ptr<Model>> RtModels) {
	enqueue(new RebuildRtDataTask(this, RtModels));
}

void RtRenderPipelineStage::RebuildRtData(std::vector<std::shared_ptr<Model>> RtModels) {
	if (rtDescriptors.heapEndIndex != 0) {
	}
}

void RtRenderPipelineStage::RebuildRtDataTask::execute() {
	stage->RebuildRtData(RtModels);
}

RtRenderPipelineStage::RebuildRtDataTask::RebuildRtDataTask(RtRenderPipelineStage* stage, std::vector<std::shared_ptr<Model>> RtModels) {
	this->stage = stage;
	this->RtModels = RtModels;
}