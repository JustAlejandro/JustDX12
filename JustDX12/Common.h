#pragma once
#include <DirectXMath.h>

// Common.h: planned to hold classes that are shared between HLSL and C++
// for now though just a fairly bare C++ header file

struct Light {
	DirectX::XMFLOAT3 pos = { 0.0f, 200.0f, 0.0f };
	float strength = 1500.0f;
	DirectX::XMFLOAT3 dir = { 0.0f, -1.0f, 0.0f };
	int padding = 0;
	DirectX::XMFLOAT3 color = { 1.0f, 1.0f, 1.0f };
	float fov = 0.0f;
};
