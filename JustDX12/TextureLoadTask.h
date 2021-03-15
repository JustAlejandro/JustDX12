#pragma once
#include "Tasks\Task.h"

#include "ModelLoading\TextureLoader.h"

// TODO: this should be in TextureLoader, and private
class TextureLoadTask : public Task {
public:
	TextureLoadTask(TextureLoader* textureLoader, DX12Texture* target) {
		loader = textureLoader;
		this->target = target;
	}

	void execute() override { loader->loadTexture(target); }

	virtual ~TextureLoadTask() override = default;
private:
	TextureLoader* loader;
	DX12Texture* target;
};