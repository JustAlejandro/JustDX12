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

ConstantBuffer<Merge> Merge : register(b0);

Texture2D colorTex : register(t0);
Texture2D specTex : register(t1);
Texture2D normalTex : register(t2);
Texture2D tangentTex : register(t3);
Texture2D biNormalTex : register(t4);
Texture2D worldTex : register(t5);

SamplerState gsamPoint : register(s1);

VertexOutMerge DeferVS(VertexIn vin) {
	VertexOutMerge vout;
	vout.PosH = float4(vin.PosL, 1.0);
	vout.TexC = vin.TexC;
	return vout;
}

PixelOutMerge DeferPS(VertexOutMerge vout) {
	PixelOutMerge pout;
	float3 col = colorTex.Sample(gsamPoint, vout.TexC).xyz;
	float3 diffuse = 0.0f;
	float3 spec = 0.0f;
	float3 worldPos = worldTex.Sample(gsamPoint, vout.TexC).xyz;
	for (int i = 0; i < Merge.numPointLights; i++) {
		float3 lightVec = Merge.lights[i].pos - worldPos;
		float3 lightDir = normalize(lightVec);
		float3 reflectDir = reflect(-lightDir, normalTex.Sample(gsamPoint, vout.TexC).xyz);
		float attenuation = clamp(1.0 - dot(lightVec, lightVec) / (Merge.lights[i].strength * Merge.lights[i].strength), 0.0, 1.0);
		diffuse += clamp(Merge.lights[i].color * attenuation
			* max(dot(normalTex.Sample(gsamPoint, vout.TexC).xyz, lightDir), 0.0), 0.0, 1.0);
		spec += clamp(Merge.lights[i].color * attenuation
			* specTex.Sample(gsamPoint, vout.TexC).x
			* pow(max(dot(reflectDir, normalize(Merge.viewPos - worldPos)), 0.0f), 32.0), 0.0, 1.0);
	}
	diffuse = clamp(diffuse + 0.01, 0.0f, 1.0f);
	spec = 1.0 * clamp(spec, 0.0f, 1.0f);
	float3 totalLight = clamp(diffuse + spec, 0.0f, 1.0f);
	pout.color = float4(totalLight * col, 1.0f);
	return pout;
}