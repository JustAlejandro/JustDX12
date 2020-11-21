#pragma once
#define NOMINMAX
#include <Windows.h>
#include <DirectXMath.h>
#include <wrl.h>
#include <d3d12.h>
#include <vector>
#include <string>
#include <unordered_map>
#include "Texture.h"
#include <DirectXCollision.h>

struct CompactBoundingBox {
	DirectX::XMFLOAT3 center;
	DirectX::XMFLOAT3 bounds;
	CompactBoundingBox(DirectX::XMFLOAT3 center, DirectX::XMFLOAT3 bounds) {
		this->center = center;
		this->bounds = bounds;
	}
};

struct Vertex {
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 norm;
	DirectX::XMFLOAT3 tan;
	DirectX::XMFLOAT3 biTan;
	DirectX::XMFLOAT2 texC;
};

struct Mesh {
	UINT typeFlags = 0;
	UINT indexCount = 0;
	UINT startIndexLocation = 0;
	INT baseVertexLocation = 0;
	INT boundingBoxVertexLocation = 0;
	UINT boundingBoxIndexLocation = 0;

	DirectX::BoundingBox boundingBox;
	DirectX::XMFLOAT3 maxPoint;
	DirectX::XMFLOAT3 minPoint;

	std::unordered_map<MODEL_FORMAT, DX12Texture*> textures;

	bool allTexturesLoaded() {
		if (texturesLoaded) return true;

		for (const auto& texture : textures) {
			if (texture.second->status == TEX_STATUS_NOT_LOADED) {
				return false;
			}
		}
		texturesLoaded = true;
		return texturesLoaded;
	}

	bool texturesBound = false;

private:
	// Trying to make repeated checks faster
	bool texturesLoaded = false;
};

