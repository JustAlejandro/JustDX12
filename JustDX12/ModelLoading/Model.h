#pragma once
#include <vector>
#include "ModelLoading\Mesh.h"
#include "Tasks\TaskQueueThread.h"
#include <string>
#include <assimp\scene.h>
#include <DirectXCollision.h>
#include <limits>
#include <TransformData.cpp>
#include "SceneNode.h"

class DX12Texture;

class Model {
public:
	bool usesRT;
	TransformData transform;
	std::vector<aiLight> lights;
	std::string name;
	std::string dir;
	std::vector<Mesh> meshes;
	SceneNode scene;

	unsigned int vertexCount;
	unsigned int indexCount;

	unsigned int vertexByteStride;
	unsigned int vertexBufferByteSize;
	unsigned int indexBufferByteSize;
	DXGI_FORMAT vertexFormat;
	DXGI_FORMAT indexFormat;

	DirectX::BoundingBox boundingBox;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBufferGPU = nullptr;

	Model(std::string name, std::string dir, ID3D12Device5* device, bool usesRT = false);
	~Model();
	bool isLoaded();
	void setup(TaskQueueThread* thread, aiNode* node, const aiScene* scene);
	void refreshAllTransforms();
	void refreshBoundingBox();
	void processLights(const aiScene* scene);
	void processMeshes(const aiScene* scene, std::vector<Vertex>& vertices, std::vector<unsigned int>& indices, ID3D12Device5* device);
	void processNodes(const aiScene* scene);
	Mesh processMesh(aiMesh* mesh, const aiScene* scene, std::vector<Vertex>& vertices, std::vector<unsigned int>& indices, ID3D12Device5* device);
	DX12Texture* loadMaterialTexture(aiMaterial* mat, aiTextureType type);
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView()const;
	D3D12_INDEX_BUFFER_VIEW indexBufferView()const;
};

