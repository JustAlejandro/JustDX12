#include "Common.hlsl"

struct VertexIn {
	float3 PosL : POSITION;
	float3 NormalL : NORMAL;
	float3 TangentL : TANGENT;
	float3 BiNormalL : BINORMAL;
	float2 TexC : TEXCOORD;
};

struct VertexOutMerge {
	float4 PosH : SV_Position;
	float2 TexC : TEXCOORD;
};

struct PixelOutMerge {
	float4 color : SV_Target0;
};

ConstantBuffer<LightData> LightData : register(b0);
ConstantBuffer<SSAOSettings> SSAOSettings : register(b1);

Texture2D depthTex : register(t0);
Texture2D colorTex : register(t1);
Texture2D specTex : register(t2);
Texture2D normalTex : register(t3);
Texture2D tangentTex : register(t4);
Texture2D biNormalTex : register(t5);
Texture2D worldTex : register(t6);

RaytracingAccelerationStructure TLAS : register(t7);

SamplerState gsamPoint : register(s1);

VertexOutMerge DeferVS(VertexIn vin) {
	VertexOutMerge vout;
	vout.PosH = float4(vin.PosL, 1.0);
	vout.TexC = vin.TexC;
	return vout;
}


// Pixel Shader Stuff

static int2 resolution;

int2 clampEdges(int2 index) {
	return clamp(index, int2(0, 0), resolution - int2(1, 1));
}

float linDepth(float depth) {
	return -SSAOSettings.rangeXNear / (depth - SSAOSettings.range);
}

float shadowAmount(int2 texIndex, float3 lightDir, float3 lightPos, float3 worldPos, float3 normal) {
	float occlusion = 0.0f;
	worldPos += normal;
	if (SSAOSettings.showSSShadows) {
		for (int j = 0; j < SSAOSettings.shadowSteps; j++) {
			worldPos.xyz += lightDir * SSAOSettings.shadowStepSize;
			float4 result = mul(float4(worldPos, 1.0f), SSAOSettings.ViewProj);
			result /= result.w;
			result.xy = result.xy * 0.5 + 0.5;
			result.y = result.y * -1.0 + 1.0;
			result.z = linDepth(result.z);
			float compareDepth = linDepth(depthTex[clampEdges((int2) (result.xy * resolution))].x);
			if (compareDepth < result.z && result.z - compareDepth < 500.0) {
				occlusion += 1.0f;
			}
		}
		return max(((SSAOSettings.shadowSteps - occlusion) / SSAOSettings.shadowSteps), !SSAOSettings.showSSShadows);
	}
	else {
		RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> query;

		uint ray_flags = 0; // Any this ray requires in addition those above.
		uint ray_instance_mask = 0xffffffff;

		RayDesc ray;
		ray.TMin = 1e-5f;
		ray.TMax = distance(lightPos, worldPos);
		ray.Origin = worldPos;
		ray.Direction = lightDir;
		query.TraceRayInline(TLAS, ray_flags, ray_instance_mask, ray);

		query.Proceed();

		if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
			return 0.0f;
		}
	}
	return 1.0f;
}

PixelOutMerge DeferPS(VertexOutMerge vout) {
	depthTex.GetDimensions(resolution.x, resolution.y);

	PixelOutMerge pout;
	float3 col = colorTex.Sample(gsamPoint, vout.TexC).xyz;
	float3 diffuse = 0.0f;
	float3 spec = 0.0f;
	float3 worldPos = worldTex.Sample(gsamPoint, vout.TexC).xyz;
	for (int i = 0; i < LightData.numPointLights; i++) {
		float3 lightVec = LightData.lights[i].pos - worldPos;
		float3 lightDir = normalize(lightVec);
		float3 normal = normalTex.Sample(gsamPoint, vout.TexC).xyz;
		float3 reflectDir = reflect(-lightDir, normal);
		float attenuation = clamp(1.0 - dot(lightVec, lightVec) / (LightData.lights[i].strength * LightData.lights[i].strength), 0.0, 1.0);
		float shadowContrib = shadowAmount(vout.TexC, lightDir, LightData.lights[i].pos, worldPos, normal);
		diffuse += clamp(LightData.lights[i].color * attenuation * shadowContrib
			* max(dot(normalTex.Sample(gsamPoint, vout.TexC).xyz, lightDir), 0.0), 0.0, 1.0);
		spec += clamp(LightData.lights[i].color * attenuation * shadowContrib
			* specTex.Sample(gsamPoint, vout.TexC).x
			* pow(max(dot(reflectDir, normalize(LightData.viewPos - worldPos)), 0.0f), 32.0), 0.0, 1.0);
	}
	diffuse = clamp(diffuse + 0.005, 0.0f, 1.0f);
	spec = 1.0 * clamp(spec, 0.0f, 1.0f);
	float3 totalLight = clamp(diffuse + spec, 0.0f, 1.0f);
	pout.color = float4(totalLight * col, 1.0f);
	return pout;
}