#include "Common.hlsl"

struct VertexOutMerge {
	float4 PosH : SV_Position;
	float2 TexC : TEXCOORD;
};

struct PixelOutMerge {
	float4 color : SV_Target0;
};

ConstantBuffer<LightData> LightData : register(b0);
ConstantBuffer<SSAOSettings> SSAOSettings : register(b1);
ConstantBuffer<PerPass> PerPass : register(b2);

Texture2D depthTex : register(t0);
Texture2D colorTex : register(t1);
Texture2D specTex : register(t2);
Texture2D normalTex : register(t3);
Texture2D tangentTex : register(t4);
Texture2D emissiveTex : register(t5);

RaytracingAccelerationStructure TLAS : register(t6);

Buffer<uint3> indexBuffers[] : register(t0,space1);
StructuredBuffer<VertexIn> vertexBuffers[] : register(t0,space2);
// Texture order is diffuse,spec(packed),normal,emissive
Texture2D textures[] : register(t0,space3);

// BRDF Lut for Specular
Texture2D brdfLut : register(t0, space4);

SamplerState gsamLinear : register(s2);

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

float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness) {
	return F0 + (max(1.0f - roughness, F0) - F0) * pow(max(1.0f - cosTheta, 0.0f), 5.0f);
}


// Pixel Shader Stuff

static int2 resolution;

int2 clampEdges(int2 index) {
	return clamp(index, int2(0, 0), resolution - int2(1, 1));
}

float linDepth(float depth) {
	return -SSAOSettings.rangeXNear / (depth - SSAOSettings.range);
}

float2 texCoordFromBary(float2 uvCoord, float2 texCoord0, float2 texCoord1, float2 texCoord2) {
	return texCoord0 + uvCoord.x * (texCoord1 - texCoord0) + uvCoord.y * (texCoord2 - texCoord0);
}

float3 normalFromBary(float2 uvCoord, float3 normal0, float3 normal1, float3 normal2) {
	return normal0 + uvCoord.x * (normal1 - normal0) + uvCoord.y * (normal2 - normal0);
}

float2 getUvCoord(uint instanceID, uint3 primitive, float2 uvCoord) {
	float2 texCoord0 = vertexBuffers[instanceID].Load(primitive.x).TexC;
	float2 texCoord1 = vertexBuffers[instanceID].Load(primitive.y).TexC;
	float2 texCoord2 = vertexBuffers[instanceID].Load(primitive.z).TexC;
	return texCoordFromBary(uvCoord, texCoord0, texCoord1, texCoord2);
}

float3 getNormal(uint instanceID, uint3 primitive, float2 uvCoord) {
	float3 normal0 = vertexBuffers[instanceID].Load(primitive.x).NormalL;
	float3 normal1 = vertexBuffers[instanceID].Load(primitive.y).NormalL;
	float3 normal2 = vertexBuffers[instanceID].Load(primitive.z).NormalL;
	return normalFromBary(uvCoord, normal0, normal1, normal2);
}

void proceedQueryUntilHitCommited(RayQuery<RAY_FLAG_NONE> query) {
	while (query.Proceed()) {
		uint instanceID = query.CandidateInstanceID() + query.CandidateGeometryIndex();
		float2 uvCoord = query.CandidateTriangleBarycentrics();
		uint3 primitive = uint3(indexBuffers[instanceID].Load(query.CandidatePrimitiveIndex()));
		float2 texCoord = getUvCoord(instanceID, primitive, uvCoord);
		if (textures[instanceID * 4 + 0].Sample(gsamLinear, texCoord).w > 0.3f) {
			query.CommitNonOpaqueTriangleHit();
		}
	}
}

float shadowAmount(float3 lightDir, float3 lightPos, float3 worldPos, float3 normal) {
	float unoccluded = 1.0;
	if (SSAOSettings.showSSShadows) {
		worldPos += normal / 100.0f;
		for (int j = 0; j < SSAOSettings.shadowSteps; j++) {
			worldPos.xyz += lightDir * SSAOSettings.shadowStepSize;
			float4 result = mul(float4(worldPos, 1.0f), SSAOSettings.ViewProj);
			result /= result.w;
			result.xy = result.xy * 0.5 + 0.5;
			result.y = result.y * -1.0 + 1.0;
			result.z = linDepth(result.z);
			float compareDepth = linDepth(depthTex[clampEdges((int2) (result.xy * resolution))].x);
			if (compareDepth < result.z && result.z - compareDepth < 1000.0) {
				unoccluded -= (1.0f / SSAOSettings.shadowSteps);
			}
		}
	}
	else {
		RayQuery<RAY_FLAG_NONE> query;

		uint ray_flags = 0; // Any this ray requires in addition those above.
		uint ray_instance_mask = 0xffffffff;

		RayDesc ray;
		ray.TMin = 5.0f;
		ray.TMax = distance(lightPos, worldPos);
		ray.Origin = worldPos;
		ray.Direction = lightDir;
		query.TraceRayInline(TLAS, ray_flags, ray_instance_mask, ray);

		proceedQueryUntilHitCommited(query);
		
		if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
			unoccluded = 0.0f;
		}
	}
	return (float) unoccluded;
}

struct LightingData {
	bool hit;
	float3 F0;
	float3 albedo;
	float roughness;
	float3 viewDir;
	float metallic;
	float3 normal;
	int lightType;
	float3 worldPos;
	Light light;
};

LightingData reflectionContrib(float3 F0, float3 albedo, float roughness, float metallic, float3 viewDir, float3 normal, float3 worldPos) {
	LightingData reflection;
	reflection.hit = false;
	reflection.albedo = 0.0f;

	RayQuery<RAY_FLAG_NONE> query;

	uint ray_flags = 0; // Any this ray requires in addition those above.
	uint ray_instance_mask = 0xffffffff;

	RayDesc ray;
	ray.TMin = 5.0f;
	ray.TMax = 1e5f;
	ray.Origin = worldPos;
	ray.Direction = reflect(-viewDir, normal);
	query.TraceRayInline(TLAS, ray_flags, ray_instance_mask, ray);

	proceedQueryUntilHitCommited(query);

	if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
		uint instanceID = query.CommittedInstanceID() + query.CommittedGeometryIndex();
		uint3 primitive = uint3(indexBuffers[instanceID].Load(query.CommittedPrimitiveIndex()));
		float2 uvCoord = query.CommittedTriangleBarycentrics();
		float2 texCoord = getUvCoord(instanceID, primitive, uvCoord);
		// Now do the lighting from this reflection
		reflection.hit = true;
		reflection.albedo = textures[instanceID * 4 + 0].Sample(gsamLinear, texCoord).xyz;
		reflection.albedo = pow(reflection.albedo, LightData.gamma);
		float4 specSample = textures[instanceID * 4 + 1].Sample(gsamLinear, texCoord);
		reflection.roughness = specSample.y;
		reflection.metallic = specSample.z;
		reflection.viewDir = reflect(-viewDir, normal);
		reflection.normal = normalize(mul(getNormal(instanceID, primitive, uvCoord), (float3x3)query.CommittedObjectToWorld3x4()));
		reflection.worldPos = worldPos + query.CommittedRayT() * reflection.viewDir;
		float3 hitF0 = 0.04f;
		reflection.F0 = lerp(hitF0, reflection.albedo, reflection.metallic);
	}
	return reflection;
}

struct PbrCalcData {
	float3 specular;
	float3 kD;
	float NdotL;
	float3 kS;
	float3 radiance;
};

PbrCalcData pbrHelper(float3 F0, float metallic, float roughness, float3 lightDir, float3 lightVec, float3 lightColor, float3 viewDir, float3 normal, float attenuationVal, bool attenuationConst) {
	PbrCalcData data;
	
	float3 H = normalize(viewDir + lightDir);
	float3 reflectDir = reflect(-lightDir, normal);
	float attenuation = attenuationConst ? attenuationVal : clamp(1.0 - dot(lightVec, lightVec) / (attenuationVal * attenuationVal), 0.0, 1.0);

	data.radiance = lightColor * attenuation;

	float NDF = DistributionGGX(normal, H, roughness);
	float G = GeometrySmith(normal, viewDir, lightDir, roughness);
	float3 F = fresnelSchlick(max(dot(H, viewDir), 0.0f), F0);

	float3 nominator = NDF * G * F;
	float denominator = 4 * max(dot(normal, viewDir), 0.0f) * max(dot(normal, lightDir), 0.0f) + 0.001f;
	data.specular = nominator / denominator;

	data.kS = F;

	data.kD = 1.0 - data.kS;

	data.kD *= (1.0 - metallic);

	data.NdotL = max(dot(normal, lightDir), 0.0f);
	
	return data;
}

float3 lightContrib(float3 F0, float3 albedo, float roughness, float metallic, float3 viewDir, float3 normal, float3 worldPos, float3 lightVec, float3 lightPos, float3 lightColor, float attenuationVal, bool attenuationConst, uint bounceCount) {
	float3 lightDir = normalize(lightVec);
	
	PbrCalcData pbrData = pbrHelper(F0, metallic, roughness, lightDir, lightVec, lightColor, viewDir, normal, attenuationVal, attenuationConst);
	
	float shadowMul = shadowAmount(lightDir, lightPos, worldPos, normal);

	return (pbrData.kD * albedo / 3.14159 + pbrData.specular) * pbrData.radiance * pbrData.NdotL * shadowMul;
}

PixelOutMerge DeferPS(VertexOutMerge vout) {
	PixelOutMerge pout;

	float depth = depthTex.Sample(gsamLinear, vout.TexC).x;
	if (depth == 1.0f) {
		pout.color = float4(0.4f, 0.4f, 1.0f, 1.0f);
		return pout;
	}

	depthTex.GetDimensions(resolution.x, resolution.y);

	float3 albedo = colorTex.Sample(gsamLinear, vout.TexC).xyz;
	albedo = pow(albedo, LightData.gamma);

	float4 specSample = specTex.Sample(gsamLinear, vout.TexC);
	float occlusion = specSample.x;
	float roughness = specSample.y;
	float metallic = specSample.z;

	float3 worldPos = positionFromDepthVal(depth, vout.TexC, PerPass);

	float3 viewDir = normalize(LightData.viewPos - worldPos);
	float3 normal = normalTex.Sample(gsamLinear, vout.TexC).xyz;
	float3 viewReflect = reflect(-viewDir, normal);

	float3 F0 = 0.04f;
	F0 = lerp(F0, albedo, metallic);

	// Reflectance equation
	float3 Lo = 0.0f;

	for (int i = 0; i < LightData.numPointLights; i++) {
		float3 lightVec = LightData.lights[i].pos - worldPos;

		Lo += lightContrib(F0, albedo, roughness, metallic, viewDir, normal, worldPos, lightVec, LightData.lights[i].pos, LightData.lights[i].color, LightData.lights[i].strength, false, 0);
	}
	
	for (int j = LightData.numPointLights; j < LightData.numPointLights + LightData.numDirectionalLights; j++) {
		float3 lightVec = -LightData.lights[j].dir;

		Lo += lightContrib(F0, albedo, roughness, metallic, viewDir, normal, worldPos, lightVec, LightData.viewPos + lightVec * 10000.0f, LightData.lights[j].color, 1.0f, true, 0);
	}
	
	// IBL (For now, just RT Reflections)
	float3 reflection = 0.0f;
	if (metallic > 0.1f && roughness < 0.3f)
	{
		LightingData reflectData;
		reflectData = reflectionContrib(F0, albedo, roughness, metallic, viewDir, normal, worldPos);
		if (reflectData.hit)
		{
			float3 refL0 = 0.0f;
			for (int i = 0; i < LightData.numPointLights; i++)
			{
				float3 lightVec = LightData.lights[i].pos - worldPos;

				refL0 += lightContrib(reflectData.F0, reflectData.albedo, reflectData.roughness, reflectData.metallic, reflectData.viewDir, reflectData.normal, reflectData.worldPos, lightVec, LightData.lights[i].pos, LightData.lights[i].color, LightData.lights[i].strength, false, 0);
			}
	
			for (int j = LightData.numPointLights; j < LightData.numPointLights + LightData.numDirectionalLights; j++)
			{
				float3 lightVec = -LightData.lights[j].dir;

				refL0 += lightContrib(reflectData.F0, reflectData.albedo, reflectData.roughness, reflectData.metallic, reflectData.viewDir, reflectData.normal, reflectData.worldPos, lightVec, LightData.viewPos + lightVec * 10000.0f, LightData.lights[j].color, 1.0f, true, 0);
			}
			
			float2 brdf = brdfLut.Sample(gsamLinear, float2(dot(normal, viewDir), roughness)).xy;
			
			float3 F = fresnelSchlickRoughness(max(dot(normal, viewDir), 0.0f), F0, roughness);
			
			reflection += refL0 * (F * brdf.x + brdf.y);
			
			float3 ambient = 0.05 * reflectData.albedo;
			
			reflection += refL0 + ambient;
		}
	}

	float3 ambient = 0.05 * albedo;

	float3 color = ambient + Lo;

	float3 emissive = emissiveTex.Sample(gsamLinear, vout.TexC).xyz;

	color += emissive;
	
	color += reflection;

	color = 1.0 - exp(-color * LightData.exposure);

	color = pow(color, 1.0f / LightData.gamma);

	pout.color = float4(color, 1.0f);
	return pout;
}