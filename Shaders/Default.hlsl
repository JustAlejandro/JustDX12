#include "Common.hlsl"

ConstantBuffer<PerObject> PerObject : register(b0);

ConstantBuffer<PerPass> PerPass : register(b1);

Texture2D gDiffuseMap : register(t0);
Texture2D gSpecularMap : register(t1);
Texture2D gNormalMap : register(t2);
Texture2D gAlphamap : register(t3);
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

VertexOut VS(VertexIn vin, uint instance : SV_InstanceID)
{
	VertexOut vout = (VertexOut) 0.0f;
    
	vout.PosW = mul(float4(vin.PosL, 1.0f), PerObject.world[instance]).xyz;
    
	vout.NormalW = mul(vin.NormalL, (float3x3) PerObject.world[instance]);
	vout.TangentW = mul(vin.TangentL, (float3x3) PerObject.world[instance]);
	vout.BiNormalW = mul(vin.BiNormalL, (float3x3) PerObject.world[instance]);
    
	vout.PosH = mul(float4(vout.PosW, 1.0f), PerPass.ViewProj);
    
	vout.TexC = vin.TexC;
	
	vout.shadingRate = 0xa;
	float4 viewPos = mul(float4(vout.PosW, 1.0), PerPass.View);
	float dist = dot(viewPos.xyz, viewPos.xyz);
	if (dist < PerPass.VrsShort * PerPass.VrsShort) {
		vout.shadingRate = 0x0;	
		return vout;
	}
	if (dist < PerPass.VrsMedium * PerPass.VrsMedium) {
		vout.shadingRate = 0x4;	
		return vout;
	}
	if (dist < PerPass.VrsLong * PerPass.VrsLong) {
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

	// Only doing transparency in the sense of masking out, not real transparency.
	float transparency = gAlphamap.Sample(anisoWrap, pin.TexC).x;
	if (transparency < 0.3f) {
		discard;
	}

	if (PerPass.renderVRS) {
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
	
	
	float3 inNormal = normalize(gNormalMap.Sample(anisoWrap, pin.TexC).xyz);
	float3x3 TBN = float3x3(pin.TangentW, pin.BiNormalW, pin.NormalW);
	
	p.specular = float4((float3)gSpecularMap.Sample(anisoWrap, pin.TexC).x, 1.0f);
	p.normal = float4(normalize(mul(inNormal,TBN)), 1.0f);
	p.tangent = float4(pin.TangentW, 0.0);
	p.binormal = float4(pin.BiNormalW, 0.0);
	p.world = float4(pin.PosW, 1.0f);
	return p;
}