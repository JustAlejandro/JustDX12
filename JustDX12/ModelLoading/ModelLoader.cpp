#include "ModelLoading\ModelLoader.h"
#include "Tasks\ModelLoadTask.h"
#include "DX12Helper.h"

ModelLoader::ModelLoader(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice)
	: TaskQueueThread(d3dDevice, D3D12_COMMAND_LIST_TYPE_COPY) {

}
std::vector<Light> ModelLoader::getAllLights(UINT& numPoint, UINT& numDir, UINT& numSpot) {
	std::vector<Light> pointLights;
	std::vector<Light> directionalLights;
	std::vector<Light> spotLights;
	for (const auto& model : loadedModels) {
		for (const auto& light : model.second.lights) {
			for (int i = 0; i < model.second.instanceCount; i++) {
				Light l;

				l.color = DirectX::XMFLOAT3(light.mColorDiffuse.r, light.mColorDiffuse.g, light.mColorDiffuse.b);

				DirectX::XMVECTOR lightPos = DirectX::XMVectorSet(light.mPosition.x, light.mPosition.y, light.mPosition.z, 1.0f);
				DirectX::XMVECTOR lightDir = DirectX::XMVectorSet(light.mDirection.x, light.mDirection.y, light.mDirection.z, 0.0f);

				DirectX::XMStoreFloat3(&l.pos, DirectX::XMVector4Transform(lightPos, DirectX::XMLoadFloat4x4(&model.second.transform[i])));
				DirectX::XMStoreFloat3(&l.dir, DirectX::XMVector4Transform(lightDir, DirectX::XMLoadFloat4x4(&model.second.transform[i])));

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

Model* ModelLoader::loadModel(std::string name, std::string dir, bool usesRT = false) {
	std::lock_guard<std::mutex> lk(databaseLock);

	auto findModel = loadedModels.find(dir + name);
	Model* model;
	if (findModel == loadedModels.end()) {
		loadedModels[dir + name] = Model(name, dir, usesRT);
		model = &loadedModels[dir + name];
		enqueue(new ModelLoadTask(this, model));
	}
	else {
		model = &findModel->second;
	}
	return model;
}

MeshletModel* ModelLoader::loadMeshletModel(std::string name, std::string dir, bool usesRT) {
	std::lock_guard<std::mutex> lk(databaseLock);

	auto findModel = loadedMeshlets.find(dir + name);
	MeshletModel* meshletModel;
	if (findModel == loadedMeshlets.end()) {
		loadedMeshlets[dir + name] = MeshletModel(name, dir, usesRT);
		meshletModel = &loadedMeshlets[dir + name];
		enqueue(new MeshletModelLoadTask(this, meshletModel));
	}
	else {
		meshletModel = &findModel->second;
	}
	return meshletModel;
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
		if (model.second.usesRT) {
			blasVec.push_back(createBLAS(&model.second, cmdList));
			models.push_back(&model.second);
		}
	}
	std::vector<MeshletModel*> meshletModels;
	for (auto& meshletModel : loadedMeshlets) {
		if (meshletModel.second.usesRT) {
			std::string modelFileName = meshletModel.second.dir + meshletModel.second.name.substr(0, meshletModel.second.name.find_last_of('.')) + ".fbx";
			blasVec.push_back(createBLAS(&loadedModels[modelFileName], cmdList));
			meshletModels.push_back(&meshletModel.second);
		}
	}

	for (int i = 0; i < blasVec.size(); i++) {
		BLAS.push_back(blasVec[i].pResult);
		SetName(BLAS[i].Get(), L"BLAS");
	}

	createTLAS(tlasSize, models, meshletModels, cmdList);

	TLAS = tlasScratch.pResult;
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
	for (auto& model : loadedModels) {
		if (model.second.usesRT) {
			models.push_back(&model.second);
		}
	}
	std::vector<MeshletModel*> meshletModels;
	for (auto& meshletModel : loadedMeshlets) {
		if (meshletModel.second.usesRT) {
			std::string modelFileName = meshletModel.second.dir + meshletModel.second.name.substr(0, meshletModel.second.name.find_last_of('.')) + ".fbx";
			meshletModels.push_back(&meshletModel.second);
		}
	}
	createTLAS(tlasSize, models, meshletModels, cmdList);
	frame++;
}

AccelerationStructureBuffers ModelLoader::createBLAS(Model* model, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList) {
	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geomDescs;
	for (auto& mesh : model->meshes) {
		D3D12_RAYTRACING_GEOMETRY_DESC geomDesc = {};
		geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		
		geomDesc.Triangles.IndexBuffer = model->indexBufferGPU->GetGPUVirtualAddress() + sizeof(unsigned int) * (UINT64)mesh.startIndexLocation;
		geomDesc.Triangles.IndexCount = mesh.indexCount;
		geomDesc.Triangles.IndexFormat = model->indexFormat;

		// Not setting transform.
		geomDesc.Triangles.VertexBuffer.StartAddress = model->vertexBufferGPU->GetGPUVirtualAddress() + sizeof(Vertex) * (UINT64)mesh.baseVertexLocation;
		geomDesc.Triangles.VertexBuffer.StrideInBytes = model->vertexByteStride;
		geomDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
		geomDesc.Triangles.VertexCount = mesh.vertexCount;

		geomDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;


		geomDescs.push_back(geomDesc);
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

void ModelLoader::createTLAS(UINT64& tlasSize, std::vector<Model*>& models, std::vector<MeshletModel*>& meshletModels, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList) {
	// Find the total number of models.
	UINT totalDescs = 0;
	for (const auto& m : models) {
		totalDescs += m->instanceCount;
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
	if (TLAS.Get() != nullptr) {
		D3D12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(TLAS.Get());
		cmdList->ResourceBarrier(1, &uavBarrier);
	}
	else {
		tlasScratch.pScratch = CreateBlankBuffer(md3dDevice.Get(), cmdList.Get(), info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, gDefaultHeapDesc);
		tlasScratch.pResult = CreateBlankBuffer(md3dDevice.Get(), cmdList.Get(), info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, gDefaultHeapDesc);
		tlasSize = info.ResultDataMaxSizeInBytes;
	}

	// Have to tell the TLAS what instances are where (thing like instanced draw calls)
	tlasScratch.pInstanceDesc = CreateBlankBuffer(md3dDevice.Get(), cmdList.Get(), sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * totalDescs, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, gUploadHeapDesc);
	instanceScratch[frame % CPU_FRAME_COUNT] = tlasScratch.pInstanceDesc;
	D3D12_RAYTRACING_INSTANCE_DESC* instanceDescs;
	ThrowIfFailed(tlasScratch.pInstanceDesc->Map(0, nullptr, (void**)&instanceDescs));
	ZeroMemory(instanceDescs, sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * totalDescs);
	
	DirectX::XMFLOAT4X4 identity;
	DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());

	int instanceIndex = 0;
	for (int i = 0; i < models.size(); i++) {
		for (int j = 0; j < models[i]->instanceCount; j++) {
			DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&models[i]->transform[j])));
			instanceDescs[instanceIndex].InstanceID = instanceIndex;
			instanceDescs[instanceIndex].InstanceContributionToHitGroupIndex = instanceIndex;
			instanceDescs[instanceIndex].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
			memcpy(instanceDescs[instanceIndex].Transform, &identity, sizeof(instanceDescs[instanceIndex].Transform));
			instanceDescs[instanceIndex].AccelerationStructure = BLAS[i]->GetGPUVirtualAddress();
			instanceDescs[instanceIndex].InstanceMask = 0xFF;

			instanceIndex++;
		}
	}
	for (int i = 0; i < meshletModels.size(); i++) {
		DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&meshletModels[i]->transform)));
		instanceDescs[instanceIndex].InstanceID = instanceIndex;
		instanceDescs[instanceIndex].InstanceContributionToHitGroupIndex = instanceIndex;
		instanceDescs[instanceIndex].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
		memcpy(instanceDescs[instanceIndex].Transform, &identity, sizeof(instanceDescs[instanceIndex].Transform));
		instanceDescs[instanceIndex].AccelerationStructure = BLAS[models.size() + i]->GetGPUVirtualAddress();
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
	if (TLAS.Get() != nullptr) {
		asDesc.Inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
		asDesc.SourceAccelerationStructureData = TLAS->GetGPUVirtualAddress();
	}

	cmdList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

	auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(tlasScratch.pResult.Get());
	cmdList->ResourceBarrier(1, &uavBarrier);
}
