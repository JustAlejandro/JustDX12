#pragma once
// Used STL thread because of familiarity
// TODO: Use Windows specific thread interface to gain priority access
#include <thread>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <assimp/Importer.hpp>		// C++ importer interface
#include <assimp/scene.h>			// Output data structure
#include <assimp/postprocess.h>		// Post processing flags

#include "Common.h"

#include "ModelLoading\Model.h"
#include "MeshletModel.h"

#include "Tasks\DX12TaskQueueThread.h"

// Buffers required to be held until build process completed.
// TODO: move structure to ModelLoader's private
struct AccelerationStructureBuffers {
	Microsoft::WRL::ComPtr<ID3D12Resource> pScratch;
	Microsoft::WRL::ComPtr<ID3D12Resource> pResult;
	Microsoft::WRL::ComPtr<ID3D12Resource> pInstanceDesc;
};

class RtRenderPipelineStage;
class ModelListener;

// Handles all Model loading actions and contains all data needed for RT structures
// load actions are run on a seperate thread, which is why this is a singleton (want only one loading thread ATM)
// recommended to not use any of the RT 'Deferred' functions since they'll hang until all enqued ModelLoad actions are complete
class ModelLoader: public DX12TaskQueueThread {
private:
	ModelLoader(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice);
	ModelLoader(ModelLoader const&) = delete;
	void operator=(ModelLoader const&) = delete;

public:
	static ModelLoader& getInstance();

	// Process if any new models have been loaded by the loading thread.
	// This should be called by someone at the start of every frame so we don't have a sudden
	// update of resources in the middle of a frame, which would necessitate more synchronization
	static bool isEmpty();
	// Clears all Models, required that this is called before the ResourceDecay singleton
	// dies, since Model deletion dumps resources into ResourceDecay
	static void destroyAll();

	static bool isModelCountChanged();

	static std::vector<Light> getAllLights(UINT& numPoint, UINT& numDir, UINT& numSpot);
	static std::vector<std::shared_ptr<Model>> getAllRTModels();
	
	static void updateTransforms();

	static std::weak_ptr<Model> loadModel(std::string name, std::string dir, bool usesRT);
	static MeshletModel* loadMeshletModel(std::string name, std::string dir, bool usesRT);
	static void unloadModel(std::string name, std::string dir);

	// Called in ModelListener constructor, should combine with RT user eventually.
	static void registerModelListener(ModelListener* listener);
	// Called in RtRenderPipelineStage setup, sets up a listener to changes in the RT data.
	static void registerRtUser(RtRenderPipelineStage* user);
	// Initial build of RT data, runs on ModelLoader thread, return HANDLE that can be waited on.
	// TODO: remove 'build' methods and only use 'update' operations
	static HANDLE buildRTAccelerationStructureDeferred(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList, std::vector<AccelerationStructureBuffers>& scratchBuffers);
	void buildRTAccelerationStructure(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList, std::vector<AccelerationStructureBuffers>& scratchBuffers);
	// Appends RT structure building commands to 'cmdList', never call Deferred option since if a Model is loading the operation won't start until the model is finished loading
	static HANDLE updateRTAccelerationStructureDeferred(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList);
	// Appends RT structure building commands to 'cmdList', safe to call this from a seperate thread than the thread owned by this object
	void updateRTAccelerationStructure(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList);

	Microsoft::WRL::ComPtr<ID3D12Resource> TLAS = nullptr;
	bool instanceCountChanged = false;
private:
	AccelerationStructureBuffers createBLAS(Model* model, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList);
	void createTLAS(Microsoft::WRL::ComPtr<ID3D12Resource>& tlas, UINT64& tlasSize, std::vector<std::shared_ptr<Model>>& models, std::vector<MeshletModel*>& meshletModels, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList);
	
	void notifyModelListeners(std::weak_ptr<Model> model);

	class ModelLoadTask : public Task {
	public:
		ModelLoadTask(std::shared_ptr<Model> model);
		virtual ~ModelLoadTask() override = default;

		void execute() override;

	private:
		std::shared_ptr<Model> model;
	};

	class ModelLoadSetupTask : public Task {
	public:
		ModelLoadSetupTask(std::shared_ptr<Model> model, std::unique_ptr<Assimp::Importer> importer);
		virtual ~ModelLoadSetupTask() override = default;

		void execute() override;

	private:
		std::shared_ptr<Model> model;
		std::unique_ptr<Assimp::Importer> importer;
	};

	class MeshletModelLoadTask : public Task {
	public:
		MeshletModelLoadTask(DX12TaskQueueThread* taskQueueThread, MeshletModel* model);
		virtual ~MeshletModelLoadTask() override = default;

		void execute() override;

	private:
		MeshletModel* model;
		DX12TaskQueueThread* taskQueueThread;
	};

	class RTStructureLoadTask : public Task {
	public:
		RTStructureLoadTask(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList, std::vector<AccelerationStructureBuffers>& scratchBuffers);
		virtual ~RTStructureLoadTask() override = default;

		void execute() override;

	private:
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList;
		std::vector<AccelerationStructureBuffers>& scratchBuffers;
	};

	class RTStructureUpdateTask : public Task {
	public:
		RTStructureUpdateTask(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList);
		virtual ~RTStructureUpdateTask() override = default;

		void execute() override;

	private:
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList;
	};

	std::mutex modelListenerLock;
	std::vector<ModelListener*> modelListeners;

	// Only have a single copy queue, so have to lock access to it by the processing threads.
	std::mutex commandQueueLock;

	// Since we're storing the models in this class, we need to synchronize access.
	std::mutex databaseLock;
	bool modelCountChanged = false;
	// string is dir + name
	std::unordered_map<std::string, std::shared_ptr<Model>> loadedModels;
	std::unordered_map<std::string, MeshletModel> loadedMeshlets;

	std::vector<RtRenderPipelineStage*> rtUsers;

	UINT64 tlasSize;
	std::unordered_map<Model*, Microsoft::WRL::ComPtr<ID3D12Resource>> BLAS;
	// Since the instanceDesc of each frame is kept seperate, have to store them each frame so they stay in memory.
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, CPU_FRAME_COUNT> instanceScratch;
	AccelerationStructureBuffers tlasScratch;
};

