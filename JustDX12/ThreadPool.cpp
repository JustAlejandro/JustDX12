#include "ThreadPool.h"
#include "Tasks/Task.h"

ThreadPool::ThreadPool() : threadSize(std::thread::hardware_concurrency()), threadIdx(0) {
	threads.reserve(std::thread::hardware_concurrency());
	for (size_t i = 0; i < std::thread::hardware_concurrency(); i++) {
		threads.emplace_back(std::make_unique<TaskQueueThread>());
	}
}

void ThreadPool::enqueue(Task* task) {
	ThreadPool& instance = ThreadPool::getInstance();
	int value = instance.threadIdx.fetch_add(1);
	instance.threads[value % instance.threadSize]->enqueue(task);
}

ThreadPool& ThreadPool::getInstance() {
	static ThreadPool instance;
	return instance;
}
