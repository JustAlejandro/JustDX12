#pragma once
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

class Model;

// Interface for objects that want to know when a new Model is fully loaded.
class ModelListener {
protected:
	ModelListener();

public:
	void broadcastNewModel(std::weak_ptr<Model> model);
	void initialEnroll(std::vector<std::weak_ptr<Model>> models);

protected:
	// Only returns true if there are models to process.
	bool processNewModels();

	virtual void processModel(std::weak_ptr<Model> model) = 0;
private:
	std::mutex queueMutex;
	std::queue<std::weak_ptr<Model>> processQueue;
};