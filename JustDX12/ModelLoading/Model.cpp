#include "Model.h"

Model::Model(ID3D12Device5* device, std::string name, std::string dir, bool usesRT) : TransformData(device){
	this->name = name;
	this->dir = dir;
	this->usesRT = usesRT;
}
