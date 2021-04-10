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

void SceneCsv::updateEntry(int index, std::vector<InstanceData> instances) {
	items[index].instances = instances;
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
	newEntry.instances.reserve((entry.size() - 3) / 9);
	for (size_t i = 3; i < entry.size();) {
		InstanceData newInstance;
		newInstance.pos.x = std::atof(entry[i++].c_str());
		newInstance.pos.y = std::atof(entry[i++].c_str());
		newInstance.pos.z = std::atof(entry[i++].c_str());
		newInstance.rot.x = std::atof(entry[i++].c_str());
		newInstance.rot.y = std::atof(entry[i++].c_str());
		newInstance.rot.z = std::atof(entry[i++].c_str());
		newInstance.scale.x = std::atof(entry[i++].c_str());
		newInstance.scale.y = std::atof(entry[i++].c_str());
		newInstance.scale.z = std::atof(entry[i++].c_str());
		newEntry.instances.push_back(newInstance);
	}	
	items.push_back(newEntry);
}
