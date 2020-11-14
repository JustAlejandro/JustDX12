#pragma once
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags
#include <thread>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include "Tasks\TaskQueueThread.h"
#include "Tasks\ModelLoadTask.h"

class Model;
class MeshletModel;

class ModelLoader: public TaskQueueThread {
public:
	ModelLoader(Microsoft::WRL::ComPtr<ID3D12Device2> d3dDevice);
	Model* loadModel(std::string name, std::string dir);
	MeshletModel* loadMeshletModel(std::string name, std::string dir);
};

