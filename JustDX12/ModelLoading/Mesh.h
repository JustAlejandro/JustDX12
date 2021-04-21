#pragma once
#define NOMINMAX
#include <vector>
#include <string>
#include <unordered_map>
#include <Windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <DirectXCollision.h>

#include "Texture.h"
#include "SceneNode.h"
#include "DescriptorClasses/DX12Descriptor.h"

#include "TransformData.h"
#include <DX12ConstantBuffer.h>

struct CompactBoundingBox {
	DirectX::XMFLOAT3 center;
	DirectX::XMFLOAT3 bounds;
	CompactBoundingBox(DirectX::XMFLOAT3 center, DirectX::XMFLOAT3 bounds) {
		this->center = center;
		this->bounds = bounds;
	}
};

struct Vertex {
	DirectX::XMFLOAT2 texC;
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 norm;
	DirectX::XMFLOAT3 tan;
	DirectX::XMFLOAT3 biTan;
};

class Model;
class PipelineStage;

// Represents a subset of a Model
// Also holds descriptors needed to render the mesh (textures), which PipelineStages can register for retreival later
// Unique: can be instanced, which does cause some headaches, and isn't typically supported in most engines
class Mesh : public TransformData {
public:
	Mesh(ID3D12Device5* device) : TransformData(device) {
		parent = nullptr;
		instanceNodes.fill(nullptr);
	}

	bool allTexturesLoaded() {
		if (texturesLoaded) return true;

		for (const auto& texture : textures) {
			if (texture.second->get() == nullptr) {
				return false;
			}
		}
		texturesLoaded = true;
		return texturesLoaded;
	}

	std::vector<DX12Descriptor> getDescriptorsForStage(PipelineStage* stage) {
		for (int i = 0; i < pipelineStageMappings.size(); i++) {
			if (stage == pipelineStageMappings[i]) {
				return pipelineStageBindings[i];
			}
		}
		throw "Binding was never created for this stage";
	}

	void updateTransform() {
		for (UINT i = 0; i < getInstanceCount(); i++) {
			setTransform(i, instanceNodes[i]->getFullTransform());
		}
	}

	void registerInstance(SceneNode* node) {
		UINT instanceCount = getInstanceCount();
		instanceNodes[instanceCount] = node;
		setInstanceCount(instanceCount + 1);
	}

	void registerPipelineStage(PipelineStage* stage, std::vector<DX12Descriptor> descriptors) {
		INT stageSlot = -1;
		for (int i = 0; i < pipelineStageMappings.size(); i++) {
			if (stage == pipelineStageMappings[i]) {
				stageSlot = i;
			}
		}
		if (stageSlot == -1) {
			stageSlot = (INT)pipelineStageMappings.size();
			pipelineStageMappings.push_back(stage);
			pipelineStageBindings.push_back(std::vector<DX12Descriptor>());
		}
		pipelineStageBindings[stageSlot] = descriptors;
		pipelineStageBindings[stageSlot].shrink_to_fit();
	}

	UINT typeFlags = 0;
	UINT indexCount = 0;
	UINT vertexCount = 0;
	UINT startIndexLocation = 0;
	INT baseVertexLocation = 0;
	INT boundingBoxVertexLocation = 0;
	UINT boundingBoxIndexLocation = 0;

	DirectX::BoundingBox boundingBox;

	std::unordered_map<MODEL_FORMAT, std::shared_ptr<DX12Texture>> textures;

	Model* parent;

private:
	// Trying to make repeated checks faster
	bool texturesLoaded = false;

	std::array<SceneNode*, MAX_INSTANCES> instanceNodes;

	std::vector<std::vector<DX12Descriptor>> pipelineStageBindings;
	std::vector<PipelineStage*> pipelineStageMappings;
};
