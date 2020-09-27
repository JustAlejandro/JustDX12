#pragma once
#include <DirectXMath.h>
#include <DX12Helper.h>
#include "ConstantBufferData.h"

class SSAOConstants : public ConstantBufferData {
public:
	struct SSAOConstantsStruct {
		int rayCount = 50;
		float maxRange = 0.1;
		float minRange = 0.001;
		int TAA = 0;
	};

	SSAOConstantsStruct data;

	virtual UINT byteSize() const override {
		return CalcConstantBufferByteSize(sizeof(SSAOConstantsStruct));
	}
	virtual std::unique_ptr<ConstantBufferData> clone() const override {
		return std::make_unique<SSAOConstants>(*this);
	}
	void* getData() override {
		return &data;
	};
};

class PerObjectConstants : public ConstantBufferData {
public:
	struct PerObjectConstantsStruct {
		DirectX::XMFLOAT4X4 World = Identity();
	};

	PerObjectConstantsStruct data;

	virtual UINT byteSize() const override {
		return CalcConstantBufferByteSize(sizeof(PerObjectConstantsStruct));
	}
	virtual std::unique_ptr<ConstantBufferData> clone() const override {
		return std::make_unique<PerObjectConstants>(*this);
	}
	void* getData() override {
		return &data;
	};
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
		float cbPadding = 0.0f;
		DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
		DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
		float NearZ = 0.0f;
		float FarZ = 0.0f;
		float TotalTime = 0.0f;
		float DeltaTime = 0.0f;
	};

	PerPassConstantsStruct data;

	virtual UINT byteSize() const override {
		return CalcConstantBufferByteSize(sizeof(PerPassConstantsStruct));
	}
	virtual std::unique_ptr<ConstantBufferData> clone() const override {
		return std::make_unique<PerPassConstants>(*this);
	}
	void* getData() override {
		return &data;
	}
};