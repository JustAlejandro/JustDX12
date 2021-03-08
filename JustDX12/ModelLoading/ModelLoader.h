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

class RtRenderPipelineStage;

class ModelLoader: public TaskQueueThread {
public:
	static ModelLoader& getInstance();
	static bool allModelsLoaded();
	static void DestroyAll();
	static std::vector<Light> getAllLights(UINT& numPoint, UINT& numDir, UINT& numSpot);
	static std::vector<std::shared_ptr<Model>> getAllRTModels();
	static std::weak_ptr<Model> loadModel(std::string name, std::string dir, bool usesRT);
	static MeshletModel* loadMeshletModel(std::string name, std::string dir, bool usesRT);
	static void registerRtUser(RtRenderPipelineStage* user);
	static bool isModelCountChanged();
	static void unloadModel(std::string name, std::string dir);
	static void updateTransforms();
	static HANDLE buildRTAccelerationStructureDeferred(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList, std::vector<AccelerationStructureBuffers>& scratchBuffers);
	void buildRTAccelerationStructure(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList, std::vector<AccelerationStructureBuffers>& scratchBuffers);
	static HANDLE updateRTAccelerationStructureDeferred(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList);
	void updateRTAccelerationStructure(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList);

	Microsoft::WRL::ComPtr<ID3D12Resource> TLAS = nullptr;
	bool instanceCountChanged = false;
private:
	ModelLoader(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice);
	ModelLoader(ModelLoader const&) = delete;
	void operator=(ModelLoader const&) = delete;

	int frame = 0;
	AccelerationStructureBuffers createBLAS(Model* model, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList);
	void createTLAS(Microsoft::WRL::ComPtr<ID3D12Resource>& tlas, UINT64& tlasSize, std::vector<std::shared_ptr<Model>>& models, std::vector<MeshletModel*>& meshletModels, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList);

	// Since we're storing the models in this class, we need to synchronize access.
	std::mutex databaseLock;
	bool modelCountChanged = false;
	// string is dir + name
	std::vector<std::pair<std::string, std::shared_ptr<Model>>> loadingModels;
	std::unordered_map<std::string, std::shared_ptr<Model>> loadedModels;
	std::unordered_map<std::string, MeshletModel> loadedMeshlets;

	std::vector<RtRenderPipelineStage*> rtUsers;

	UINT64 tlasSize;
	std::unordered_map<Model*, Microsoft::WRL::ComPtr<ID3D12Resource>> BLAS;
	// Since the instanceDesc of each frame is kept seperate, have to store them each frame so they stay in memory.
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, CPU_FRAME_COUNT> instanceScratch;
	AccelerationStructureBuffers tlasScratch;
};

