#pragma once
#include "TaskQueueThread.h"
#include <atomic>

class ThreadPool {
private:
	ThreadPool();
	ThreadPool(ThreadPool const&) = delete;
	void operator=(ThreadPool const&) = delete;

public:
	static void enqueue(Task* task);

private:
	static ThreadPool& getInstance();

	const unsigned int threadSize;
	std::atomic_uint64_t threadIdx;
	// Size set at runtime, so can't use Array.
	// Only want it set once, so some const wrapper would be better here.
	std::vector<std::unique_ptr<TaskQueueThread>> threads;
};