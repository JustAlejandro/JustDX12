#pragma once
#include <DirectXMath.h>
#include <DX12Helper.h>
#include "ConstantBufferData.h"
#include <Settings.h>
#include "Common.h"

#include <iostream>

class SSAOConstants : public ConstantBufferData {
public:
	struct SSAOConstantsStruct {
		BOOL showSSAO = true;
		BOOL showSSShadows = false;
		DirectX::XMFLOAT2 padding = {0.0f, 0.0f};
		DirectX::XMFLOAT4X4 viewProj = Identity();
		int rayCount = 3;
		float rayLength = 1.0f;
		int TAA = 0;
		float range = 0.0f;
		float rangeXNear = 0.0f;
		int shadowSteps = 10;
		float shadowStepSize = 0.5f;
	};

	SSAOConstantsStruct data;

	virtual UINT byteSize() const override {
		return sizeof(SSAOConstantsStruct);
	}
	virtual std::unique_ptr<ConstantBufferData> clone() const override {
		return std::make_unique<SSAOConstants>(*this);
	}
	void* getData() override {
		return &data;
	};
	virtual ~SSAOConstants() override {}
};


class PerObjectConstants : public ConstantBufferData {
public:
	struct PerObjectConstantsStruct {
		DirectX::XMFLOAT4X4 World[MAX_INSTANCES] = { Identity() };
		UINT instanceCount = 0;
		DirectX::XMFLOAT3 padding;
	};

	PerObjectConstantsStruct data;

	virtual UINT byteSize() const override {
		return sizeof(PerObjectConstantsStruct);
	}
	virtual std::unique_ptr<ConstantBufferData> clone() const override {
		return std::make_unique<PerObjectConstants>(*this);
	}
	void* getData() override {
		return &data;
	};
	virtual ~PerObjectConstants() override {}
};

class PerPassConstants : public ConstantBufferData {
public:
	struct PerPassConstantsStruct {
		DirectX::XMFLOAT4X4 view = Identity();
		DirectX::XMFLOAT4X4 invView = Identity();
		DirectX::XMFLOAT4X4 proj = Identity();
		DirectX::XMFLOAT4X4 invProj = Identity();
		DirectX::XMFLOAT4X4 viewProj = Identity();
		DirectX::XMFLOAT4X4 invViewProj = Identity();
		DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
		BOOL meshletCull = 1;
		DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
		DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
		float NearZ = 0.0f;
		float FarZ = 0.0f;
		float TotalTime = 0.0f;
		float DeltaTime = 0.0f;
		float VrsShort = shadingRateDistance[0];
		float VrsMedium = shadingRateDistance[1];
		float VrsLong = shadingRateDistance[2];
		BOOL renderVRS = 1;
	};

	PerPassConstantsStruct data;

	virtual UINT byteSize() const override {
		return sizeof(PerPassConstantsStruct);
	}
	virtual std::unique_ptr<ConstantBufferData> clone() const override {
		return std::make_unique<PerPassConstants>(*this);
	}
	void* getData() override {
		return &data;
	}
	virtual ~PerPassConstants() override {}
};

class LightData : public ConstantBufferData {
public:
	struct LightDataStruct {
		DirectX::XMFLOAT3 viewPos;
		float exposure = 0.7f;
		UINT numPointLights = 0;
		UINT numDirectionalLights = 0;
		UINT numSpotLights = 0;
		float gamma = 2.2f;
		Light lights[MAX_LIGHTS];
	};

	LightDataStruct data;

	virtual UINT byteSize() const override {
		return sizeof(LightDataStruct);
	}
	virtual std::unique_ptr<ConstantBufferData> clone() const override {
		return std::make_unique<LightData>(*this);
	}
	void* getData() override {
		return &data;
	}
	virtual ~LightData() override {};
};

class VrsConstants : public ConstantBufferData {
public:
	// Default values are just some test values that actually look 'decent'
	struct VrsConstantsStruct {
		BOOL vrsAvgLum = true;
		BOOL vrsVarLum = true;
		float vrsAvgCut = 0.007f;
		float vrsVarianceCut = 0.007f;
		int vrsVarianceVotes = 67;
	};

	VrsConstantsStruct data;

	virtual UINT byteSize() const override {
		return sizeof(VrsConstantsStruct);
	}
	virtual std::unique_ptr<ConstantBufferData> clone() const override {
		return std::make_unique<VrsConstants>(*this);
	}
	void* getData() override {
		return &data;
	}
	virtual ~VrsConstants() override {};
};