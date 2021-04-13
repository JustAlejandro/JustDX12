#pragma once
#include <unordered_map>

#include "Texture.h"
#include "ModelLoading\Mesh.h"

#include "Tasks\DX12TaskQueueThread.h"

// Singleton that represents a background thread that loads in textures
class TextureLoader : public DX12TaskQueueThread {
private:
	TextureLoader(Microsoft::WRL::ComPtr<ID3D12Device5> dev);
	TextureLoader(TextureLoader const&) = delete;
	void operator=(TextureLoader const&) = delete;

public:
	static TextureLoader& getInstance();
	void destroyAll();

	// Returns pointer to texture, returnedValue->resource will remain nullptr until texture has completed loading
	std::shared_ptr<DX12Texture> deferLoad(std::string fileName, std::string dir = "..\\Models\\");
	// Will be made private once TextureLoadTask is integrated into this class
	void loadTexture(DX12Texture* tex);
	// Unimplemented
	void loadMip(int mipLevel, DX12Texture* texture);
private:
	UINT usageIndex = 0;
	std::array<UINT, CPU_FRAME_COUNT> fenceValueForWait = { 0 };
	std::unordered_map<std::string, std::weak_ptr<DX12Texture>> textureCache;
};

