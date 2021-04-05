#include "ModelLoading\TextureLoader.h"

#include <d3dx12.h>
#include <DirectXTex.h>

#include "Settings.h"
#include "DX12Helper.h"

#include "TextureLoadTask.h"
#include "ResourceDecay.h"
#include "DX12App.h"

TextureLoader::TextureLoader(Microsoft::WRL::ComPtr<ID3D12Device5> dev) :
	TaskQueueThread(dev, D3D12_COMMAND_LIST_TYPE_COPY) {
}

TextureLoader& TextureLoader::getInstance() {
	static TextureLoader instance(DX12App::getDevice());
	return instance;
}

void TextureLoader::destroyAll() {
	textures.clear();
}

DX12Texture* TextureLoader::deferLoad(std::string fileName, std::string dir) {
	if (textures.find(fileName) != textures.end()) {
		return &textures.at(fileName);
	}
	// Can't cause race condition because we only call from one thread.
	DX12Texture t;
	t.Filename = fileName;
	t.dir = dir;
	t.status = TEX_STATUS_NOT_LOADED;
	textures.emplace(fileName, t);
	enqueue(new TextureLoadTask(this, &textures.at(fileName)));
	return &textures.at(fileName);
}

void TextureLoader::loadTexture(DX12Texture* tex) {
	usageIndex = (usageIndex + 1) % CPU_FRAME_COUNT;
	WaitOnFenceForever(getFence(), fenceValueForWait[usageIndex]);

	mDirectCmdListAlloc = frameResourceArray[usageIndex].get()->CmdListAlloc;
	ThrowIfFailed(mDirectCmdListAlloc->Reset());
	mDirectCmdListAlloc->SetName(L"TexLoad");
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	std::string nameDir = tex->dir + "\\" + tex->Filename;
	std::wstring nameDirW = std::wstring(nameDir.begin(), nameDir.end());
	DirectX::ScratchImage* imgData = new DirectX::ScratchImage();
	HRESULT result = DirectX::LoadFromDDSFile(nameDirW.c_str(), DirectX::DDS_FLAGS_NONE,
		nullptr, *imgData);
	assert(result == S_OK);

	const DirectX::TexMetadata& texMetaData = imgData->GetMetadata();
	// No 3d/1d tex for the moment
	assert(texMetaData.dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D);
	assert(texMetaData.dimension != D3D12_RESOURCE_DIMENSION_TEXTURE1D);

	tex->MetaData.Format = texMetaData.format;
	tex->MetaData.Width = (UINT)texMetaData.width;
	tex->MetaData.Height = (UINT)texMetaData.height;
	if (texMetaData.format == DXGI_FORMAT_BC5_UNORM) {
		tex->MetaData.Width = (UINT)DivRoundUp(texMetaData.width, 4) * 4;
		tex->MetaData.Height = (UINT)DivRoundUp(texMetaData.height, 4) * 4;
	}
	tex->MetaData.Flags = D3D12_RESOURCE_FLAG_NONE;
	tex->MetaData.DepthOrArraySize = (UINT16)texMetaData.arraySize;
	tex->MetaData.MipLevels = (UINT16)texMetaData.mipLevels;
	tex->MetaData.SampleDesc.Count = 1;
	tex->MetaData.SampleDesc.Quality = 0;
	tex->MetaData.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	tex->MetaData.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	tex->MetaData.Alignment = 0;

	Microsoft::WRL::ComPtr<ID3D12Resource> textureData;
	md3dDevice->CreateCommittedResource(
		&gDefaultHeapDesc,
		D3D12_HEAP_FLAG_NONE,
		&tex->MetaData,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(textureData.GetAddressOf()));

	UINT64 textureMemorySize = 0;
	// 30 is what I'm expecting the max of mip levels * array size to be
	UINT numRows[30];
	UINT64 rowSizesInBytes[30];
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts[30];
	UINT64 numSubResources = tex->MetaData.MipLevels * (UINT64)tex->MetaData.DepthOrArraySize;
	
	// Fetch to CPU all the params needed for GPU copies
	md3dDevice->GetCopyableFootprints(&tex->MetaData, 0, (UINT)numSubResources,
		0, layouts, numRows, rowSizesInBytes, &textureMemorySize);

	// Create the upload heap for the texture data
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
	auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(CalcBufferByteSize(textureMemorySize, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT));
	md3dDevice->CreateCommittedResource(&gUploadHeapDesc,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&UploadHeap));

	UINT8* mappedData;
	UploadHeap->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));

	for (int i = 0; i < tex->MetaData.DepthOrArraySize; i++) {
		for (int mipIndex = 0; mipIndex < tex->MetaData.MipLevels; mipIndex++) {
			UINT64 subResourceIndex = mipIndex + (i * (UINT64)tex->MetaData.MipLevels);

			D3D12_PLACED_SUBRESOURCE_FOOTPRINT& subLayout = layouts[subResourceIndex];
			UINT64 subResourceHeight = numRows[subResourceIndex];
			UINT64 subResourcePitch = CalcBufferByteSize(subLayout.Footprint.RowPitch,
				D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
			UINT64 subResourceDepth = subLayout.Footprint.Depth;
			UINT8* destSubMemory = mappedData + subLayout.Offset;

			for (int sliceIndex = 0; sliceIndex < subResourceDepth; sliceIndex++) {
				const DirectX::Image* subImg = imgData->GetImage(mipIndex, i, sliceIndex);
				UINT8* sourceSubMemory = subImg->pixels;

				for (int height = 0; height < subResourceHeight; height++) {
					// If the image isn't properly padded, we can duplicate the pixels to fill (i.e. texture isn't sized correctly for compression)
					if (subResourcePitch > subImg->rowPitch) {
						for (int j = 0; j < subResourcePitch / subImg->rowPitch; j++) {
							memcpy(destSubMemory + j * subImg->rowPitch, sourceSubMemory, subImg->rowPitch);
						}
						if (subResourcePitch % subImg->rowPitch != 0) {
							memcpy(destSubMemory + (subResourcePitch / subImg->rowPitch) * subImg->rowPitch, sourceSubMemory, subResourcePitch % subImg->rowPitch);
						}
					}
					else {
						memcpy(destSubMemory, sourceSubMemory, std::min(subResourcePitch, subImg->rowPitch));
					}
					
					destSubMemory += subResourcePitch;
					sourceSubMemory += subImg->rowPitch;
				}
			}
		}
	}

	for (int subResourceIndex = 0; subResourceIndex < numSubResources; subResourceIndex++) {
		D3D12_TEXTURE_COPY_LOCATION dest = {};
		dest.pResource = textureData.Get();
		dest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dest.SubresourceIndex = subResourceIndex;

		D3D12_TEXTURE_COPY_LOCATION source = {};
		source.pResource = UploadHeap.Get();
		source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		source.PlacedFootprint = layouts[subResourceIndex];
		source.PlacedFootprint.Offset = layouts[subResourceIndex].Offset;

		mCommandList->CopyTextureRegion(&dest, 0, 0, 0,
			&source, nullptr);
	}

	mCommandList->Close();
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	int fenceVal = getFenceValue() + 1;
	ResourceDecay::destroyOnEventAndFillPointer(UploadHeap, EventFromFence(getFence().Get(), fenceVal), textureData, &tex->resource);
	fenceValueForWait[usageIndex] = fenceVal;
	setFence(fenceVal);
	tex->curState = D3D12_RESOURCE_STATE_COPY_DEST;
	tex->format = tex->MetaData.Format;
	tex->type = DESCRIPTOR_TYPE_SRV;
	tex->status = TEX_STATUS_LOADED;
	delete imgData;
	OutputDebugStringA(("Loaded Texture: " + tex->Filename + "\n").c_str());
}

void TextureLoader::loadMip(int mipLevel, DX12Texture* texture) {
	// TODO
}
