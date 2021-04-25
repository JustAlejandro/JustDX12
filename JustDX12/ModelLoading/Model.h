#pragma once
#include <string>
#include <atomic>

#include "TransformData.h"

class Model : public TransformData {
public:
	Model(ID3D12Device5* device, std::string name, std::string dir, bool usesRT = false);
	std::string name;
	std::string dir;
	bool usesRT;
	std::atomic_bool loaded;
};