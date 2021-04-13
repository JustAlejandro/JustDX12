#include "TaskQueueThread.h"

TaskQueueThread::TaskQueueThread() {
	running = true;
	worker = std::thread(&TaskQueueThread::threadMain, this);
}

TaskQueueThread::~TaskQueueThread() {
	running = false;
	taskCv.notify_one();
	worker.join();
	while (!taskQueue.empty()) {
		delete taskQueue.front();
		taskQueue.pop();
	}
}

void TaskQueueThread::enqueue(Task* t) {
	std::lock_guard<std::mutex> lk(taskQueueMutex);
	taskQueue.push(t);
	taskCv.notify_one();
}

HANDLE TaskQueueThread::deferSetCpuEvent() {
	HANDLE ev = CreateEvent(
		NULL,
		FALSE,
		FALSE,
		NULL);
	enqueue(new SetCpuEventTask(ev));
	return ev;
}

void TaskQueueThread::threadMain() {
	try {
		while (true) {
			std::unique_lock<std::mutex> lk(taskQueueMutex);
			if (taskQueue.empty()) {
				taskCv.wait(lk, [this]() { return !taskQueue.empty() || !running; });
			}
			if (!running) {
				return;
			}
			Task* toExecute = taskQueue.front();
			taskQueue.pop();
			lk.unlock();

			toExecute->execute();
			delete toExecute;
		}
	}
	catch (const std::string& ex) {
		MessageBoxA(nullptr, ex.c_str(), "String Exception", MB_OK);
		throw ex;
	}
}
