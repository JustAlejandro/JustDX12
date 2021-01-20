#pragma once
#include <DirectXMath.h>
#include <DX12Helper.h>
#include "ConstantBufferData.h"
#include <Settings.h>

#include <iostream>

class SSAOConstants : public ConstantBufferData {
public:
	struct SSAOConstantsStruct {
		BOOL showSSAO = true;
		BOOL showSSShadows = true;
		DirectX::XMFLOAT2 padding = {0.0f, 0.0f};
		DirectX::XMFLOAT4X4 viewProj = Identity();
		int rayCount = 10;
		float rayLength = 3.0f;
		int TAA = 0;
		float range = 0.0f;
		float rangeXNear = 0.0f;
		int shadowSteps = 20;
		float shadowStepSize = 0.05f;
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

struct Light {
	DirectX::XMFLOAT3 pos = { 0.0f, 200.0f, 0.0f };
	float strength = 1500.0f;
	DirectX::XMFLOAT3 dir;
	int padding = 0;
	DirectX::XMFLOAT3 color = { 1.0f, 1.0f, 1.0f };
	float fov = 0.0f;
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
		int ipadding;
		int numPointLights = 0;
		int numDirectionalLights = 0;
		int numSpotLights = 0;
		int padding = 0;
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