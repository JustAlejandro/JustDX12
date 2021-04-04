#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

class CsvParser {
public:
	CsvParser(std::string fileName, std::string dir) {
		std::ifstream file(dir + "\\" + fileName);
		if ((file.rdstate() & std::ifstream::failbit) != 0) {
			throw "Error Reading in File " + fileName + " at Dir: " + dir;
		}

		std::string line;

		std::getline(file, line);
		header = line;

		while (std::getline(file, line)) {
			std::vector<std::string> entry;
			std::string entrySubData;

			std::istringstream lineParse(line);
			while (std::getline(lineParse, entrySubData, ',')) {
				entry.push_back(entrySubData);
			}
			entries.push_back(entry);
		}
	}

	std::vector<std::vector<std::string>> entries;
	std::string header;
};