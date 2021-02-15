#pragma once
#include <DX12Helper.h>
#include <queue>

struct Mesh;

class SceneNode {
public:
	SceneNode();
	SceneNode(DirectX::XMFLOAT4X4 initTransform, std::string name);

	SceneNode* addChild(DirectX::XMFLOAT4X4 initTransform, std::string name);
	void calculateFullTransform();
	const SceneNode* findNode(std::string name) const;
	DirectX::XMFLOAT4X4 getFullTransform() const;

	std::string name;
	DirectX::XMFLOAT4X4 transform;
private:
	SceneNode* parent;
	DirectX::XMFLOAT4X4 currentFullTransform;
	std::vector<std::unique_ptr<SceneNode>> children;
};

