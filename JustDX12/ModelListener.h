#pragma once
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

class BasicModel;

// Interface for objects that want to know when a new BasicModel is fully loaded.
class ModelListener {
protected:
	ModelListener();

public:
	void broadcastNewModel(std::weak_ptr<BasicModel> model);
	void initialEnroll(std::vector<std::weak_ptr<BasicModel>> models);

protected:
	// Only returns true if there are models to process.
	bool processNewModels();

	virtual void processModel(std::weak_ptr<BasicModel> model) = 0;
private:
	std::mutex queueMutex;
	std::queue<std::weak_ptr<BasicModel>> processQueue;
};