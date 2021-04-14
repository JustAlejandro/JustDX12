#pragma once
#include "Tasks\Task.h"
#include <queue>
#include <thread>
#include <condition_variable>
#define NOMINMAX

// Base class that represents a CPU thread that runs through a list of enqueued commands
// An implementation similar to the Command pattern (though a little different)
class TaskQueueThread {
public:
	TaskQueueThread();
	~TaskQueueThread();

	void enqueue(Task* t);

	HANDLE deferSetCpuEvent();

private:
	bool running;
	std::mutex taskQueueMutex;
	std::queue<Task*> taskQueue;
	std::thread worker;
	std::condition_variable taskCv;

	void threadMain();
};
