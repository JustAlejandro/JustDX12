#include "ModelLoading\Mesh.h"
#include "ModelLoading\Model.h"
#include "DX12Helper.h"
#include <d3dcompiler.h>
#include "DX12App.h"
#include "Settings.h"

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
Model::Model(std::string name, std::string dir) {
	loaded = false;
	this->name = name;
	this->dir = dir;
}

void Model::setup(TaskQueueThread* thread, aiNode* node, const aiScene* scene) {
	processNode(node, scene);

	vertexByteStride = sizeof(Vertex);
	indexFormat = DXGI_FORMAT_R32_UINT;
	vertexBufferByteSize = (unsigned int)vertices.size() * sizeof(Vertex);
	indexBufferByteSize = (unsigned int)indices.size() * sizeof(unsigned int);

	D3DCreateBlob(vertexBufferByteSize, &vertexBufferCPU);
	CopyMemory(vertexBufferCPU->GetBufferPointer(), vertices.data(), vertexBufferByteSize);

	D3DCreateBlob(indexBufferByteSize, &indexBufferCPU);
	CopyMemory(indexBufferCPU->GetBufferPointer(), indices.data(), indexBufferByteSize);

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
	std::vector<unsigned int>().swap(indices);
#endif // CLEAR_MODEL_MEMORY
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
	Mesh meshStorage = { 0 };
	meshStorage.baseVertexLocation = vertices.size();
	meshStorage.startIndexLocation = indices.size();
	for (int i = 0; i < mesh->mNumVertices; i++) {
		Vertex vertex;

		vertex.pos = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
		meshStorage.typeFlags |= MODEL_FORMAT_POSITON;

		if (mesh->HasNormals()) {
			meshStorage.typeFlags |= MODEL_FORMAT_NORMAL;
			vertex.norm = { mesh->mNormals[i].x, mesh->mNormals[i].y,mesh->mNormals[i].z };
			vertex.tan = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
			vertex.biTan = { mesh->mBitangents[i].x,mesh->mBitangents[i].y,mesh->mBitangents[i].z };
		}
		
		if (mesh->mTextureCoords[0]) {
			meshStorage.typeFlags |= MODEL_FORMAT_TEXCOORD;
			vertex.texC = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
		}
		else {
			vertex.texC = { 0.0f, 0.0f };
		}
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
		std::vector<Texture> diffuseTexs = loadMaterialTextures(material,
			aiTextureType_DIFFUSE, "texture_diffuse");
		textures.insert(textures.end(), diffuseTexs.begin(), diffuseTexs.end());
		std::vector<Texture> normalTexs = loadMaterialTextures(material,
			aiTextureType_HEIGHT, "texture_normal");
		textures.insert(textures.end(), normalTexs.begin(), normalTexs.end());
		std::vector<Texture> specTexs = loadMaterialTextures(material,
			aiTextureType_OPACITY, "texture_alpha");
		if (material->GetTextureCount(aiTextureType_DIFFUSE))
			meshStorage.typeFlags |= MODEL_FORMAT_DIFFUSE_TEX;
		if (material->GetTextureCount(aiTextureType_HEIGHT))
			meshStorage.typeFlags |= MODEL_FORMAT_NORMAL;
		if (material->GetTextureCount(aiTextureType_OPACITY))
			meshStorage.typeFlags |= MODEL_FORMAT_OPACITY;
	}
	meshStorage.indexCount = mesh->mNumFaces * 3;
	return meshStorage;
}

std::vector<Texture> Model::loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName) {
	//TODO: Texture Load to GPU, maybe do defferred loading, who knows.
	return std::vector<Texture>();
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
