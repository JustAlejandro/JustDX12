#include "Common.hlsl"

ConstantBuffer<PerObject> PerObjectBuffer : register(b0);
ConstantBuffer<PerObject> PerMeshBuffer : register(b1);

ConstantBuffer<PerPass> PerPass : register(b2);

Texture2D gDiffuseMap : register(t0);
Texture2D gSpecularMap : register(t1);
Texture2D gNormalMap : register(t2);
Texture2D gEmissiveMap : register(t3);
SamplerState gsamLinear : register(s0);
SamplerState anisoWrap : register(s4);

VertexOut VS(VertexIn vin, uint instance : SV_InstanceID)
{
	VertexOut vout = (VertexOut) 0.0f;

	uint meshID = instance % PerMeshBuffer.instanceCount;
	uint objID = instance / PerMeshBuffer.instanceCount;

	float4x4 toWorld = mul(PerMeshBuffer.world[meshID], PerObjectBuffer.world[objID]);
    
	vout.PosW = mul(float4(vin.PosL, 1.0f), toWorld).xyz;
    
	vout.NormalW = normalize(mul(vin.NormalL, (float3x3) toWorld));
	vout.TangentW = normalize(mul(vin.TangentL, (float3x3) toWorld));
	vout.BiNormalW = normalize(mul(vin.BiNormalL, (float3x3) toWorld));
    
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

	p.color = gDiffuseMap.Sample(anisoWrap, pin.TexC);

	if (p.color.a < 0.3f) {
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
	
	
	float2 texNormal = (gNormalMap.Sample(anisoWrap, pin.TexC).xy);
	texNormal = texNormal * 2.0f - 1.0f;
	float3 inNormal = (float3(texNormal.xy, 1.0f - dot(texNormal, texNormal)));
	float3x3 TBN = float3x3(pin.TangentW, pin.BiNormalW, pin.NormalW);
	
	p.specular = float4(gSpecularMap.Sample(anisoWrap, pin.TexC).xyz, 1.0f);
	p.normal = float4(normalize(mul(inNormal,TBN)), 0.0f);
	p.tangent = float4(pin.TangentW, 0.0);
	p.emissive = gEmissiveMap.Sample(anisoWrap, pin.TexC);
	return p;
}