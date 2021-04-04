#include "SceneCsv.h"
#include "CsvParser.h"

SceneCsv::SceneCsv(std::string fileName, std::string dir) {
	this->fileName = fileName;
	this->dir = dir;
	CsvParser parsedFile(fileName, dir);
	this->header = parsedFile.header;
	for (const auto& entry : parsedFile.entries) {
		processEntry(entry);
	}
}

const std::string& SceneCsv::getFileName() const {
	return fileName;
}

const std::vector<SceneCsvEntry>& SceneCsv::getItems() {
	return items;
}

void SceneCsv::updateEntry(int index, DirectX::XMFLOAT3 pos, DirectX::XMFLOAT3 rot, DirectX::XMFLOAT3 scale) {
	items[index].pos = pos;
	items[index].rot = rot;
	items[index].scale = scale;
}

void SceneCsv::saveSceneToDisk() {
	std::string savedName = fileName;
	while (std::ifstream(dir + "\\" + savedName).good()) {
		savedName.insert(savedName.find_last_of('.'), "Copy");
	}
	std::ofstream fileOut(dir + "\\" + savedName);
	fileOut << header << "\n";
	for (const auto& item : items) {
		fileOut << item.toString() << "\n";
	}
}

void SceneCsv::processEntry(const std::vector<std::string>& entry) {
	SceneCsvEntry newEntry;
	newEntry.modelName = entry[0];
	newEntry.fileName = entry[1];
	newEntry.filePath = entry[2];
	newEntry.pos.x = std::atof(entry[3].c_str());
	newEntry.pos.y = std::atof(entry[4].c_str());
	newEntry.pos.z = std::atof(entry[5].c_str());
	newEntry.rot.x = std::atof(entry[6].c_str());
	newEntry.rot.y = std::atof(entry[7].c_str());
	newEntry.rot.z = std::atof(entry[8].c_str());
	newEntry.scale.x = std::atof(entry[9].c_str());
	newEntry.scale.y = std::atof(entry[10].c_str());
	newEntry.scale.z = std::atof(entry[11].c_str());
	
	items.push_back(newEntry);
}
