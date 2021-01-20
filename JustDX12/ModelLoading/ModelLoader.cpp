#include "ModelLoading\ModelLoader.h"
#include "Tasks\ModelLoadTask.h"
#include "DX12Helper.h"

ModelLoader::ModelLoader(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice)
	: TaskQueueThread(d3dDevice, D3D12_COMMAND_LIST_TYPE_COPY) {

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

MeshletModel* ModelLoader::loadMeshletModel(std::string name, std::string dir) {
	std::lock_guard<std::mutex> lk(databaseLock);

	auto findModel = loadedMeshlets.find(dir + name);
	MeshletModel* meshletModel;
	if (findModel == loadedMeshlets.end()) {
		loadedMeshlets[dir + name] = MeshletModel(name, dir);
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
	for (auto& model : loadedModels) {
		if (model.second.usesRT) {
			blasVec.push_back(createBLAS(&model.second, cmdList));
		}
	}

	AccelerationStructureBuffers tlas = createTLAS(blasVec, tlasSize, cmdList);

	TLAS = tlas.pResult;
	SetName(TLAS.Get(), L"TLAS Structure");

	for (int i = 0; i < blasVec.size(); i++) {
		BLAS.push_back(blasVec[i].pResult);
		SetName(BLAS[i].Get(), L"BLAS");
	}
	scratchBuffers = blasVec;
	scratchBuffers.push_back(tlas);
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

AccelerationStructureBuffers ModelLoader::createTLAS(std::vector<AccelerationStructureBuffers>& blasVec, UINT64& tlasSize, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList) {
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
	// Normally this is larger because of instancing, but for now we don't have instancing
	inputs.NumDescs = blasVec.size();
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
	md3dDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

	AccelerationStructureBuffers buffers;
	buffers.pScratch = CreateBlankBuffer(md3dDevice.Get(), cmdList.Get(), info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, gDefaultHeapDesc);
	buffers.pResult = CreateBlankBuffer(md3dDevice.Get(), cmdList.Get(), info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, gDefaultHeapDesc);
	tlasSize = info.ResultDataMaxSizeInBytes;

	// Have to tell the TLAS what instances are where (thing like instanced draw calls)
	buffers.pInstanceDesc = CreateBlankBuffer(md3dDevice.Get(), cmdList.Get(), sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * blasVec.size(), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, gUploadHeapDesc);
	D3D12_RAYTRACING_INSTANCE_DESC* instanceDescs;
	buffers.pInstanceDesc->Map(0, nullptr, (void**)&instanceDescs);
	ZeroMemory(instanceDescs, sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * blasVec.size());

	DirectX::XMFLOAT4X4 identity;
	DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());

	for (int i = 0; i < blasVec.size(); i++) {
		instanceDescs[i].InstanceID = i;
		instanceDescs[i].InstanceContributionToHitGroupIndex = i;
		instanceDescs[i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
		memcpy(instanceDescs[i].Transform, &identity, sizeof(instanceDescs[i].Transform));
		instanceDescs[i].AccelerationStructure = blasVec[i].pResult->GetGPUVirtualAddress();
		instanceDescs[i].InstanceMask = 0xFF;
	}

	buffers.pInstanceDesc->Unmap(0, nullptr);

	// Create the TLAS
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
	asDesc.Inputs = inputs;
	asDesc.Inputs.InstanceDescs = buffers.pInstanceDesc->GetGPUVirtualAddress();
	asDesc.DestAccelerationStructureData = buffers.pResult->GetGPUVirtualAddress();
	asDesc.ScratchAccelerationStructureData = buffers.pScratch->GetGPUVirtualAddress();

	cmdList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

	auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(buffers.pResult.Get());
	cmdList->ResourceBarrier(1, &uavBarrier);
	
	return buffers;
}
