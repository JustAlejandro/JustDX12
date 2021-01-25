#include "MeshletModel.h"
#include "DX12Helper.h"

#include <assimp/Importer.hpp>		// C++ importer interface
#include <assimp/scene.h>			// Output data structure
#include <assimp/postprocess.h>		// Post processing flags
#include "Model.h"

#include <iostream>
#include <fstream>
#include <ModelLoading\TextureLoader.h>

const D3D12_INPUT_ELEMENT_DESC elementDescs[Attribute::Count] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 1 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 1 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 1 },
	{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 1 },
	{ "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 1 }
};

const UINT32 sizeMap[] = {
	12, // Position
	12, // Normal
	8,  // TexCoord
	12, // Tangent
	12  // Bitangent
};

const UINT32 prolog = 'MSHL';

enum FileVersion {
	FILE_VERSION_INITIAL = 0,
	CURRENT_FILE_VERSION = FILE_VERSION_INITIAL
};

struct FileHeader {
	UINT32 Prolog;
	UINT32 Version;

	UINT32 MeshCount;
	UINT32 AccessorCount;
	UINT32 BufferViewCount;
	UINT32 BufferSize;
};

struct MeshHeader {
	UINT32 Indices;
	UINT32 IndexSubsets;
	UINT32 Attributes[Attribute::Count];

	UINT32 Meshlets;
	UINT32 MeshletSubsets;
	UINT32 UniqueVertexIndices;
	UINT32 PrimitiveIndices;
	UINT32 CullData;
};

struct BufferView {
	UINT32 Offset;
	UINT32 Size;
};

struct Accessor {
	UINT32 BufferView;
	UINT32 Offset;
	UINT32 Size;
	UINT32 Stride;
	UINT32 Count;
};

MeshletModel::MeshletModel(std::string name, std::string dir, bool usesRT) {
	loaded = false;
	this->name = name;
	this->dir = dir;
	this->usesRT = usesRT;
	transform = Identity();
}

HRESULT MeshletModel::LoadFromFile(const std::string fileName) {
	std::ifstream stream(fileName, std::ios::binary);
	if (!stream.is_open()) {
		return E_INVALIDARG;
	}

	std::vector<MeshHeader> meshes;
	std::vector<BufferView> bufferViews;
	std::vector<Accessor> accessors;

	FileHeader header;
	stream.read(reinterpret_cast<char*>(&header), sizeof(header));

	if (header.Prolog != prolog) {
		return E_FAIL; // Incorrect File Format
	}
	if (header.Version != CURRENT_FILE_VERSION) {
		return E_FAIL; // Version Mismatch
	}
	
	// Start loading the materials once we're sure the model is valid.
	LoadSimpleMtl();

	meshes.resize(header.MeshCount);
	stream.read(reinterpret_cast<char*>(meshes.data()), meshes.size() * sizeof(meshes[0]));

	accessors.resize(header.AccessorCount);
	stream.read(reinterpret_cast<char*>(accessors.data()), accessors.size() * sizeof(accessors[0]));

	bufferViews.resize(header.BufferViewCount);
	stream.read(reinterpret_cast<char*>(bufferViews.data()), bufferViews.size() * sizeof(bufferViews[0]));

	m_buffer.resize(header.BufferSize);
	stream.read(reinterpret_cast<char*>(m_buffer.data()), header.BufferSize);

	char eofbyte;
	stream.read(&eofbyte, 1);

	assert(stream.eof());

	// Fill mesh sources
	m_meshes.resize(meshes.size());
	for (UINT32 i = 0; i < static_cast<UINT32>(meshes.size()); i++) {
		auto& meshView = meshes[i];
		auto& mesh = m_meshes[i];

		// Index Data
		{
			Accessor& accessor = accessors[meshView.Indices];
			BufferView& bufferView = bufferViews[accessor.BufferView];

			mesh.IndexSize = accessor.Size;
			mesh.IndexCount = accessor.Count;

			mesh.Indices = std::span(m_buffer.data() + bufferView.Offset, bufferView.Size);
		}

		// Index Subsets
		{
			Accessor& accessor = accessors[meshView.IndexSubsets];
			BufferView& bufferView = bufferViews[accessor.BufferView];

			mesh.IndexSubsets = std::span(reinterpret_cast<Subset*>(m_buffer.data() + bufferView.Offset), accessor.Count);
		}

		// Vertex data & layout

		std::vector<UINT32> vbMap;

		mesh.LayoutDesc.pInputElementDescs = mesh.LayoutElems;
		mesh.LayoutDesc.NumElements = 0;

		for (UINT32 j = 0; j < Attribute::Count; j++) {
			if (meshView.Attributes[j] == -1) {
				continue;
			}

			Accessor& accessor = accessors[meshView.Attributes[j]];

			auto it = std::find(vbMap.begin(), vbMap.end(), accessor.BufferView);
			if (it != vbMap.end()) {
				continue;
			}

			// New BufferView, so add it to the list
			vbMap.push_back(accessor.BufferView);
			BufferView& bufferView = bufferViews[accessor.BufferView];

			std::span<UINT8> verts = std::span(m_buffer.data() + bufferView.Offset, bufferView.Size);

			mesh.VertStrides.push_back(accessor.Stride);
			mesh.Verts.push_back(verts);
			mesh.VertCount = static_cast<UINT32>(verts.size()) / accessor.Stride;
		}

		// Vertex Buffer Metadata from Accessors
		for (uint32_t j = 0; j < Attribute::Count; ++j) {
			if (meshView.Attributes[j] == -1)
				continue;

			Accessor& accessor = accessors[meshView.Attributes[j]];

			// Determine which vertex buffer index holds this attribute's data
			auto it = std::find(vbMap.begin(), vbMap.end(), accessor.BufferView);

			D3D12_INPUT_ELEMENT_DESC desc = elementDescs[j];
			desc.InputSlot = static_cast<uint32_t>(std::distance(vbMap.begin(), it));

			mesh.LayoutElems[mesh.LayoutDesc.NumElements++] = desc;
		}

		// Meshlet data
		{
			Accessor& accessor = accessors[meshView.Meshlets];
			BufferView& bufferView = bufferViews[accessor.BufferView];

			mesh.Meshlets = std::span(reinterpret_cast<Meshlet*>(m_buffer.data() + bufferView.Offset), accessor.Count);
		}

		// Meshlet Subset data
		{
			Accessor& accessor = accessors[meshView.MeshletSubsets];
			BufferView& bufferView = bufferViews[accessor.BufferView];

			mesh.MeshletSubsets = std::span(reinterpret_cast<Subset*>(m_buffer.data() + bufferView.Offset), accessor.Count);
		}

		// Unique Vertex Index data
		{
			Accessor& accessor = accessors[meshView.UniqueVertexIndices];
			BufferView& bufferView = bufferViews[accessor.BufferView];

			mesh.UniqueVertexIndices = std::span(m_buffer.data() + bufferView.Offset, bufferView.Size);
		}

		// Primitive Index data
		{
			Accessor& accessor = accessors[meshView.PrimitiveIndices];
			BufferView& bufferView = bufferViews[accessor.BufferView];

			mesh.PrimitiveIndices = std::span(reinterpret_cast<PackedTriangle*>(m_buffer.data() + bufferView.Offset), accessor.Count);
		}

		// Cull data
		{
			Accessor& accessor = accessors[meshView.CullData];
			BufferView& bufferView = bufferViews[accessor.BufferView];

			mesh.CullingData = std::span(reinterpret_cast<CullData*>(m_buffer.data() + bufferView.Offset), accessor.Count);
		}
	}

	// Build bounding structures (bounding sphere for mesh shader, bounding box for predication culling)
	for (UINT32 i = 0; i < static_cast<UINT32>(m_meshes.size()); i++) {
		auto& m = m_meshes[i];

		UINT32 vbIndexPos = 0;

		// Find the index of the vertex buffer of the position attribute
		for (UINT32 j = 1; j < m.LayoutDesc.NumElements; ++j) {
			auto& desc = m.LayoutElems[j];
			if (strcmp(desc.SemanticName, "POSITION") == 0) {
				vbIndexPos = j;
				break;
			}
		}

		// Find the byte offset of the position attribute with its vertex buffer
		UINT32 positionOffset = 0;

		for (UINT32 j = 0; j < m.LayoutDesc.NumElements; ++j) {
			auto& desc = m.LayoutElems[j];
			if (strcmp(desc.SemanticName, "POSITION") == 0) {
				break;
			}

			if (desc.InputSlot == vbIndexPos) {
				positionOffset += GetFormatSize(m.LayoutElems[j].Format);
			}
		}

		DirectX::XMFLOAT3* v0 = reinterpret_cast<DirectX::XMFLOAT3*>(m.Verts[vbIndexPos].data() + positionOffset);
		UINT32 stride = m.VertStrides[vbIndexPos];

		DirectX::BoundingSphere::CreateFromPoints(m.BoundingSphere, m.VertCount, v0, stride);
		DirectX::BoundingBox::CreateFromPoints(m.BoundingBox, m.VertCount, v0, stride);

		if (i == 0) {
			m_boundingSphere = m.BoundingSphere;
			m_boundingBox = m.BoundingBox;
		}
		else {
			DirectX::BoundingSphere::CreateMerged(m_boundingSphere, m_boundingSphere, m.BoundingSphere);
			DirectX::BoundingBox::CreateMerged(m_boundingBox, m_boundingBox, m.BoundingBox);
		}
	}
	return S_OK;
}

HRESULT MeshletModel::UploadGpuResources(ID3D12Device5* device, ID3D12CommandQueue* cmdQueue, ID3D12CommandAllocator* cmdAlloc, ID3D12GraphicsCommandList* cmdList) {
	for (UINT32 i = 0; i < m_meshes.size(); ++i) {
		auto& m = m_meshes[i];

		// Create committed D3D resources of proper sizes
		auto indexDesc = CD3DX12_RESOURCE_DESC::Buffer(m.Indices.size());
		auto meshletDesc = CD3DX12_RESOURCE_DESC::Buffer(m.Meshlets.size() * sizeof(m.Meshlets[0]));
		auto cullDataDesc = CD3DX12_RESOURCE_DESC::Buffer(m.CullingData.size() * sizeof(m.CullingData[0]));
		auto vertexIndexDesc = CD3DX12_RESOURCE_DESC::Buffer(DivRoundUp(m.UniqueVertexIndices.size(), 4) * 4);
		auto primitiveDesc = CD3DX12_RESOURCE_DESC::Buffer(m.PrimitiveIndices.size() * sizeof(m.PrimitiveIndices[0]));
		auto meshInfoDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(MeshInfo));

		auto defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &indexDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m.IndexResource)));
		ThrowIfFailed(device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &meshletDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m.MeshletResource)));
		ThrowIfFailed(device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &cullDataDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m.CullDataResource)));
		ThrowIfFailed(device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &vertexIndexDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m.UniqueVertexIndexResource)));
		ThrowIfFailed(device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &primitiveDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m.PrimitiveIndexResource)));
		ThrowIfFailed(device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &meshInfoDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m.MeshInfoResource)));


		m.IBView.BufferLocation = m.IndexResource->GetGPUVirtualAddress();
		m.IBView.Format = m.IndexSize == 4 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
		m.IBView.SizeInBytes = m.IndexCount * m.IndexSize;

		m.VertexResources.resize(m.Verts.size());
		m.VBViews.resize(m.Verts.size());

		for (UINT32 j = 0; j < m.Verts.size(); ++j) {
			auto vertexDesc = CD3DX12_RESOURCE_DESC::Buffer(m.Verts[j].size());
			device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &vertexDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m.VertexResources[j]));

			m.VBViews[j].BufferLocation = m.VertexResources[j]->GetGPUVirtualAddress();
			m.VBViews[j].SizeInBytes = static_cast<UINT32>(m.Verts[j].size());
			m.VBViews[j].StrideInBytes = m.VertStrides[j];
		}

		// Create upload resources
		std::vector<ComPtr<ID3D12Resource>> vertexUploads;
		ComPtr<ID3D12Resource>				indexUpload;
		ComPtr<ID3D12Resource>				meshletUpload;
		ComPtr<ID3D12Resource>				cullDataUpload;
		ComPtr<ID3D12Resource>				uniqueVertexIndexUpload;
		ComPtr<ID3D12Resource>				primitiveIndexUpload;
		ComPtr<ID3D12Resource>				meshInfoUpload;

		auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		ThrowIfFailed(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &indexDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&indexUpload)));
		ThrowIfFailed(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &meshletDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&meshletUpload)));
		ThrowIfFailed(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &cullDataDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&cullDataUpload)));
		ThrowIfFailed(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &vertexIndexDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uniqueVertexIndexUpload)));
		ThrowIfFailed(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &primitiveDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&primitiveIndexUpload)));
		ThrowIfFailed(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &meshInfoDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&meshInfoUpload)));

		// Map & copy memory to upload heap
		vertexUploads.resize(m.Verts.size());
		for (UINT32 j = 0; j < m.Verts.size(); ++j) {
			auto vertexDesc = CD3DX12_RESOURCE_DESC::Buffer(m.Verts[j].size());
			ThrowIfFailed(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &vertexDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexUploads[j])));

			UINT8* memory = nullptr;
			vertexUploads[j]->Map(0, nullptr, reinterpret_cast<void**>(&memory));
			std::memcpy(memory, m.Verts[j].data(), m.Verts[j].size());
			vertexUploads[j]->Unmap(0, nullptr);
		}

		{
			UINT8* memory = nullptr;
			indexUpload->Map(0, nullptr, reinterpret_cast<void**>(&memory));
			std::memcpy(memory, m.Indices.data(), m.Indices.size());
			indexUpload->Unmap(0, nullptr);
		}

		{
			UINT8* memory = nullptr;
			meshletUpload->Map(0, nullptr, reinterpret_cast<void**>(&memory));
			std::memcpy(memory, m.Meshlets.data(), m.Meshlets.size() * sizeof(m.Meshlets[0]));
			meshletUpload->Unmap(0, nullptr);
		}

		{
			UINT8* memory = nullptr;
			cullDataUpload->Map(0, nullptr, reinterpret_cast<void**>(&memory));
			std::memcpy(memory, m.CullingData.data(), m.CullingData.size() * sizeof(m.CullingData[0]));
			cullDataUpload->Unmap(0, nullptr);
		}

		{
			UINT8* memory = nullptr;
			uniqueVertexIndexUpload->Map(0, nullptr, reinterpret_cast<void**>(&memory));
			std::memcpy(memory, m.UniqueVertexIndices.data(), m.UniqueVertexIndices.size());
			uniqueVertexIndexUpload->Unmap(0, nullptr);
		}

		{
			UINT8* memory = nullptr;
			primitiveIndexUpload->Map(0, nullptr, reinterpret_cast<void**>(&memory));
			std::memcpy(memory, m.PrimitiveIndices.data(), m.PrimitiveIndices.size() * sizeof(m.PrimitiveIndices[0]));
			primitiveIndexUpload->Unmap(0, nullptr);
		}

		{
			MeshInfo info = {};
			info.IndexSize = m.IndexSize;
			info.MeshletCount = static_cast<UINT32>(m.Meshlets.size());
			info.LastMeshletVert = m.Meshlets.back().VertCount;
			info.LastMesheltPrim = m.Meshlets.back().PrimCount;


			UINT8* memory = nullptr;
			meshInfoUpload->Map(0, nullptr, reinterpret_cast<void**>(&memory));
			std::memcpy(memory, &info, sizeof(MeshInfo));
			meshInfoUpload->Unmap(0, nullptr);
		}

		// Populate our command list

		for (uint32_t j = 0; j < m.Verts.size(); ++j) {
			cmdList->CopyResource(m.VertexResources[j].Get(), vertexUploads[j].Get());
			//cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m.VertexResources[j].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
		}

		// Since our modelLoader only uses a copy command list, we'll have to transition the resource somewhere else.

		cmdList->CopyResource(m.IndexResource.Get(), indexUpload.Get());
		//cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m.IndexResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

		cmdList->CopyResource(m.MeshletResource.Get(), meshletUpload.Get());
		//cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m.MeshletResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

		cmdList->CopyResource(m.CullDataResource.Get(), cullDataUpload.Get());
		//cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m.CullDataResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

		cmdList->CopyResource(m.UniqueVertexIndexResource.Get(), uniqueVertexIndexUpload.Get());
		//cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m.UniqueVertexIndexResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

		cmdList->CopyResource(m.PrimitiveIndexResource.Get(), primitiveIndexUpload.Get());
		//cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m.PrimitiveIndexResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

		cmdList->CopyResource(m.MeshInfoResource.Get(), meshInfoUpload.Get());
		//cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m.MeshInfoResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

		ThrowIfFailed(cmdList->Close());

		ID3D12CommandList* ppCommandLists[] = { cmdList };
		cmdQueue->ExecuteCommandLists(1, ppCommandLists);

		// Create our sync fence
		ComPtr<ID3D12Fence> fence;
		ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

		cmdQueue->Signal(fence.Get(), 1);

		// Wait for GPU
		if (fence->GetCompletedValue() != 1) {
			HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			fence->SetEventOnCompletion(1, event);

			WaitForSingleObjectEx(event, INFINITE, false);
			CloseHandle(event);
		}
	}
	
	loaded = true;

	return S_OK;
}

bool MeshletModel::allTexturesLoaded() {
	if (texturesLoaded) return true;

	for (const auto& texture : textures) {
		if (texture.second->status == TEX_STATUS_NOT_LOADED) {
			return false;
		}
	}
	texturesLoaded = true;
	return texturesLoaded;
}

void MeshletModel::LoadSimpleMtl() {
	TextureLoader& textureLoader = TextureLoader::getInstance();

	std::fstream mtlFile;
	mtlFile.open(dir + "\\" + name.substr(0, name.find_last_of('.')) + ".simplemtl", std::ios::in);
	
	if (!mtlFile.is_open()) {
		OutputDebugStringA(("Couldn't open .simplemtl file for Meshlet Model: " + name + "\n").c_str());
	}
	std::string line;
	while (std::getline(mtlFile, line)) {
		MODEL_FORMAT texType = simpleMtlTypeToModelFormat(line.substr(0, line.find_first_of(" ")));
		std::string texPath = line.substr(line.find_first_of(" ") + 1, line.length());
		texPath = texPath.substr(texPath.find_last_of("\\") == std::string::npos ? 0 : texPath.find_last_of("\\") + 1, texPath.length());
		texPath = texPath.substr(texPath.find_last_of("/") == std::string::npos ? 0 : texPath.find_last_of("/") + 1, texPath.length());
		texPath = texPath.substr(0, texPath.find_last_of('.')) + ".dds";

		textures[texType] = textureLoader.deferLoad(texPath, dir + "\\textures");
	}
}
