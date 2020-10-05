#pragma once
#include "Tasks\Task.h"
#include "ModelLoading\TextureLoader.h"

class TextureLoadTask : public Task {
public:
	TextureLoadTask(TextureLoader* textureLoader, DX12Texture* target) {
		loader = textureLoader;
		this->target = target;
	}

	void execute() override { loader->loadTexture(target); }

private:
	TextureLoader* loader;
	DX12Texture* target;
};