#pragma once
#include "PipelineStage/RenderPipelineStage.h"

class MeshletRenderPipelineStage : public RenderPipelineStage, public ModelListener {
public:
    MeshletRenderPipelineStage(Microsoft::WRL::ComPtr<ID3D12Device5> d3dDevice, RenderPipelineDesc renderDesc, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect);

protected:
    virtual void buildPSO() override;
    void buildMeshletTexturesDescriptors(MeshletModel* m, int usageIndex);
    // Inherited via ModelListener
    virtual void processModel(std::weak_ptr<Model> model) override;

    virtual void draw() override;

    // Replace this with interface that SimpleModel has for binding as soon as stable.
    UINT meshletIndex = 0;
    std::vector<DX12Descriptor> meshletTexDescs;
    std::vector<std::weak_ptr<MeshletModel>> meshletRenderObjects;
};

