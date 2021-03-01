#pragma once
#include "Tasks\TaskQueueThread.h"
#include "ModelLoading\Mesh.h"
#include <unordered_map>
#include "Texture.h"

class TextureLoader : public TaskQueueThread {
public:
	static TextureLoader& getInstance();
	void clearAll();
	DX12Texture* deferLoad(std::string fileName, std::string dir = "..\\Models\\Sponza\\");
	void loadTexture(DX12Texture* tex);
	void loadMip(int mipLevel, DX12Texture* texture);
private:
	UINT usageIndex = 0;
	std::array<UINT, CPU_FRAME_COUNT> fenceValueForWait = { 0 };
	TextureLoader(Microsoft::WRL::ComPtr<ID3D12Device5> dev);
	TextureLoader(TextureLoader const&) = delete;
	void operator=(TextureLoader const&) = delete;
	std::unordered_map<std::string, DX12Texture> textures;
};

