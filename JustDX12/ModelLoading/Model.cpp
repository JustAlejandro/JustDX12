#include "ModelLoading\Mesh.h"
#include "ModelLoading\Model.h"
#include "DX12Helper.h"
#include <d3dcompiler.h>
#include "DX12App.h"
#include "Settings.h"
#include "TextureLoader.h"
#include "ConstantBufferTypes.h"
#include "ResourceDecay.h"

#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "D3D12.lib")
Model::Model(std::string name, std::string dir, ID3D12Device5* device, bool usesRT) : transform(device) {
	this->name = name;
	this->dir = dir;
	this->usesRT = usesRT;
	transform.setInstanceCount(1);
	transform.setTransform(0, Identity());
}

Model::~Model() {
	ResourceDecay::DestroyAfterDelay(vertexBufferGPU);
	ResourceDecay::DestroyAfterDelay(indexBufferGPU);
}

bool Model::isLoaded() {
	if ((indexBufferGPU.Get() == nullptr) || (vertexBufferGPU.Get() == nullptr)) {
		return false;
	}
	for (auto& m : meshes) {
		if (!m.allTexturesLoaded()) {
			return false;
		}
	}
	return true;
}

void Model::setup(TaskQueueThread* thread, aiNode* node, const aiScene* scene) {
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;
	processLights(scene);
	processMeshes(scene, vertices, indices, thread->md3dDevice.Get());
	processNodes(scene);
	this->scene.calculateFullTransform();
	refreshAllTransforms();
	refreshBoundingBox();

	indexCount = (UINT)indices.size();
	vertexCount = (UINT)vertices.size();

	vertexByteStride = sizeof(Vertex);
	indexFormat = DXGI_FORMAT_R32_UINT;
	vertexBufferByteSize = vertexCount * sizeof(Vertex);
	indexBufferByteSize = indexCount * sizeof(unsigned int);

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBufferUploader = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer = CreateDefaultBuffer(thread->md3dDevice.Get(),
		thread->mCommandList.Get(),
		vertices.data(), vertexBufferByteSize, vertexBufferUploader);
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer = CreateDefaultBuffer(thread->md3dDevice.Get(),
		thread->mCommandList.Get(),
		indices.data(), indexBufferByteSize, indexBufferUploader);

	thread->mCommandList->Close();
	ID3D12CommandList* cmdLists[] = { thread->mCommandList.Get() };
	thread->mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	// the ResourceDecay class will tell the model about the resource only once the copy is completed.
	int fenceVal = thread->getFenceValue() + 1;
	ResourceDecay::DestroyOnEventAndFillPointer(vertexBufferUploader, EventFromFence(thread->getFence().Get(), fenceVal), vertexBuffer, &vertexBufferGPU);
	ResourceDecay::DestroyOnEventAndFillPointer(indexBufferUploader, EventFromFence(thread->getFence().Get(), fenceVal), indexBuffer, &indexBufferGPU);
	thread->setFence(fenceVal);

#ifdef CLEAR_MODEL_MEMORY
	std::vector<Vertex>().swap(vertices);
	vertices.shrink_to_fit();
	std::vector<unsigned int>().swap(indices);
	indices.shrink_to_fit();
#endif // CLEAR_MODEL_MEMORY
}

void Model::refreshAllTransforms() {
	for (Mesh& mesh : meshes) {
		mesh.updateTransform();
		mesh.meshTransform.submitUpdatesAll();
	}
}

void Model::refreshBoundingBox() {
	if (meshes.size() > 0) {
		meshes[0].boundingBox.Transform(boundingBox, TransposeLoad(meshes[0].meshTransform.getTransform(0)));
	}
	DirectX::BoundingBox scratchBB;
	DirectX::BoundingBox meshBB;
	for (const auto& mesh : meshes) {
		for (UINT i = 0; i < mesh.meshTransform.getInstanceCount(); i++) {
			mesh.boundingBox.Transform(meshBB, TransposeLoad(mesh.meshTransform.getTransform(i)));
			scratchBB = boundingBox;
			DirectX::BoundingBox::CreateMerged(boundingBox, meshBB, scratchBB);
		}
	}
}

void Model::processLights(const aiScene* scene) {
	for (UINT i = 0; i < scene->mNumLights; i++) {
		lights.push_back(scene->mLights[0][i]);
	}
}

void Model::processMeshes(const aiScene* scene, std::vector<Vertex>& vertices, std::vector<unsigned int>& indices, ID3D12Device5* device) {
	for (UINT i = 0; i < scene->mNumMeshes; i++) {
		meshes.push_back(processMesh(scene->mMeshes[i], scene, vertices, indices, device));
		meshes.back().parent = this;
	}
}

void Model::processNodes(const aiScene* scene) {
	SceneNode* currentNode = &this->scene;
	aiNode* currentAiNode = scene->mRootNode;
	currentNode->name = currentAiNode->mName.C_Str();
	DirectX::XMFLOAT4X4 nodeTrans(&currentAiNode->mTransformation.a1);
	DirectX::XMStoreFloat4x4(&currentNode->transform, DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&nodeTrans)));

	std::queue<std::pair<SceneNode*, aiNode*>> nodes;
	nodes.push(std::make_pair(currentNode, currentAiNode));
	while (!nodes.empty()) {
		currentNode = nodes.front().first;
		currentAiNode = nodes.front().second;
		nodes.pop();

		for (UINT i = 0; i < currentAiNode->mNumChildren; i++) {
			DirectX::XMFLOAT4X4 childTranform = DirectX::XMFLOAT4X4(&currentAiNode->mChildren[i]->mTransformation.a1);
			DirectX::XMStoreFloat4x4(&childTranform, DirectX::XMLoadFloat4x4(&childTranform));
			nodes.push(std::make_pair(currentNode->addChild(childTranform, currentAiNode->mChildren[i]->mName.C_Str()), currentAiNode->mChildren[i]));
		}

		for (UINT j = 0; j < currentAiNode->mNumMeshes; j++) {
			meshes[currentAiNode->mMeshes[j]].registerInstance(currentNode);
		}
	}
	this->scene.calculateFullTransform();
}

Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene, std::vector<Vertex>& vertices, std::vector<unsigned int>& indices, ID3D12Device5* device) {
	Mesh meshStorage(device);

	DirectX::XMFLOAT3 minPoint = { std::numeric_limits<FLOAT>::max(),
							 std::numeric_limits<FLOAT>::max(),
							 std::numeric_limits<FLOAT>::max() };
	DirectX::XMFLOAT3 maxPoint = { std::numeric_limits<FLOAT>::lowest(),
							 std::numeric_limits<FLOAT>::lowest(),
							 std::numeric_limits<FLOAT>::lowest() };

	meshStorage.baseVertexLocation = (UINT)vertices.size();
	meshStorage.startIndexLocation = (UINT)indices.size();
	for (UINT i = 0; i < mesh->mNumVertices; i++) {
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
		updateBoundingBoxMinMax(minPoint, maxPoint, vertex.pos);

		vertices.push_back(vertex);
	}

	for (UINT i = 0; i < mesh->mNumFaces; i++) {
		aiFace face = mesh->mFaces[i];
		for (UINT j = 0; j < face.mNumIndices; j++) {
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
		if (material->GetTextureCount(aiTextureType_NORMALS)) {
			meshStorage.textures[MODEL_FORMAT_NORMAL_TEX] = loadMaterialTexture(material,
				aiTextureType_NORMALS);
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
		if (material->GetTextureCount(aiTextureType_EMISSIVE)) {
			meshStorage.textures[MODEL_FORMAT_EMMISIVE_TEX] = loadMaterialTexture(material,
				aiTextureType_EMISSIVE);
			meshStorage.typeFlags |= MODEL_FORMAT_EMMISIVE_TEX;
		}
	}
	meshStorage.indexCount = mesh->mNumFaces * 3;
	meshStorage.vertexCount = mesh->mNumVertices;
	meshStorage.boundingBox = boundingBoxFromMinMax(minPoint, maxPoint);
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
