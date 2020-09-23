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
}

struct VertexIn
{
	float3 PosL : POSITION;
	float3 NormalL : NORMAL;
	float2 TexC : TEXCOORD;
};

struct VertexOut
{
	float4 PosH : SV_Position;
	float3 PosW : POSITION;
	float3 NormalW : NORMAL;
	float2 TexC : TEXCOORD;
};

struct PixelOut {
	float4 color : SV_Target0;
	float4 normal : SV_Target1;
	float4 world : SV_Target2;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut) 0.0f;
    
	vout.PosW = mul(float4(vin.PosL, 1.0f), world).xyz;
    
	vout.NormalW = mul(vin.NormalL, (float3x3) world);
    
	vout.PosH = mul(float4(vout.PosW, 1.0f), ViewProj);
    
	vout.TexC = vin.TexC;
    
	return vout;
}

PixelOut PS(VertexOut pin)
{
	PixelOut p;
	p.color = float4(pin.NormalW, 1.0f);
	p.normal = float4(pin.NormalW, 1.0f);
	p.world = float4(pin.PosW, 1.0f);
	return p;
}