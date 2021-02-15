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
Texture2D worldTex : register(t5);
Texture2D emissiveTex : register(t6);

RaytracingAccelerationStructure TLAS : register(t7);

SamplerState gsamPoint : register(s1);

VertexOutMerge DeferVS(VertexIn vin) {
	VertexOutMerge vout;
	vout.PosH = float4(vin.PosL, 1.0);
	vout.TexC = vin.TexC;
	return vout;
}


// Trowbridge-Reitz GGX (from LearnOpenGL)
float DistributionGGX(float3 N, float3 H, float roughness) {
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.0f);
	float NdotH2 = NdotH * NdotH;

	float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
	denom = 3.14159 * denom * denom;

	return a2 / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
	float r = roughness + 1.0f;
	float k = (r * r) / 8.0f;

	return NdotV / (NdotV * (1.0f - k) + k);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
	float NdotV = max(dot(N, V), 0.0f);
	float NdotL = max(dot(N, L), 0.0f);
	float ggx1 = GeometrySchlickGGX(NdotV, roughness);
	float ggx2 = GeometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}

float3 fresnelSchlick(float cosTheta, float3 F0) {
	return F0 + (1.0f - F0) * pow(max(1.0f - cosTheta, 0.0f), 5.0f);
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
	float unoccluded = 1.0;
	if (SSAOSettings.showSSShadows) {
		worldPos += normal;
		for (int j = 0; j < SSAOSettings.shadowSteps; j++) {
			worldPos.xyz += lightDir * SSAOSettings.shadowStepSize;
			float4 result = mul(float4(worldPos, 1.0f), SSAOSettings.ViewProj);
			result /= result.w;
			result.xy = result.xy * 0.5 + 0.5;
			result.y = result.y * -1.0 + 1.0;
			result.z = linDepth(result.z);
			float compareDepth = linDepth(depthTex[clampEdges((int2) (result.xy * resolution))].x);
			if (compareDepth < result.z && result.z - compareDepth < 10.0) {
				unoccluded -= (1.0f / SSAOSettings.shadowSteps);
			}
		}
	}
	else {
		RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> query;

		uint ray_flags = 0; // Any this ray requires in addition those above.
		uint ray_instance_mask = 0xffffffff;

		RayDesc ray;
		ray.TMin = 0.01f;
		ray.TMax = distance(lightPos, worldPos);
		ray.Origin = worldPos;
		ray.Direction = lightDir;
		query.TraceRayInline(TLAS, ray_flags, ray_instance_mask, ray);

		query.Proceed();

		if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
			unoccluded = 0.0f;
		}
	}
	return (float) unoccluded;
}

float3 lightContrib(float3 F0, float3 albedo, float roughness, float metallic, float3 viewDir, float3 normal, float3 lightVec, float3 lightPos, float3 lightColor, float attenuationVal, bool attenuationConst) {
	float3 lightDir = normalize(lightVec);
	float3 H = normalize(viewDir + lightDir);
	float3 reflectDir = reflect(-lightDir, normal);
	float attenuation = attenuationConst ? attenuationVal : clamp(1.0 - dot(lightVec, lightVec) / (attenuationVal * attenuationVal), 0.0, 1.0);

	float3 radiance = lightColor * attenuation;

	float NDF = DistributionGGX(normal, H, roughness);
	float G = GeometrySmith(normal, viewDir, lightDir, roughness);
	float3 F = fresnelSchlick(max(dot(H, viewDir), 0.0f), F0);

	float3 nominator = NDF * G * F;
	float denominator = 4 * max(dot(normal, viewDir), 0.0f) * max(dot(normal, lightDir), 0.0f) + 0.001f;
	float3 specular = nominator / denominator;

	float3 kS = F;

	float3 kD = 1.0 - kS;

	kD *= (1.0 - metallic);

	float NdotL = max(dot(normal, lightDir), 0.0f);

	return (kD * albedo / 3.14159 + specular) * radiance * NdotL;
}

PixelOutMerge DeferPS(VertexOutMerge vout) {
	depthTex.GetDimensions(resolution.x, resolution.y);

	PixelOutMerge pout;

	float3 albedo = colorTex.Sample(gsamPoint, vout.TexC).xyz;
	albedo = pow(albedo, LightData.gamma);
	float4 specSample = specTex.Sample(gsamPoint, vout.TexC);
	float occlusion = specSample.x;
	float roughness = specSample.y;
	float metallic = specSample.z;
	float emmisive = specSample.w;

	float3 worldPos = worldTex.Sample(gsamPoint, vout.TexC).xyz;
	float3 viewDir = normalize(LightData.viewPos - worldPos);
	float3 normal = normalTex.Sample(gsamPoint, vout.TexC).xyz;
	float3 viewReflect = reflect(-viewDir, normal);

	float3 F0 = 0.04f;
	F0 = lerp(F0, albedo, metallic);

	// Reflectance equation
	float3 Lo = 0.0f;

	for (int i = 0; i < LightData.numPointLights; i++) {
		float3 lightVec = LightData.lights[i].pos - worldPos;

		Lo += lightContrib(F0, albedo, roughness, metallic, viewDir, normal, lightVec, LightData.lights[i].pos, LightData.lights[i].color, LightData.lights[i].strength, false)
			 *shadowAmount(vout.TexC, normalize(lightVec), LightData.lights[i].pos, worldPos, normal);
	}
	
	for (int j = LightData.numPointLights; j < LightData.numPointLights + LightData.numDirectionalLights; j++) {
		float3 lightVec = LightData.lights[j].dir;

		Lo += lightContrib(F0, albedo, roughness, metallic, viewDir, normal, lightVec, LightData.viewPos + lightVec * 10000.0f, LightData.lights[j].color * 0.8, 1.0f, true)
			 *shadowAmount(vout.TexC, normalize(lightVec), LightData.viewPos + lightVec * 10000.0f, worldPos, normal);
	}

	float3 ambient = 0.05 * albedo;

	float3 color = ambient + Lo;

	float3 emissive = emissiveTex.Sample(gsamPoint, vout.TexC).xyz;

	color += emissive;

	color = 1.0 - exp(-color * LightData.exposure);

	color = pow(color, 1.0f / LightData.gamma);

	pout.color = float4(color, 1.0f);
	return pout;
}