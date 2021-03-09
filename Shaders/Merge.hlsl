#include "Common.hlsl"

struct VertexOutMerge
{
	float4 PosH : SV_Position;
	float2 TexC : TEXCOORD;
};

struct PixelOutMerge {
	float4 color : SV_Target0;
};

ConstantBuffer<LightData> LightData : register(b0);

Texture2D ssaoTex : register(t0);
Texture2D deferTex : register(t1);

SamplerState gsamPoint : register(s1);

VertexOutMerge MergeVS(VertexIn vin)
{
	VertexOutMerge vout;
	vout.PosH = float4(vin.PosL,1.0);
	vout.TexC = vin.TexC;
	return vout;
}

PixelOutMerge MergePS(VertexOutMerge vout) 
{
	PixelOutMerge pout;
	float3 col = deferTex.Sample(gsamPoint, vout.TexC).xyz;
	float ssaoOut = ssaoTex.Sample(gsamPoint, vout.TexC).x;
	pout.color = float4(col * ssaoOut, 1.0f);
	return pout;
}