#pragma once
#define NOMINMAX
#include <windows.h>
#include <string>

// Describes a generic task that a TaskQueueThread can execute
// Reflective generally of the Command design pattern (mostly)
class Task {
public:
	virtual void execute() = 0;
	virtual ~Task() = default;
protected:
	Task() =default;
};

class SetCpuEventTask : public Task {
public:
	SetCpuEventTask(HANDLE ev) : Task() {
		this->ev = ev;
	}
	void execute() override {
		if (!SetEvent(ev)) {
			throw "SetEvent failed: " + std::to_string(GetLastError());
		}
	}
	~SetCpuEventTask() override = default;
private:
	HANDLE ev;
};