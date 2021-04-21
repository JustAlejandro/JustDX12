#pragma once
#include "PipelineStage/RenderPipelineStage.h"

class MeshletRenderPipelineStage : public RenderPipelineStage {
public:
    MeshletRenderPipelineStage(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice, RenderPipelineDesc renderDesc, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect);

protected:
    virtual void buildPSO() override;
    void buildMeshletTexturesDescriptors(MeshletModel* m, int usageIndex);

    virtual void draw() override;

    bool setupRenderObjects();

    std::vector<MeshletModel*> meshletRenderObjects;
};

