#pragma once
#include <vector>
#include "ModelLoading\Mesh.h"
#include "Tasks\TaskQueueThread.h"
#include <string>
#include <assimp\scene.h>
#include <DirectXCollision.h>
#include <limits>
#include "SceneNode.h"

class DX12Texture;

// Contains all data needed to render a Model to the scene (executed through Mesh list)
// Capable of holding instance/transform/lighting data
// Safe to delete, since Index/Vertex resources get dumped into ResourceDecay on deletion
class Model {
public:
	Model(std::string name, std::string dir, ID3D12Device5* device, bool usesRT = false);
	~Model();

	void setup(TaskQueueThread* thread, aiNode* node, const aiScene* scene);

	bool isLoaded();

	D3D12_VERTEX_BUFFER_VIEW getVertexBufferView()const;
	D3D12_INDEX_BUFFER_VIEW getIndexBufferView()const;

	void refreshAllTransforms();
	void refreshBoundingBox();

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

	// TODO: Redundant, but needed for RT. Should replace this later.
	std::unique_ptr<DX12Resource> vertexBuffer;
	std::unique_ptr<DX12Resource> indexBuffer;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBufferGPU = nullptr;

private:
	void processLights(const aiScene* scene);
	void processMeshes(const aiScene* scene, std::vector<Vertex>& vertices, std::vector<unsigned int>& indices, ID3D12Device5* device);
	void processNodes(const aiScene* scene);
	Mesh processMesh(aiMesh* mesh, const aiScene* scene, std::vector<Vertex>& vertices, std::vector<unsigned int>& indices, ID3D12Device5* device);
	DX12Texture* loadMaterialTexture(aiMaterial* mat, aiTextureType type);
};