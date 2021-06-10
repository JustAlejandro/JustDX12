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
	
	// Empties all the threads in the ThreadPool's work queues and then
	// Returns a single handle to wait for showing all TaskThreads finished their work.
	static std::vector<HANDLE> prepareQuit();

private:
	static ThreadPool& getInstance();

	const unsigned int threadSize;
	std::atomic_uint64_t threadIdx;
	// Size set at runtime, so can't use Array.
	// Only want it set once, so some const wrapper would be better here.
	std::vector<std::unique_ptr<TaskQueueThread>> threads;
};