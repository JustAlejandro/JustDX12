
struct VertexIn
{
	float3 PosL : POSITION;
	float3 NormalL : NORMAL;
	float3 TangentL : TANGENT;
	float3 BiNormalL : BINORMAL;
	float2 TexC : TEXCOORD;
};

struct VertexOut
{
	float4 PosH : SV_Position;
	float2 TexC : TEXCOORD;
};

struct PixelOut {
	float4 color : SV_Target0;
};

Texture2D ssaoTex : register(t0);
Texture2D colorTex : register(t1);
Texture2D normalTex : register(t2);
Texture2D tangentTex : register(t3);
Texture2D biNormalTex : register(t4);
Texture2D worldTex : register(t5);

SamplerState gsamPoint : register(s1);

VertexOut MergeVS(VertexIn vin)
{
	VertexOut vout;
	vout.PosH = float4(vin.PosL,1.0);
	vout.TexC = vin.TexC;
	return vout;
}

PixelOut MergePS(VertexOut vout) 
{
	PixelOut pout;
	pout.color = float4(colorTex.Sample(gsamPoint, vout.TexC).xyz * ssaoTex.Sample(gsamPoint, vout.TexC).x, 1.0f);
	return pout;
}