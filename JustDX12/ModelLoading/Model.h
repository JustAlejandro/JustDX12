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
	DirectX::XMFLOAT3 pos = { 0.0f, 0.0f, 0.0f };

	bool loaded;
	std::string name;
	std::string dir;
	std::vector<Mesh> meshes;

	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;

	unsigned int vertexByteStride;
	unsigned int vertexBufferByteSize;
	unsigned int indexBufferByteSize;
	DXGI_FORMAT indexFormat;
	INT boundingBoxVertexLocation = 0;
	UINT boundingBoxIndexLocation = 0;

	DirectX::BoundingBox boundingBox;
	DirectX::XMFLOAT3 maxPoint;
	DirectX::XMFLOAT3 minPoint;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBufferGPU = nullptr;

	Model(std::string name, std::string dir);
	void setup(TaskQueueThread* thread, aiNode* node, const aiScene* scene);
	void addBoundingBoxesToVertexBuffer();
	void processNode(aiNode* node, const aiScene* scene);
	Mesh processMesh(aiMesh* mesh, const aiScene* scene);
	DX12Texture* loadMaterialTexture(aiMaterial* mat, aiTextureType type);
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView()const;
	D3D12_INDEX_BUFFER_VIEW indexBufferView()const;
};

