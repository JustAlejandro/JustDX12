#pragma once
#include <string>
#include <vector>
#include <DirectXMath.h>

struct InstanceData {
	InstanceData() : pos(0.0f,0.0f,0.0f), rot(0.0f,0.0f,0.0f), scale(1.0f,1.0f,1.0f) {}

	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 rot;
	DirectX::XMFLOAT3 scale;
};

struct SceneCsvEntry {
	std::string toString(bool spaced = false) const {
		std::string modelString = modelName + "," + fileName + "," + filePath;
		for (const auto& instance : instances) {
			modelString += "," + float3ToString(instance.pos, spaced) + "," +
				float3ToString(instance.rot, spaced) + "," +
				float3ToString(instance.scale, spaced);
		}
		return modelString;
	}

	std::string modelName;
	std::string fileName;
	std::string filePath;
	std::vector<InstanceData> instances;

private:
	std::string float3ToString(DirectX::XMFLOAT3 vec, bool spaced) const {
		std::string delim = spaced ? ", " : ",";
		return std::to_string(vec.x) + delim + std::to_string(vec.y) + delim + std::to_string(vec.z);
	}
};

class SceneCsv {
public:
	SceneCsv(std::string fileName, std::string dir);

	const std::string& getFileName() const;
	const std::vector<SceneCsvEntry>& getItems();

	void updateEntry(int index, std::vector<InstanceData> instances);

	void saveSceneToDisk();
private:
	void processEntry(const std::vector<std::string>& entry);

	std::vector<SceneCsvEntry> items;
	std::string fileName;
	std::string dir;
	// Only used to make sure saved CSVs have the save header row as the original.
	std::string header;
};

