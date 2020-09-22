#pragma once
#include "Tasks\TaskQueueThread.h"
#include "ModelLoading\Mesh.h"

class TextureLoader : public TaskQueueThread {
public:
	Texture* loadTexture(std::string name, std::string dir);
	void loadMip(int mipLevel, Texture* texture);
};

