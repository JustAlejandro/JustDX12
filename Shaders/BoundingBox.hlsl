cbuffer cbPerObject : register(b0)
{
	float4x4 world;
}

cbuffer cbPass : register(b1)
{
	float4x4 View;
	float4x4 InvView;
	float4x4 Proj;
	float4x4 InvProj;
	float4x4 ViewProj;
	float4x4 InvViewProj;
	float3 EyePosW;
	float cbPerObjectPad;
	float2 RenderTargetSize;
	float2 InvRenderTargetSize;
	float NearZ;
	float FarZ;
	float TotalTime;
	float DeltaTime;
	float VrsShort;
	float VrsMedium;
	float VrsLong;
	int renderVRS;
}

Texture2D gDiffuseMap : register(t0);
Texture2D gSpecularMap : register(t1);
Texture2D gNormalMap : register(t2);
SamplerState gsamLinear : register(s0);
SamplerState anisoWrap : register(s4);

struct VBoundingBoxIn
{
	float3 center : POSITION;
	float3 extents : NORMAL;
	float3 padding0 : TANGENT;
	float3 padding1 : BINORMAL;
	float2 padding2 : TEXCOORD;
};

struct VBoundingBoxOut 
{
	float3 center : POSITION;
	float3 extents : NORMAL;
};

struct GBoundingBoxOut
{
	float4 pos : SV_Position;
};

struct PBoundingBoxOut 
{
	float4 col : SV_Target0;	
};

VBoundingBoxOut VS(VBoundingBoxIn vin) 
{
	VBoundingBoxOut vout;
	vout.center = mul(float4(vin.center, 1.0f), world).xyz;
	vout.extents = mul(float4(vin.extents, 0.0f), world).xyz;
	return vout;
}

[maxvertexcount(14)]
void GS(point VBoundingBoxOut gin[1], inout TriangleStream<GBoundingBoxOut> triStream) 
{
	float3 verts[14];
	float3 center = gin[0].center;
	float3 e = gin[0].extents;
	
	verts[0] = center + float3(-e.x, e.y, e.z);
	verts[1] = center + float3(e.x, e.y, e.z);
	verts[2] = center + float3(-e.x, -e.y, e.z);
	verts[3] = center + float3(e.x, -e.y, e.z);
	verts[4] = center + float3(e.x, -e.y, -e.z);
	verts[5] = center + float3(e.x, e.y, e.z);
	verts[6] = center + float3(e.x, e.y, -e.z);
	verts[7] = center + float3(-e.x, e.y, e.z);
	verts[8] = center + float3(-e.x, e.y, -e.z);
	verts[9] = center + float3(-e.x, -e.y, e.z);
	verts[10] = center + float3(-e.x, -e.y, -e.z);
	verts[11] = center + float3(e.x, -e.y, -e.z);
	verts[12] = center + float3(-e.x, e.y, -e.z);
	verts[13] = center + float3(e.x, e.y, -e.z);
	
	GBoundingBoxOut gout;
	[unroll]
	for (int i = 0; i < 14; i++) 
	{
		gout.pos = mul(float4(verts[i],1.0f), ViewProj);
		triStream.Append(gout);
	}
}

PBoundingBoxOut PS(GBoundingBoxOut pin)
{
	PBoundingBoxOut pout;
	pout.col = float4(1.0f,1,1,1);
	return pout;
}
