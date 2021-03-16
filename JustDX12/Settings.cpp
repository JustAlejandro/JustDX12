#include "Settings.h"

extern UINT gScreenWidth = 0;
extern UINT gScreenHeight = 0;

UINT gFrameIndex = 0;
UINT gFrame = 0;

UINT gRtvDescriptorSize = 0;
UINT gDsvDescriptorSize = 0;
UINT gCbvSrvUavDescriptorSize = 0;
UINT gSamplerDescriptorSize = 0;

D3D12_HEAP_PROPERTIES gDefaultHeapDesc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
D3D12_HEAP_PROPERTIES gUploadHeapDesc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

D3D12_FEATURE_DATA_D3D12_OPTIONS6 vrsSupport;
D3D12_FEATURE_DATA_D3D12_OPTIONS5 rtSupport;
D3D12_FEATURE_DATA_D3D12_OPTIONS1 waveSupport;
