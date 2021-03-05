#include "ModelLoading\ModelLoader.h"
#include "Tasks\ModelLoadTask.h"
#include "DX12Helper.h"
#include "ResourceDecay.h"

ModelLoader::ModelLoader(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice)
	: TaskQueueThread(d3dDevice, D3D12_COMMAND_LIST_TYPE_COPY) {

}
bool ModelLoader::allModelsLoaded() {
	newModelLoaded = false;
	for (int i = 0; i < loadingModels.size(); i++) {
		Model* m = loadingModels[i].second.get();
		if (m->isLoaded()) {
			loadedModels[loadingModels[i].first] = std::move(loadingModels[i].second);
			loadingModels.erase(loadingModels.begin() + i);
			i--;
			newModelLoaded = true;
		}
	}
	if (!loadingModels.empty()) {
		return false;
	}
	for (auto& meshletModel : loadedMeshlets) {
		if (!meshletModel.second.loaded) {
			return false;
		}
	}
	return true;
}
std::vector<Light> ModelLoader::getAllLights(UINT& numPoint, UINT& numDir, UINT& numSpot) {
	std::vector<Light> pointLights;
	std::vector<Light> directionalLights;
	std::vector<Light> spotLights;
	for (const auto& model : loadedModels) {
		for (const auto& light : model.second->lights) {
			for (int i = 0; i < model.second->transform.getInstanceCount(); i++) {
				Light l;

				l.color = DirectX::XMFLOAT3(light.mColorDiffuse.r, light.mColorDiffuse.g, light.mColorDiffuse.b);

				DirectX::XMVECTOR lightPos = DirectX::XMVectorSet(light.mPosition.x, light.mPosition.y, light.mPosition.z, 1.0f);
				DirectX::XMFLOAT4X4 lightTransform = model.second->scene.findNode(light.mName.C_Str())->getFullTransform();
				DirectX::XMMATRIX lightTransformMatrix = TransposeLoad(&lightTransform);
				lightPos = DirectX::XMVector4Transform(lightPos, lightTransformMatrix);
				DirectX::XMVECTOR lightDir = DirectX::XMVectorSet(light.mDirection.x, light.mDirection.y, light.mDirection.z, 0.0f);
				lightDir = DirectX::XMVector4Transform(lightDir, lightTransformMatrix);

				auto transform = model.second->transform.getTransform(i);
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
	numPoint = pointLights.size();
	numDir = directionalLights.size();
	numSpot = spotLights.size();

	std::vector<Light> outVec;
	outVec.reserve((UINT64)numPoint + numDir + numSpot);
	outVec.insert(outVec.end(), pointLights.begin(), pointLights.end());
	outVec.insert(outVec.end(), directionalLights.begin(), directionalLights.end());
	outVec.insert(outVec.end(), spotLights.begin(), spotLights.end());
	return outVec;
}

std::weak_ptr<Model> ModelLoader::loadModel(std::string name, std::string dir, bool usesRT = false) {
	std::lock_guard<std::mutex> lk(databaseLock);

	auto findModel = loadedModels.find(dir + name);
	std::weak_ptr<Model> model;
	if (findModel == loadedModels.end()) {
		// Search through what's loading for the model
		for (int i = 0; i < loadingModels.size(); i++) {
			if (loadingModels[i].first == (dir + name)) {
				return loadingModels[i].second;
			}
		}
		loadingModels.push_back(std::make_pair(dir + name, std::make_shared<Model>(name, dir, md3dDevice.Get(), usesRT)));
		model = loadingModels.back().second;
		enqueue(new ModelLoadTask(this, loadingModels.back().second.get()));
	}
	else {
		model = findModel->second;
	}
	return model;
}

MeshletModel* ModelLoader::loadMeshletModel(std::string name, std::string dir, bool usesRT) {
	std::lock_guard<std::mutex> lk(databaseLock);

	auto findModel = loadedMeshlets.find(dir + name);
	MeshletModel* meshletModel;
	if (findModel == loadedMeshlets.end()) {
		auto inserted = loadedMeshlets.try_emplace((dir + name), name, dir, usesRT, md3dDevice.Get());
		meshletModel = &inserted.first->second;
		enqueue(new MeshletModelLoadTask(this, meshletModel));
	}
	else {
		meshletModel = &findModel->second;
	}
	return meshletModel;
}

void ModelLoader::unloadModel(std::string name, std::string dir) {
	std::lock_guard<std::mutex> lk(databaseLock);
	auto findModel = loadedModels.find(dir + name);
	if (findModel != loadedModels.end()) {
		loadedModels.erase(findModel);
	}
}

void ModelLoader::updateTransforms() {
	for (auto& model : loadedModels) {
		model.second->transform.submitUpdates(gFrameIndex);
	}
	for (auto& model : loadingModels) {
		model.second->transform.submitUpdates(gFrameIndex);
	}
	for (auto& meshletModel : loadedMeshlets) {
		meshletModel.second.transform.submitUpdates(gFrameIndex);
	}
}

HANDLE ModelLoader::buildRTAccelerationStructureDeferred(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList, std::vector<AccelerationStructureBuffers>& scratchBuffers) {
	enqueue(new RTStructureLoadTask(this, cmdList, scratchBuffers));
	HANDLE ev = CreateEvent(
		NULL,
		FALSE,
		FALSE,
		NULL);
	enqueue(new SetCpuEventTask(ev));
	return ev;
}

void ModelLoader::buildRTAccelerationStructure(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList, std::vector<AccelerationStructureBuffers>& scratchBuffers) {
	// Don't want to start building before all loading is completed.
	waitOnFence();

	std::vector<AccelerationStructureBuffers> blasVec;
	std::vector<Model*> models;
	for (auto& model : loadedModels) {
		if (model.second->usesRT) {
			blasVec.push_back(createBLAS(model.second.get(), cmdList));
			models.push_back(model.second.get());
		}
	}
	std::vector<MeshletModel*> meshletModels;
	for (auto& meshletModel : loadedMeshlets) {
		if (meshletModel.second.usesRT) {
			std::string modelFileName = meshletModel.second.dir + meshletModel.second.name.substr(0, meshletModel.second.name.find_last_of('.')) + ".fbx";
			Model* modelForMeshlet = loadedModels.find(modelFileName)->second.get();
			blasVec.push_back(createBLAS(modelForMeshlet, cmdList));
			meshletModels.push_back(&meshletModel.second);
		}
	}

	for (int i = 0; i < blasVec.size(); i++) {
		BLAS[models[i]] = blasVec[i].pResult;
		ResourceDecay::DestroyAfterDelay(blasVec[i].pScratch);
		SetName(BLAS[models[i]].Get(), L"BLAS");
	}

	createTLAS(TLAS, tlasSize, models, meshletModels, cmdList);
	SetName(TLAS.Get(), L"TLAS Structure");
	scratchBuffers = blasVec;
}

HANDLE ModelLoader::updateRTAccelerationStructureDeferred(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList) {
	enqueue(new RTStructureUpdateTask(this, cmdList));
	HANDLE ev = CreateEvent(
		NULL,
		FALSE,
		FALSE,
		NULL);
	enqueue(new SetCpuEventTask(ev));
	return ev;
}

void ModelLoader::updateRTAccelerationStructure(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList) {
	std::vector<Model*> models;
	if (newModelLoaded) {
		ResourceDecay::DestroyAfterDelay(tlasScratch.pResult);
		ResourceDecay::DestroyAfterDelay(tlasScratch.pScratch);
	}
	for (auto& model : loadedModels) {
		if (model.second->usesRT) {
			models.push_back(model.second.get());
			if (BLAS.find(model.second.get()) == BLAS.end()) {
				AccelerationStructureBuffers blasScratch = createBLAS(model.second.get(), cmdList);
				ResourceDecay::DestroyAfterDelay(blasScratch.pScratch);
				ResourceDecay::DestroyAfterDelay(blasScratch.pInstanceDesc);
				BLAS[model.second.get()] = blasScratch.pResult;
			}
		}
	}
	std::vector<MeshletModel*> meshletModels;
	for (auto& meshletModel : loadedMeshlets) {
		if (meshletModel.second.usesRT) {
			std::string modelFileName = meshletModel.second.dir + meshletModel.second.name.substr(0, meshletModel.second.name.find_last_of('.')) + ".fbx";
			meshletModels.push_back(&meshletModel.second);
		}
	}

	if (newModelLoaded) {
		Microsoft::WRL::ComPtr<ID3D12Resource> newTLAS = nullptr;
		createTLAS(newTLAS, tlasSize, models, meshletModels, cmdList);
		ResourceDecay::DestroyAfterDelay(TLAS);
		// Problem: Can't get ComPtr to play nice here. So we get stuck with the TLAS going null if this runs .GetAdressOf() gets the actual address of the underlying interface, but it also sucks.
		ResourceDecay::DestroyOnDelayAndFillPointer(nullptr, 1, newTLAS, std::addressof(TLAS));
	}
	else {
		createTLAS(TLAS, tlasSize, models, meshletModels, cmdList);
	}
	frame++;
}

AccelerationStructureBuffers ModelLoader::createBLAS(Model* model, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList) {
	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geomDescs;
	for (auto& mesh : model->meshes) {
		for (int i = 0; i < mesh.meshTransform.getInstanceCount(); i++) {
			D3D12_RAYTRACING_GEOMETRY_DESC geomDesc = {};
			geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;

			geomDesc.Triangles.IndexBuffer = model->indexBufferGPU->GetGPUVirtualAddress() + sizeof(unsigned int) * (UINT64)mesh.startIndexLocation;
			geomDesc.Triangles.IndexCount = mesh.indexCount;
			geomDesc.Triangles.IndexFormat = model->indexFormat;

			geomDesc.Triangles.VertexBuffer.StartAddress = model->vertexBufferGPU->GetGPUVirtualAddress() + sizeof(Vertex) * (UINT64)mesh.baseVertexLocation;
			geomDesc.Triangles.VertexBuffer.StrideInBytes = model->vertexByteStride;
			geomDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
			geomDesc.Triangles.VertexCount = mesh.vertexCount;

			geomDesc.Triangles.Transform3x4 = mesh.meshTransform.getFrameTransformVirtualAddress(i, gFrameIndex);

			geomDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;


			geomDescs.push_back(geomDesc);
		}
	}

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
	inputs.NumDescs = geomDescs.size();
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

void ModelLoader::createTLAS(Microsoft::WRL::ComPtr<ID3D12Resource>& tlas, UINT64& tlasSize, std::vector<Model*>& models, std::vector<MeshletModel*>& meshletModels, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList) {
	// Find the total number of models.
	UINT totalDescs = 0;
	for (const auto& m : models) {
		totalDescs += m->transform.getInstanceCount();
	}
	totalDescs += meshletModels.size();

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

	int instanceIndex = 0;
	for (int i = 0; i < models.size(); i++) {
		for (int j = 0; j < models[i]->transform.getInstanceCount(); j++) {
			auto transform = models[i]->transform.getTransform(j);
			DirectX::XMStoreFloat4x4(&identity, (DirectX::XMLoadFloat4x4(&transform)));
			instanceDescs[instanceIndex].InstanceID = instanceIndex;
			instanceDescs[instanceIndex].InstanceContributionToHitGroupIndex = instanceIndex;
			instanceDescs[instanceIndex].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
			memcpy(instanceDescs[instanceIndex].Transform, &identity, sizeof(instanceDescs[instanceIndex].Transform));
			instanceDescs[instanceIndex].AccelerationStructure = BLAS[models[i]]->GetGPUVirtualAddress();
			instanceDescs[instanceIndex].InstanceMask = 0xFF;

			instanceIndex++;
		}
	}
	for (int i = 0; i < meshletModels.size(); i++) {
		auto transform = meshletModels[i]->transform.getTransform(0);
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
