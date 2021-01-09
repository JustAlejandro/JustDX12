#include "ModelLoading\Mesh.h"
#include "ModelLoading\Model.h"
#include "DX12Helper.h"
#include <d3dcompiler.h>
#include "DX12App.h"
#include "Settings.h"
#include "TextureLoader.h"

#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "D3D12.lib")
Model::Model(std::string name, std::string dir) {
	loaded = false;
	this->name = name;
	this->dir = dir;
	minPoint = { std::numeric_limits<FLOAT>::max(),
				 std::numeric_limits<FLOAT>::max(),
				 std::numeric_limits<FLOAT>::max() };
	maxPoint = { std::numeric_limits<FLOAT>::min(),
				 std::numeric_limits<FLOAT>::min(),
				 std::numeric_limits<FLOAT>::min() };
}

void Model::setup(TaskQueueThread* thread, aiNode* node, const aiScene* scene) {
	processNode(node, scene);

	boundingBox = boundingBoxFromMinMax(minPoint, maxPoint);

	addBoundingBoxesToVertexBuffer();

	vertexByteStride = sizeof(Vertex);
	indexFormat = DXGI_FORMAT_R32_UINT;
	vertexBufferByteSize = (unsigned int)vertices.size() * sizeof(Vertex);
	indexBufferByteSize = (unsigned int)indices.size() * sizeof(unsigned int);

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBufferUploader = nullptr;

	vertexBufferGPU = CreateDefaultBuffer(thread->md3dDevice.Get(),
		thread->mCommandList.Get(),
		vertices.data(), vertexBufferByteSize, vertexBufferUploader);

	indexBufferGPU = CreateDefaultBuffer(thread->md3dDevice.Get(),
		thread->mCommandList.Get(),
		indices.data(), indexBufferByteSize, indexBufferUploader);

	thread->mCommandList->Close();
	ID3D12CommandList* cmdLists[] = { thread->mCommandList.Get() };
	thread->mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	// Wait for the upload to finish before moving on.
	thread->waitOnFence();

#ifdef CLEAR_MODEL_MEMORY
	std::vector<Vertex>().swap(vertices);
	vertices.shrink_to_fit();
	std::vector<unsigned int>().swap(indices);
	indices.shrink_to_fit();
#endif // CLEAR_MODEL_MEMORY
}

void Model::addBoundingBoxesToVertexBuffer() {
	for (auto& mesh : meshes) {
		// Add Bounding Box to index/vertex list
		Vertex BB;
		BB.pos = { mesh.boundingBox.Center.x, mesh.boundingBox.Center.y, mesh.boundingBox.Center.z };
		// Using the normal position to store the extents
		BB.norm = { mesh.boundingBox.Extents.x, mesh.boundingBox.Extents.y, mesh.boundingBox.Extents.z };
		vertices.push_back(BB);
		mesh.boundingBoxVertexLocation = vertices.size() - 1;
		mesh.boundingBoxIndexLocation = 0;
	}
	Vertex BB;
	BB.pos = { boundingBox.Center.x, boundingBox.Center.y, boundingBox.Center.z };
	// Using the normal position to store the extents
	BB.norm = { boundingBox.Extents.x, boundingBox.Extents.y, boundingBox.Extents.z };
	vertices.push_back(BB);
	boundingBoxVertexLocation = vertices.size() - 1;
	boundingBoxIndexLocation = 0;
}

void Model::processNode(aiNode* node, const aiScene* scene) {
	// Assuming 1 Model per scene (can contain multiple meshes)

	for (int i = 0; i < node->mNumMeshes; i++) {
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		meshes.push_back(processMesh(mesh, scene));
	}

	for (int i = 0; i < node->mNumChildren; i++) {
		processNode(node->mChildren[i], scene);
	}
}

Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene) {
	Mesh meshStorage;
	meshStorage.minPoint = { std::numeric_limits<FLOAT>::max(),
							 std::numeric_limits<FLOAT>::max(),
							 std::numeric_limits<FLOAT>::max() };
	meshStorage.maxPoint = { std::numeric_limits<FLOAT>::lowest(),
							 std::numeric_limits<FLOAT>::lowest(),
							 std::numeric_limits<FLOAT>::lowest() };
	meshStorage.baseVertexLocation = vertices.size();
	meshStorage.startIndexLocation = indices.size();
	for (int i = 0; i < mesh->mNumVertices; i++) {
		Vertex vertex;

		vertex.pos = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
		meshStorage.typeFlags |= MODEL_FORMAT_POSITON;

		if (mesh->HasNormals()) {
			meshStorage.typeFlags |= MODEL_FORMAT_NORMAL;
			vertex.norm = { mesh->mNormals[i].x, mesh->mNormals[i].y,mesh->mNormals[i].z };
			if (mesh->HasTangentsAndBitangents()) {
				vertex.tan = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
				vertex.biTan = { mesh->mBitangents[i].x,mesh->mBitangents[i].y,mesh->mBitangents[i].z };
			}
		}
		
		if (mesh->mTextureCoords[0]) {
			meshStorage.typeFlags |= MODEL_FORMAT_TEXCOORD;
			vertex.texC = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
		}
		else {
			vertex.texC = { 0.0f, 0.0f };
		}

		updateBoundingBoxMinMax(meshStorage.minPoint, meshStorage.maxPoint, vertex.pos);
		updateBoundingBoxMinMax(minPoint, maxPoint, vertex.pos);

		vertices.push_back(vertex);
	}

	for (int i = 0; i < mesh->mNumFaces; i++) {
		aiFace face = mesh->mFaces[i];
		for (int j = 0; j < face.mNumIndices; j++) {
			indices.push_back(face.mIndices[j]);
		}
	}

	if (mesh->mMaterialIndex >= 0) {
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
		// TODO: make texture array distinguish between types...
		// Probably just make a map from the typename to the array...
		if (material->GetTextureCount(aiTextureType_DIFFUSE)) {
			meshStorage.textures[MODEL_FORMAT_DIFFUSE_TEX] = loadMaterialTexture(material,
				aiTextureType_DIFFUSE);
			meshStorage.typeFlags |= MODEL_FORMAT_DIFFUSE_TEX;
		}
		if (material->GetTextureCount(aiTextureType_HEIGHT)) {
			meshStorage.textures[MODEL_FORMAT_NORMAL_TEX] = loadMaterialTexture(material,
				aiTextureType_HEIGHT);
			meshStorage.typeFlags |= MODEL_FORMAT_NORMAL_TEX;
		}
		if (material->GetTextureCount(aiTextureType_SPECULAR)) {
			meshStorage.textures[MODEL_FORMAT_SPECULAR_TEX] = loadMaterialTexture(material,
				aiTextureType_SPECULAR);
			meshStorage.typeFlags |= MODEL_FORMAT_SPECULAR_TEX;
		}
		if (material->GetTextureCount(aiTextureType_OPACITY)) {
			meshStorage.textures[MODEL_FORMAT_OPACITY_TEX] = loadMaterialTexture(material,
				aiTextureType_OPACITY);
			meshStorage.typeFlags |= MODEL_FORMAT_OPACITY_TEX;
		}
	}
	meshStorage.indexCount = mesh->mNumFaces * 3;
	meshStorage.boundingBox = boundingBoxFromMinMax(meshStorage.minPoint, meshStorage.maxPoint);
	return meshStorage;
}

DX12Texture* Model::loadMaterialTexture(aiMaterial* mat, aiTextureType type) {
	DX12Texture* texture = nullptr;

	if (mat->GetTextureCount(type) == 0) {
		return texture;
	}
	if (mat->GetTextureCount(type) > 1) {
		OutputDebugStringA("We don't support more than one texture per type for each material, defaulting to first texture seen.");
	}

	TextureLoader& textureLoader = TextureLoader::getInstance();

	aiString aiPath;
	mat->GetTexture(type, 0, &aiPath);
	std::string path = aiPath.C_Str();
	path = path.substr(path.find_last_of("\\") == std::string::npos ? 0 : path.find_last_of("\\")+1, path.length());
	path = path.substr(path.find_last_of("/") == std::string::npos ? 0 : path.find_last_of("/")+1, path.length());
	path = path.substr(0, path.find_last_of('.')) + ".dds";
	texture = textureLoader.deferLoad(path, dir + "\\textures");
	return texture;
}

D3D12_VERTEX_BUFFER_VIEW Model::vertexBufferView() const {
	D3D12_VERTEX_BUFFER_VIEW vbv;
	vbv.BufferLocation = vertexBufferGPU->GetGPUVirtualAddress();
	vbv.StrideInBytes = vertexByteStride;
	vbv.SizeInBytes = vertexBufferByteSize;
	return vbv;
}

D3D12_INDEX_BUFFER_VIEW Model::indexBufferView() const {
	D3D12_INDEX_BUFFER_VIEW ibv;
	ibv.BufferLocation = indexBufferGPU->GetGPUVirtualAddress();
	ibv.Format = indexFormat;
	ibv.SizeInBytes = indexBufferByteSize;
	return ibv;
}
