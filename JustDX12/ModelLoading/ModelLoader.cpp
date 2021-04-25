#include "ModelLoading\ModelLoader.h"

#include "DX12Helper.h"

#include "ThreadPool.h"
#include "ResourceDecay.h"
#include "RtRenderPipelineStage.h"
#include "DX12App.h"

ModelLoader::ModelLoader(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice)
	: DX12TaskQueueThread(d3dDevice, D3D12_COMMAND_LIST_TYPE_COPY) {
}
ModelLoader& ModelLoader::getInstance() {
	static ModelLoader instance(DX12App::getDevice());
	return instance;
}
bool ModelLoader::isEmpty() {
	auto& instance = ModelLoader::getInstance();
	std::lock_guard<std::mutex> lk(instance.databaseLock);
	return instance.loadedModels.empty();
}

void ModelLoader::destroyAll() {
	auto& instance = ModelLoader::getInstance();
	instance.BLAS.clear();
	instance.TLAS.Reset();
	instance.loadedModels.clear();
	instance.loadedMeshlets.clear();
}

bool ModelLoader::isModelCountChanged() {
	auto& instance = ModelLoader::getInstance();
	return instance.modelCountChanged;
}

std::vector<Light> ModelLoader::getAllLights(UINT& numPoint, UINT& numDir, UINT& numSpot) {
	auto& instance = ModelLoader::getInstance();
	std::vector<Light> pointLights;
	std::vector<Light> directionalLights;
	std::vector<Light> spotLights;
	for (const auto& model : instance.loadedModels) {
		for (const auto& light : model.second->lights) {
			for (UINT i = 0; i < model.second->getInstanceCount(); i++) {
				Light l;

				l.color = DirectX::XMFLOAT3(light.mColorDiffuse.r, light.mColorDiffuse.g, light.mColorDiffuse.b);

				DirectX::XMVECTOR lightPos = DirectX::XMVectorSet(light.mPosition.x, light.mPosition.y, light.mPosition.z, 1.0f);
				DirectX::XMFLOAT4X4 lightTransform = model.second->scene.findNode(light.mName.C_Str())->getFullTransform();
				DirectX::XMMATRIX lightTransformMatrix = TransposeLoad(&lightTransform);
				lightPos = DirectX::XMVector4Transform(lightPos, lightTransformMatrix);
				DirectX::XMVECTOR lightDir = DirectX::XMVectorSet(light.mDirection.x, light.mDirection.y, light.mDirection.z, 0.0f);
				lightDir = DirectX::XMVector4Transform(lightDir, lightTransformMatrix);

				auto transform = model.second->getTransform(i);
				DirectX::XMStoreFloat3(&l.pos, DirectX::XMVector4Transform(lightPos, TransposeLoad(&transform)));
				DirectX::XMStoreFloat3(&l.dir, DirectX::XMVector4Transform(lightDir, TransposeLoad(&transform)));

				l.fov = light.mAngleInnerCone;
				l.strength = light.mAttenuationQuadratic;

				switch (light.mType) {
				case aiLightSource_POINT:
					pointLights.push_back(l);
					break;
				case aiLightSource_DIRECTIONAL:
					directionalLights.push_back(l);
					break;
				case aiLightSource_SPOT:
					spotLights.push_back(l);
					break;
				default:
					throw "Unknown Light source found";
					break;
				}
			}
		}
	}
	numPoint = (UINT)pointLights.size();
	numDir = (UINT)directionalLights.size();
	numSpot = (UINT)spotLights.size();

	std::vector<Light> outVec;
	outVec.reserve((UINT64)numPoint + numDir + numSpot);
	outVec.insert(outVec.end(), pointLights.begin(), pointLights.end());
	outVec.insert(outVec.end(), directionalLights.begin(), directionalLights.end());
	outVec.insert(outVec.end(), spotLights.begin(), spotLights.end());
	return outVec;
}

std::vector<std::shared_ptr<SimpleModel>> ModelLoader::getAllRTModels() {
	auto& instance = ModelLoader::getInstance();
	std::vector<std::shared_ptr<SimpleModel>> allModels;
	for (auto& m : instance.loadedModels) {
		if (m.second->usesRT) {
			allModels.push_back(m.second);
		}
	}
	return allModels;
}

void ModelLoader::updateTransforms() {
	auto& instance = ModelLoader::getInstance();
	std::lock_guard<std::mutex> lk(instance.databaseLock);
	for (auto& model : instance.loadedModels) {
		model.second->submitUpdates(gFrameIndex);
	}
	for (auto& meshletModel : instance.loadedMeshlets) {
		meshletModel.second->submitUpdates(gFrameIndex);
	}
}

std::weak_ptr<Model> ModelLoader::loadModel(std::string name, std::string dir, bool usesRT) {
	auto& instance = ModelLoader::getInstance();
	std::lock_guard<std::mutex> lk(instance.databaseLock);

	// Meshlet, not normal model.
	if (name.ends_with(".bin")) {
		auto findModel = instance.loadedMeshlets.find(dir + name);
		std::shared_ptr<MeshletModel> meshletModel;
		if (findModel == instance.loadedMeshlets.end()) {
			auto inserted = instance.loadedMeshlets.emplace(std::make_pair((dir + name), std::make_shared<MeshletModel>(name, dir, usesRT, instance.md3dDevice.Get())));
			meshletModel = inserted.first->second;
			instance.enqueue(new MeshletModelLoadTask(&instance, meshletModel));
		}
		else {
			meshletModel = findModel->second;
		}
		return meshletModel;
	}
	else {
		auto findModel = instance.loadedModels.find(dir + name);
		std::weak_ptr<SimpleModel> model;
		if (findModel == instance.loadedModels.end()) {
			// Don't have tracking of loading models, have to see about making this safer, but it does
			// keep the model loading code far simpler.
			std::shared_ptr<SimpleModel> mPtr = std::make_shared<SimpleModel>(name, dir, instance.md3dDevice.Get(), usesRT);
			model = mPtr;
			instance.enqueue(new ModelLoadTask(mPtr));
		}
		else {
			model = findModel->second;
		}
		return model;
	}
}

std::shared_ptr<Model> ModelLoader::loadModelTakeOwnership(std::string name, std::string dir, bool usesRT) {
	auto& instance = ModelLoader::getInstance();
	// Not checking the loaded models since the caller takes ownership and can't take ownership of cached data.
	// Meshlet, not normal model.
	if (name.ends_with(".bin")) {
		std::shared_ptr<MeshletModel> meshletModel = std::make_shared<MeshletModel>(name, dir, usesRT, instance.md3dDevice.Get());

		instance.enqueue(new MeshletModelLoadTask(&instance, meshletModel));

		return meshletModel;
	}
	else {
		std::shared_ptr<SimpleModel> model = std::make_shared<SimpleModel>(name, dir, instance.md3dDevice.Get(), usesRT);

		instance.enqueue(new ModelLoadTask(model, false));

		return model;
	}
}

void ModelLoader::unloadModel(std::string name, std::string dir) {
	auto& instance = ModelLoader::getInstance();
	std::lock_guard<std::mutex> lk(instance.databaseLock);
	auto findModel = instance.loadedModels.find(dir + name);
	if (findModel != instance.loadedModels.end()) {
		instance.loadedModels.erase(findModel);
	}
	instance.modelCountChanged = true;
}

void ModelLoader::registerModelListener(ModelListener* listener) {
	auto& instance = ModelLoader::getInstance();
	{
		std::lock_guard<std::mutex> lk(instance.modelListenerLock);
		instance.modelListeners.push_back(listener);
	}
	std::vector<std::weak_ptr<Model>> initialBroadcastModels;
	{
		std::lock_guard<std::mutex> lk(instance.databaseLock);
		for (const auto& modelPair : instance.loadedModels) {
			initialBroadcastModels.push_back(modelPair.second);
		}
	}
	listener->initialEnroll(initialBroadcastModels);
}

void ModelLoader::registerRtUser(RtRenderPipelineStage* user) {
	auto& instance = ModelLoader::getInstance();
	instance.rtUsers.push_back(user);
}

HANDLE ModelLoader::buildRTAccelerationStructureDeferred(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList, std::vector<AccelerationStructureBuffers>& scratchBuffers) {
	auto& instance = ModelLoader::getInstance();
	instance.enqueue(new RTStructureLoadTask(cmdList, scratchBuffers));
	HANDLE ev = CreateEvent(
		NULL,
		FALSE,
		FALSE,
		NULL);
	instance.enqueue(new SetCpuEventTask(ev));
	return ev;
}

void ModelLoader::buildRTAccelerationStructure(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList, std::vector<AccelerationStructureBuffers>& scratchBuffers) {
	// Don't want to start building before all loading is completed.
	waitOnFence();
	if (!supportsRt()) {
		return;
	}

	std::vector<AccelerationStructureBuffers> blasVec;
	std::vector<std::shared_ptr<SimpleModel>> models;
	std::lock_guard<std::mutex> lk(databaseLock);
	for (auto& model : loadedModels) {
		if (model.second->usesRT) {
			blasVec.push_back(createBLAS(model.second.get(), cmdList));
			models.push_back(model.second);
		}
	}
	std::vector<MeshletModel*> meshletModels;
	for (auto& meshletModel : loadedMeshlets) {
		if (meshletModel.second->usesRT) {
			std::string modelFileName = meshletModel.second->dir + meshletModel.second->name.substr(0, meshletModel.second->name.find_last_of('.')) + ".fbx";
			SimpleModel* modelForMeshlet = loadedModels.find(modelFileName)->second.get();
			blasVec.push_back(createBLAS(modelForMeshlet, cmdList));
			meshletModels.push_back(meshletModel.second.get());
		}
	}

	for (int i = 0; i < blasVec.size(); i++) {
		BLAS[models[i].get()] = blasVec[i].pResult;
		ResourceDecay::destroyAfterDelay(blasVec[i].pScratch);
		SetName(BLAS[models[i].get()].Get(), L"BLAS");
	}

	for (auto& rtUser : rtUsers) {
		rtUser->deferRebuildRtData(models);
	}

	createTLAS(TLAS, tlasSize, models, meshletModels, cmdList);
	SetName(TLAS.Get(), L"TLAS Structure");
	scratchBuffers = blasVec;
}

HANDLE ModelLoader::updateRTAccelerationStructureDeferred(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList) {
	auto& instance = ModelLoader::getInstance();
	instance.enqueue(new RTStructureUpdateTask(cmdList));
	HANDLE ev = CreateEvent(
		NULL,
		FALSE,
		FALSE,
		NULL);
	instance.enqueue(new SetCpuEventTask(ev));
	return ev;
}

void ModelLoader::updateRTAccelerationStructure(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList) {
	if (!supportsRt()) {
		return;
	}

	std::vector<std::shared_ptr<SimpleModel>> models;
	std::lock_guard<std::mutex> lk(databaseLock);
	if (modelCountChanged || instanceCountChanged) {
		ResourceDecay::destroyAfterDelay(tlasScratch.pResult);
		ResourceDecay::destroyAfterDelay(tlasScratch.pScratch);
	}
	for (auto& model : loadedModels) {
		if (model.second->usesRT) {
			models.push_back(model.second);
			if (BLAS.find(model.second.get()) == BLAS.end()) {
				AccelerationStructureBuffers blasScratch = createBLAS(model.second.get(), cmdList);
				ResourceDecay::destroyAfterDelay(blasScratch.pScratch);
				ResourceDecay::destroyAfterDelay(blasScratch.pInstanceDesc);
				BLAS[model.second.get()] = blasScratch.pResult;
			}
		}
	}
	std::vector<MeshletModel*> meshletModels;
	for (auto& meshletModel : loadedMeshlets) {
		if (false && meshletModel.second->usesRT) {
			std::string modelFileName = meshletModel.second->dir + meshletModel.second->name.substr(0, meshletModel.second->name.find_last_of('.')) + ".fbx";
			meshletModels.push_back(meshletModel.second.get());
		}
	}

	if (modelCountChanged || instanceCountChanged) {
		Microsoft::WRL::ComPtr<ID3D12Resource> newTLAS = nullptr;
		createTLAS(newTLAS, tlasSize, models, meshletModels, cmdList);
		ResourceDecay::destroyAfterDelay(TLAS);
		// Problem: Can't get ComPtr to play nice here. So we get stuck with the TLAS going null if this runs .GetAdressOf() gets the actual address of the underlying interface, but it also sucks.
		ResourceDecay::destroyOnDelayAndFillPointer(nullptr, 1, newTLAS, std::addressof(TLAS));
		if (modelCountChanged) {
			for (auto& rtUser : rtUsers) {
				rtUser->deferRebuildRtData(models);
			}
		}
	}
	else {
		createTLAS(TLAS, tlasSize, models, meshletModels, cmdList);
	}
	modelCountChanged = false;
}

AccelerationStructureBuffers ModelLoader::createBLAS(SimpleModel* model, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList) {
	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geomDescs;
	for (auto& mesh : model->meshes) {
		for (UINT i = 0; i < mesh.getInstanceCount(); i++) {
			D3D12_RAYTRACING_GEOMETRY_DESC geomDesc = {};
			geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;

			geomDesc.Triangles.IndexBuffer = model->indexBufferGPU->GetGPUVirtualAddress() + sizeof(unsigned int) * (UINT64)mesh.startIndexLocation;
			geomDesc.Triangles.IndexCount = mesh.indexCount;
			geomDesc.Triangles.IndexFormat = model->indexFormat;

			geomDesc.Triangles.VertexBuffer.StartAddress = model->vertexBufferGPU->GetGPUVirtualAddress() + sizeof(Vertex) * (UINT64)mesh.baseVertexLocation + offsetof(Vertex, pos);
			geomDesc.Triangles.VertexBuffer.StrideInBytes = model->vertexByteStride;
			geomDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
			geomDesc.Triangles.VertexCount = mesh.vertexCount;

			geomDesc.Triangles.Transform3x4 = mesh.getFrameTransformVirtualAddress(i, gFrameIndex);

			// Optimization here would be to attach an opaque or not flag here.
			geomDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;

			geomDescs.push_back(geomDesc);
		}
	}

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
	inputs.NumDescs = (UINT)geomDescs.size();
	inputs.pGeometryDescs = geomDescs.data();
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
	md3dDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

	AccelerationStructureBuffers buffers;
	buffers.pScratch = CreateBlankBuffer(md3dDevice.Get(), cmdList.Get(), info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, gDefaultHeapDesc);
	buffers.pResult = CreateBlankBuffer(md3dDevice.Get(), cmdList.Get(), info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, gDefaultHeapDesc);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
	asDesc.Inputs = inputs;
	asDesc.DestAccelerationStructureData = buffers.pResult->GetGPUVirtualAddress();
	asDesc.ScratchAccelerationStructureData = buffers.pScratch->GetGPUVirtualAddress();

	cmdList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

	auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(buffers.pResult.Get());
	cmdList->ResourceBarrier(1, &uavBarrier);

	return buffers;
}

void ModelLoader::createTLAS(Microsoft::WRL::ComPtr<ID3D12Resource>& tlas, UINT64& tlasSize, std::vector<std::shared_ptr<SimpleModel>>& models, std::vector<MeshletModel*>& meshletModels, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList) {
	// Find the total number of models.
	UINT totalDescs = 0;
	std::vector<UINT> descsPerModel;
	for (const auto& m : models) {
		totalDescs += m->getInstanceCount();
		descsPerModel.push_back(0);
		for (const auto& mesh : m->meshes) {
			descsPerModel.back() += mesh.getInstanceCount();
		}
	}
	totalDescs += (UINT)meshletModels.size();

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
	inputs.NumDescs = totalDescs;
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
	md3dDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

	// Update or create
	if (tlas.Get() != nullptr) {
		D3D12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(tlas.Get());
		cmdList->ResourceBarrier(1, &uavBarrier);
	}
	else {
		tlasScratch.pScratch = CreateBlankBuffer(md3dDevice.Get(), cmdList.Get(), info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, gDefaultHeapDesc);
		tlasScratch.pResult = CreateBlankBuffer(md3dDevice.Get(), cmdList.Get(), info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, gDefaultHeapDesc);
		SetName(tlasScratch.pScratch.Get(), (L"TLAS Scratch: Frame" + std::to_wstring(gFrame)).c_str());
		SetName(tlasScratch.pScratch.Get(), (L"TLAS Result: Frame" + std::to_wstring(gFrame)).c_str());
		tlasSize = info.ResultDataMaxSizeInBytes;
	}

	// Have to tell the TLAS what instances are where (think like instanced draw calls)
	tlasScratch.pInstanceDesc = CreateBlankBuffer(md3dDevice.Get(), cmdList.Get(), sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * totalDescs, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, gUploadHeapDesc);
	instanceScratch[gFrameIndex] = tlasScratch.pInstanceDesc;
	D3D12_RAYTRACING_INSTANCE_DESC* instanceDescs;
	ThrowIfFailed(tlasScratch.pInstanceDesc->Map(0, nullptr, (void**)&instanceDescs));
	ZeroMemory(instanceDescs, sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * totalDescs);
	
	DirectX::XMFLOAT4X4 identity;
	DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());

	UINT instanceIndex = 0;
	UINT startMeshIndex = 0;
	for (UINT i = 0; i < models.size(); i++) {
		for (UINT j = 0; j < models[i]->getInstanceCount(); j++) {
			auto transform = models[i]->getTransform(j);
			DirectX::XMStoreFloat4x4(&identity, (DirectX::XMLoadFloat4x4(&transform)));
			// We can use this later to index when we perform deferred shading. (Technically not what this is intended for,
			//	but what it's intended for can be obtained through CandidateInstanceIndex, so we'll hijack).
			instanceDescs[instanceIndex].InstanceID = startMeshIndex;
			instanceDescs[instanceIndex].InstanceContributionToHitGroupIndex = instanceIndex;
			instanceDescs[instanceIndex].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
			memcpy(instanceDescs[instanceIndex].Transform, &identity, sizeof(instanceDescs[instanceIndex].Transform));
			instanceDescs[instanceIndex].AccelerationStructure = BLAS[models[i].get()]->GetGPUVirtualAddress();
			instanceDescs[instanceIndex].InstanceMask = 0xFF;

			instanceIndex++;
		}
		startMeshIndex += descsPerModel[i];
	}
	for (UINT i = 0; i < meshletModels.size(); i++) {
		auto transform = meshletModels[i]->getTransform(0);
		DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&transform)));
		instanceDescs[instanceIndex].InstanceID = instanceIndex;
		instanceDescs[instanceIndex].InstanceContributionToHitGroupIndex = instanceIndex;
		instanceDescs[instanceIndex].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
		memcpy(instanceDescs[instanceIndex].Transform, &identity, sizeof(instanceDescs[instanceIndex].Transform));
		// This will cause a crash if the meshlet loads before the model
		// TODO : MAKE THAT NOT HAPPEN
		instanceDescs[instanceIndex].AccelerationStructure = BLAS[loadedModels.find(meshletModels[i]->name)->second.get()]->GetGPUVirtualAddress();
		instanceDescs[instanceIndex].InstanceMask = 0xFF;

		instanceIndex++;
	}

	tlasScratch.pInstanceDesc->Unmap(0, nullptr);

	// Create the TLAS
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
	asDesc.Inputs = inputs;
	asDesc.Inputs.InstanceDescs = tlasScratch.pInstanceDesc->GetGPUVirtualAddress();
	asDesc.DestAccelerationStructureData = tlasScratch.pResult->GetGPUVirtualAddress();
	asDesc.ScratchAccelerationStructureData = tlasScratch.pScratch->GetGPUVirtualAddress();
	if (tlas.Get() != nullptr) {
		asDesc.Inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
		asDesc.SourceAccelerationStructureData = tlas->GetGPUVirtualAddress();
	}

	cmdList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

	auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(tlasScratch.pResult.Get());
	cmdList->ResourceBarrier(1, &uavBarrier);

	tlas = tlasScratch.pResult.Get();
	SetName(tlas.Get(), L"TLAS Structure");
}

void ModelLoader::notifyModelListeners(std::weak_ptr<Model> model) {
	std::lock_guard<std::mutex> lk(modelListenerLock);
	for (auto& listener : modelListeners) {
		listener->broadcastNewModel(model);
	}
}

ModelLoader::ModelLoadTask::ModelLoadTask(std::shared_ptr<SimpleModel> model, bool registerToModelLoader) {
	this->model = model;
	this->registerToModelLoader = registerToModelLoader;
}

void ModelLoader::ModelLoadTask::execute() {
	ModelLoader& instance = ModelLoader::getInstance();

	// Have to alloc to pass around, will try allocating a pool of these initially at some point.
	std::unique_ptr<Assimp::Importer> importer = std::make_unique<Assimp::Importer>();

	OutputDebugStringA(("Starting to Load BasicModel: " + model->name + "\n").c_str());
	// Trying to limit IO to a single thread.
	importer->ReadFile(model->dir + "\\" + model->name, 0);

	ThreadPool::enqueue(new ModelLoadSetupTask(model, std::move(importer), registerToModelLoader));
}

ModelLoader::ModelLoadSetupTask::ModelLoadSetupTask(std::shared_ptr<SimpleModel> model, std::unique_ptr<Assimp::Importer> importer, bool registerToModelLoader) {
	this->model = model;
	this->importer = std::move(importer);
	this->registerToModelLoader = registerToModelLoader;
}

void ModelLoader::ModelLoadSetupTask::execute() {
	auto& instance = ModelLoader::getInstance();

	const aiScene* scene = importer->ApplyPostProcessing(aiProcess_GenUVCoords | aiProcess_Triangulate | aiProcess_ConvertToLeftHanded |
		aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace | aiProcess_FindInstances |
		aiProcess_SplitLargeMeshes);
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		std::string error = importer->GetErrorString();
		OutputDebugStringA(("ERROR::ASSIMP::" + error).c_str());
	}
	else {
		OutputDebugStringA(("Finished load, beginning processing/upload: " + model->name + "\n").c_str());
		// TODO: possibly have multiple allocators for model loading.
		{
			std::lock_guard<std::mutex> lk(instance.commandQueueLock);
			instance.waitOnFence();
			instance.mDirectCmdListAlloc->Reset();
			instance.mCommandList->Reset(instance.mDirectCmdListAlloc.Get(), nullptr);
			instance.mDirectCmdListAlloc->SetName(L"ModelLoad");
			model->setup(&instance, scene->mRootNode, scene);
			instance.waitOnFence();
		}
		OutputDebugStringA(("Finished load, beginning processing/upload: " + model->name + "\n").c_str());
		
		// Add the model to the map of loaded models
		// have to add some duplication checking code since the model loading isn't entirely safe.
		// TODO: investigate how to make this far safer than it is.
		std::string modelName = model->name;
		while (!model->allTexturesLoaded()) {

		}
		OutputDebugStringA(("Finished load, beginning processing/upload: " + model->name + "\n").c_str());
		model->loaded = true;
		std::unique_lock<std::mutex> lk(instance.databaseLock);
		while (instance.loadedModels.contains(model->dir + modelName)) {
			modelName.insert(0, "Dupe");
		}
		lk.unlock();
		if (registerToModelLoader) {
			lk.lock();
			instance.loadedModels[model->dir + modelName] = model;
			instance.instanceCountChanged = true;
			instance.modelCountChanged = true;
			instance.notifyModelListeners(model);
		}
	}
}

ModelLoader::MeshletModelLoadTask::MeshletModelLoadTask(DX12TaskQueueThread* taskQueueThread, std::shared_ptr<MeshletModel> model) {
	this->model = model;
	this->taskQueueThread = taskQueueThread;
}

void ModelLoader::MeshletModelLoadTask::execute() {
	auto& instance = ModelLoader::getInstance();

	taskQueueThread->mDirectCmdListAlloc->Reset();
	taskQueueThread->mCommandList->Reset(taskQueueThread->mDirectCmdListAlloc.Get(), nullptr);

	OutputDebugStringA(("Starting to Load Meshlet Model: " + model->name + "\n").c_str());

	model->LoadFromFile(model->dir + "\\" + model->name);

	OutputDebugStringA(("Finished load, beginning processing/upload: " + model->name + "\n").c_str());

	// TODO: possibly have multiple allocators for model loading.
	{
		std::lock_guard<std::mutex> lk(instance.commandQueueLock);
		model->UploadGpuResources(taskQueueThread->md3dDevice.Get(), taskQueueThread->mCommandQueue.Get(), taskQueueThread->mDirectCmdListAlloc.Get(), taskQueueThread->mCommandList.Get());
	}

	ThreadPool::enqueue(new MeshletModelSetupTask(model));
}

ModelLoader::RTStructureLoadTask::RTStructureLoadTask(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList, std::vector<AccelerationStructureBuffers>& scratchBuffers) : scratchBuffers(scratchBuffers) {
	this->cmdList = cmdList;
}

void ModelLoader::RTStructureLoadTask::execute() {
	ModelLoader::getInstance().buildRTAccelerationStructure(cmdList, scratchBuffers);
}

ModelLoader::RTStructureUpdateTask::RTStructureUpdateTask(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList) {
	this->cmdList = cmdList;
}

void ModelLoader::RTStructureUpdateTask::execute() {
	ModelLoader::getInstance().updateRTAccelerationStructure(cmdList);
}

ModelLoader::MeshletModelSetupTask::MeshletModelSetupTask(std::shared_ptr<MeshletModel> model) {
	this->model = model;
}

void ModelLoader::MeshletModelSetupTask::execute() {
	while (!model->allTexturesLoaded() || !model->rtModel->loaded) {
	}

	OutputDebugStringA(("Finished Upload Meshlet Model: " + model->name + "\n").c_str());

	model->loaded = true;
	ModelLoader::getInstance().notifyModelListeners(model);
}
