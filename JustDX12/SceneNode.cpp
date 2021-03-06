#include "SceneNode.h"

SceneNode::SceneNode(DirectX::XMFLOAT4X4 initTransform, std::string name) {
	transform = initTransform;
	this->name = name;
	parent = nullptr;
}

SceneNode* SceneNode::addChild(DirectX::XMFLOAT4X4 initTransform, std::string name) {
	children.push_back(std::make_unique<SceneNode>(initTransform, name));
	SceneNode* newNode = children.back().get();
	newNode->parent = this;
	return newNode;
}

SceneNode::SceneNode() {
	name = "";
	transform = Identity();
	parent = nullptr;
}

const SceneNode* SceneNode::findNode(std::string name) const {
	if (this->name == name) {
		return this;
	}
	for (const auto& child : children) {
		const SceneNode* childResult = child->findNode(name);
		if (childResult != nullptr) {
			return childResult;
		}
	}
	return nullptr;
}

DirectX::XMFLOAT4X4 SceneNode::getFullTransform() const {
	return currentFullTransform;
}

void SceneNode::calculateFullTransform() {
	// Making this safe to call from any node.
	SceneNode* current = this;
	while (current->parent != nullptr) {
		current = current->parent;
	}

	// Soft BFS, since no nodes can be revisited because it's a tree.
	std::queue<std::pair<SceneNode*, DirectX::XMMATRIX>> bfsQueue;
	bfsQueue.push(std::make_pair(current, DirectX::XMMatrixIdentity()));
	DirectX::XMMATRIX nodeTransform;
	DirectX::XMMATRIX nodeFullTransform;
	while (!bfsQueue.empty()) {
		current = bfsQueue.front().first;
		nodeTransform = bfsQueue.front().second;
		bfsQueue.pop();

		// Since these are in the format so to be directly HLSL usagble, multiplication on CPU is done in reverse order
		// Could also just transpose both, multiply, then transpose result, but that's really wasteful.
		nodeFullTransform = DirectX::XMMatrixMultiply(nodeTransform, DirectX::XMLoadFloat4x4(&current->transform));
		DirectX::XMStoreFloat4x4(&current->currentFullTransform, nodeFullTransform);
		for (auto& child : current->children) {
			bfsQueue.push(std::make_pair(child.get(), nodeFullTransform));
		}
	}
}
