#pragma once
#include <vector>
#include "ModelLoading\Mesh.h"
#include "Tasks\TaskQueueThread.h"
#include <string>
#include <assimp\scene.h>

enum ModelFlags {
	MODEL_FORMAT_POSITON = 1 << 0,
	MODEL_FORMAT_NORMAL	= 1 << 1,
	MODEL_FORMAT_TEXCOORD = 1 << 2,
	MODEL_FORMAT_DIFFUSE_TEX = 1 << 3,
	MODLE_FORMAT_NORMAL_TEX = 1 << 4,
	MODEL_FORMAT_OPACITY = 1 << 5
};

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

	Microsoft::WRL::ComPtr<ID3DBlob> vertexBufferCPU = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> indexBufferCPU = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBufferGPU = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBufferUploader = nullptr;

	Model(std::string name, std::string dir);
	void setup(TaskQueueThread* thread, aiNode* node, const aiScene* scene);
	void processNode(aiNode* node, const aiScene* scene);
	Mesh processMesh(aiMesh* mesh, const aiScene* scene);
	std::vector<DX12Texture*> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName);
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView()const;
	D3D12_INDEX_BUFFER_VIEW indexBufferView()const;
};

