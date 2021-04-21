#include "ModelListener.h"

#include "ModelLoading/ModelLoader.h"

ModelListener::ModelListener() {
	ModelLoader::registerModelListener(this);
}

void ModelListener::broadcastNewModel(std::weak_ptr<BasicModel> model) {
	std::lock_guard<std::mutex> lk(queueMutex);
	processQueue.push(model);
}

void ModelListener::initialEnroll(std::vector<std::weak_ptr<BasicModel>> models) {
	std::lock_guard<std::mutex> lk(queueMutex);
	for (const auto& model : models) {
		processQueue.push(model);
	}
}

bool ModelListener::processNewModels() {
	std::unique_lock<std::mutex> lk(queueMutex, std::defer_lock);
	bool modelsProcessed = !processQueue.empty();
	while (!processQueue.empty()) {
		lk.lock();
		std::weak_ptr<BasicModel> model = processQueue.front();
		processQueue.pop();
		lk.unlock();
		processModel(model);
	}
	return modelsProcessed;
}
