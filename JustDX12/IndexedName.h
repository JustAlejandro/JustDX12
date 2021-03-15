#pragma once
#include <string>

// Simple representation of a pair of an index and a string name
// IndexedName is used in almost all maps as a key and is immutable
class IndexedName {
public:
	IndexedName(std::string name, int index) {
		this->index = index;
		this->name = name;
	}

	int getIndex() const {
		return index;
	}

	std::string getName() const {
		return name;
	}

	bool operator==(const IndexedName& other) const {
		if (name == other.name) {
			return index == other.index;
		}
		else {
			return false;
		}
	}

private:
	int index;
	std::string name;
};

namespace std {
	template<>
	struct hash<IndexedName> {
		std::size_t operator()(const IndexedName& key) const {
			return ((std::hash<std::string>()(key.getName())
				^ (std::hash<int>()(key.getIndex()))));
		}
	};
}