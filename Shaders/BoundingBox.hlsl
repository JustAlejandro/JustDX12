#include "Common.hlsl"

ConstantBuffer<PerObject> PerObject : register(b0);

ConstantBuffer<PerPass> PerPass : register(b1);

struct VBoundingBoxIn
{
	float3 center : POSITION;
	float3 extents : NORMAL;
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

VBoundingBoxOut VS(VBoundingBoxIn vin, uint instance : SV_InstanceID)
{
	VBoundingBoxOut vout;
	vout.center = mul(float4(vin.center, 1.0f), PerObject.world[instance]).xyz;
	vout.extents = mul(float4(vin.extents, 0.0f), PerObject.world[instance]).xyz;
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
		gout.pos = mul(float4(verts[i],1.0f), PerPass.ViewProj);
		triStream.Append(gout);
	}
}

PBoundingBoxOut PS(GBoundingBoxOut pin)
{
	PBoundingBoxOut pout;
	pout.col = float4(1.0f,1,1,1);
	return pout;
}
