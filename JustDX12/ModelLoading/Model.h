#pragma once
#include <vector>
#include "ModelLoading\Mesh.h"
#include "Tasks\TaskQueueThread.h"
#include <string>
#include <assimp\scene.h>
#include <DirectXCollision.h>
#include <limits>

class DX12Texture;

class Model {
public:
	bool loaded;
	bool usesRT;
	unsigned int instanceCount;
	std::vector<DirectX::XMFLOAT4X4> transform;
	std::vector<aiLight> lights;
	std::string name;
	std::string dir;
	std::vector<Mesh> meshes;

	unsigned int vertexCount;
	unsigned int indexCount;

	unsigned int vertexByteStride;
	unsigned int vertexBufferByteSize;
	unsigned int indexBufferByteSize;
	DXGI_FORMAT vertexFormat;
	DXGI_FORMAT indexFormat;

	DirectX::BoundingBox boundingBox;
	DirectX::XMFLOAT3 maxPoint;
	DirectX::XMFLOAT3 minPoint;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBufferGPU = nullptr;

	Model() = default;
	Model(std::string name, std::string dir, bool usesRT = false);
	void setup(TaskQueueThread* thread, aiNode* node, const aiScene* scene);
	void processLights(const aiScene* scene);
	void processNode(aiNode* node, const aiScene* scene, std::vector<Vertex>& vertices, std::vector<unsigned int>& indices);
	Mesh processMesh(aiMesh* mesh, const aiScene* scene, std::vector<Vertex>& vertices, std::vector<unsigned int>& indices);
	DX12Texture* loadMaterialTexture(aiMaterial* mat, aiTextureType type);
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView()const;
	D3D12_INDEX_BUFFER_VIEW indexBufferView()const;
};

