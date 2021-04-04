#pragma once
#include <string>
#include <vector>
#include <DirectXMath.h>

struct SceneCsvEntry {
	std::string modelName;
	std::string fileName;
	std::string filePath;
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 rot;
	DirectX::XMFLOAT3 scale;

	std::string toString(bool spaced = false) const {
		return modelName + "," +
			fileName + "," +
			filePath + "," +
			float3ToString(pos, spaced) + "," +
			float3ToString(rot, spaced) + "," +
			float3ToString(scale, spaced);
	}

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

	void updateEntry(int index, DirectX::XMFLOAT3 pos, DirectX::XMFLOAT3 rot, DirectX::XMFLOAT3 scale);

	void saveSceneToDisk();
private:
	void processEntry(const std::vector<std::string>& entry);

	std::vector<SceneCsvEntry> items;
	std::string fileName;
	std::string dir;
	// Only used to make sure saved CSVs have the save header row as the original.
	std::string header;
};

