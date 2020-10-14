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
SamplerState gsamLinear : register(s0);
SamplerState anisoWrap : register(s4);

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
	float3 PosW : TEXCOORD1;
	float3 NormalW : NORMAL;
	float3 TangentW : TANGENT;
	float3 BiNormalW : BINORMAL;
	float2 TexC : TEXCOORD;
	uint shadingRate : SV_ShadingRate;
};

struct PixelOut {
	float4 color : SV_Target0;
	float4 normal : SV_Target1;
	float4 tangent : SV_Target2;
	float4 binormal : SV_Target3;
	float4 world : SV_Target4;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut) 0.0f;
    
	vout.PosW = mul(float4(vin.PosL, 1.0f), world).xyz;
    
	vout.NormalW = mul(vin.NormalL, (float3x3) world);
	vout.TangentW = mul(vin.TangentL, (float3x3) world);
	vout.BiNormalW = mul(vin.BiNormalL, (float3x3) world);
    
	vout.PosH = mul(float4(vout.PosW, 1.0f), ViewProj);
    
	vout.TexC = vin.TexC;
	
	vout.shadingRate = 0xa;
	float4 viewPos = mul(float4(vout.PosW, 1.0), View);
	float dist = dot(viewPos.xyz, viewPos.xyz);
	if (dist < VrsShort * VrsShort) {
		vout.shadingRate = 0x0;	
		return vout;
	}
	if (dist < VrsMedium * VrsMedium) {
		vout.shadingRate = 0x4;	
		return vout;
	}
	if (dist < VrsLong * VrsLong) {
		vout.shadingRate = 0x5;
		return vout;
	}
	#ifdef VRS_4X4
		vout.shadingRate = 0xa;	
		return vout;
	#endif
	vout.shadingRate = 0x0;
	return vout;
}

PixelOut PS(VertexOut pin)
{
	PixelOut p;
	if (renderVRS) {
		switch (pin.shadingRate) {
			case 0x0:
				p.color = float4(1.0,0.0,0.0,1.0);
				break;
			case 0x4:
				p.color = float4(1.0,0.5,0.0,1.0);
				break;
			case 0x5:
				p.color = float4(1.0,1.0,0.0,1.0);
				break;
			default:
				p.color = float4(0.0,1.0,0.0,1.0);
		}
	}
	else {
		p.color = gDiffuseMap.Sample(anisoWrap, pin.TexC);
	}
	p.normal = float4(pin.NormalW, 1.0f);
	p.tangent = float4(pin.TangentW, 0.0);
	p.binormal = float4(pin.BiNormalW, 0.0);
	p.world = float4(pin.PosW, 1.0f);
	return p;
}