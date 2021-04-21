#pragma once
#include <DirectXCollision.h>
#include <span>
#include <unordered_map>

#include "ModelLoading\Model.h"

#include "ModelLoading/TextureLoader.h"

// This is mostly a copy of Meshlet Representation from the DirectXMesh library
// But with minor simplifications/convention changes

struct Attribute {
	enum EType : UINT32 {
		Position,
		Normal,
		TexCoord,
		Tangent,
		Bitangent,
		Count
	};

	EType Type;
	UINT32 Offset;
};

struct Subset {
	UINT32 Offset;
	UINT32 Count;
};

struct MeshInfo {
	UINT32 IndexSize;
	UINT32 MeshletCount;

	UINT32 LastMeshletVert;
	UINT32 LastMesheltPrim;
};

struct Meshlet {
	UINT32 VertCount;
	UINT32 VertOffset;
	UINT32 PrimCount;
	UINT32 PrimOffset;
};

struct PackedTriangle {
	UINT32 i0 : 10;
	UINT32 i1 : 10;
	UINT32 i2 : 10;
};

struct CullData {
	DirectX::XMFLOAT4 BoundingSphere; // xyz = center, w = radius
	UINT8 NormalCone[4]; /// xyz = axis, w = -cos(a + 90)
	FLOAT ApexOffset; // apex = center - axis * offset
};

struct MeshletMesh {
	D3D12_INPUT_ELEMENT_DESC LayoutElems[Attribute::Count];
	D3D12_INPUT_LAYOUT_DESC LayoutDesc;

	std::vector<std::span<UINT8>> Verts;
	std::vector<UINT32> VertStrides;
	UINT32 VertCount;
	DirectX::BoundingSphere BoundingSphere;
	DirectX::BoundingBox BoundingBox;

	std::span<Subset> IndexSubsets;
	std::span<UINT8> Indices;
	UINT32 IndexSize;
	UINT32 IndexCount;

	std::span<Subset> MeshletSubsets;
	std::span<Meshlet> Meshlets;
	std::span<UINT8> UniqueVertexIndices;
	std::span<PackedTriangle> PrimitiveIndices;
	std::span<CullData> CullingData;

	// D3D resource references
	std::vector<D3D12_VERTEX_BUFFER_VIEW>  VBViews;
	D3D12_INDEX_BUFFER_VIEW				   IBView;

	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> VertexResources;
	Microsoft::WRL::ComPtr<ID3D12Resource>				IndexResource;
	Microsoft::WRL::ComPtr<ID3D12Resource>				MeshletResource;
	Microsoft::WRL::ComPtr<ID3D12Resource>				UniqueVertexIndexResource;
	Microsoft::WRL::ComPtr<ID3D12Resource>				PrimitiveIndexResource;
	Microsoft::WRL::ComPtr<ID3D12Resource>				CullDataResource;
	Microsoft::WRL::ComPtr<ID3D12Resource>				MeshInfoResource;

	UINT32 GetLastMeshletPackCount(UINT32 subsetIndex, UINT32 maxGroupVerts, UINT32 maxGroupPrims) {
		// Trivial, no Meshlets, nothing to do.
		if (Meshlets.size() == 0) {
			return 0;
		}
		auto& subset = MeshletSubsets[subsetIndex];
		auto& meshlet = Meshlets[(UINT64)subset.Offset + subset.Count - 1];

		return std::min(maxGroupVerts / meshlet.VertCount, maxGroupPrims / meshlet.PrimCount);
	}

	// Unpack primitives
	void GetPrimitive(UINT32 index, UINT32& i0, UINT32& i1, UINT32& i2) const {
		auto prim = PrimitiveIndices[index];
		i0 = prim.i0;
		i1 = prim.i1;
		i2 = prim.i2;
	}

	UINT32 GetVertexIndex(UINT32 index) const {
		const UINT8* addr = UniqueVertexIndices.data() + (UINT64)index * IndexSize;
		if (IndexSize == 4) {
			return *reinterpret_cast<const UINT32*>(addr);
		}
		else {
			return *reinterpret_cast<const UINT16*>(addr);
		}
	}
};

class MeshletModel : public Model {
public:
	MeshletModel(std::string name, std::string dir, bool usesRT, ID3D12Device5* device);
	HRESULT LoadFromFile(const std::string fileName);
	HRESULT UploadGpuResources(ID3D12Device5* device, ID3D12CommandQueue* cmdQueue, ID3D12CommandAllocator* cmdAlloc, ID3D12GraphicsCommandList* cmdList);
	
	UINT32 GetMeshCount() const { return static_cast<UINT32>(m_meshes.size()); }
	const MeshletMesh& GetMesh(UINT32 i) const { return m_meshes[i]; }

	const DirectX::BoundingSphere& GetBoundingSphere() const { return m_boundingSphere; }
	const DirectX::BoundingBox& GetBoundingBox() const { return m_boundingBox; }

	auto begin() { return m_meshes.begin(); }
	auto end() { return m_meshes.end(); }

	void setInstanceCount(UINT count) override;

	bool loaded = false;

	std::unordered_map<MODEL_FORMAT, std::shared_ptr<DX12Texture>> textures;

	bool allTexturesLoaded();

	bool texturesBound = false;

private:
	// Trying to make repeated checks faster
	bool texturesLoaded = false;

private:
	void LoadSimpleMtl();

	std::vector<MeshletMesh> m_meshes;
	DirectX::BoundingSphere m_boundingSphere;
	DirectX::BoundingBox m_boundingBox;

	std::vector<UINT8> m_buffer;
};

