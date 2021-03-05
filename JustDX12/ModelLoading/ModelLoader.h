#pragma once
#include <assimp/Importer.hpp>		// C++ importer interface
#include <assimp/scene.h>			// Output data structure
#include <assimp/postprocess.h>		// Post processing flags
#include <thread>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include "Tasks\TaskQueueThread.h"
#include "ModelLoading\Model.h"
#include "MeshletModel.h"
#include "Common.h"

struct AccelerationStructureBuffers {
	Microsoft::WRL::ComPtr<ID3D12Resource> pScratch;
	Microsoft::WRL::ComPtr<ID3D12Resource> pResult;
	Microsoft::WRL::ComPtr<ID3D12Resource> pInstanceDesc;
};

class ModelLoader: public TaskQueueThread {
public:
	ModelLoader(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice);
	bool allModelsLoaded();
	std::vector<Light> getAllLights(UINT& numPoint, UINT& numDir, UINT& numSpot);
	std::weak_ptr<Model> loadModel(std::string name, std::string dir, bool usesRT);
	MeshletModel* loadMeshletModel(std::string name, std::string dir, bool usesRT);
	void unloadModel(std::string name, std::string dir);
	void updateTransforms();
	HANDLE buildRTAccelerationStructureDeferred(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList, std::vector<AccelerationStructureBuffers>& scratchBuffers);
	void buildRTAccelerationStructure(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList, std::vector<AccelerationStructureBuffers>& scratchBuffers);
	HANDLE updateRTAccelerationStructureDeferred(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList);
	void updateRTAccelerationStructure(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList);

	Microsoft::WRL::ComPtr<ID3D12Resource> TLAS = nullptr;
private:
	int frame = 0;
	AccelerationStructureBuffers createBLAS(Model* model, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList);
	void createTLAS(Microsoft::WRL::ComPtr<ID3D12Resource>& tlas, UINT64& tlasSize, std::vector<Model*>& models, std::vector<MeshletModel*>& meshletModels, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList);

	// Since we're storing the models in this class, we need to synchronize access.
	std::mutex databaseLock;
	bool newModelLoaded = false;
	// string is dir + name
	std::vector<std::pair<std::string, std::shared_ptr<Model>>> loadingModels;
	std::unordered_map<std::string, std::shared_ptr<Model>> loadedModels;
	std::unordered_map<std::string, MeshletModel> loadedMeshlets;

	UINT64 tlasSize;
	std::unordered_map<Model*, Microsoft::WRL::ComPtr<ID3D12Resource>> BLAS;
	// Since the instanceDesc of each frame is kept seperate, have to store them each frame so they stay in memory.
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, CPU_FRAME_COUNT> instanceScratch;
	AccelerationStructureBuffers tlasScratch;
};

