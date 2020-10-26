

struct Light
{
	float3 pos;
	float strength;
	float3 dir;
	int padding;
	float3 color;
	float fov;
};

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

cbuffer cbMerge : register(b0)
{
	float3 viewPos;
	int ipadding;
	int numPointLights;
	int numDirectionalLights;
	int numSpotLights;
	float padding;
	Light lights[MAX_LIGHTS];
};

Texture2D ssaoTex : register(t0);
Texture2D colorTex : register(t1);
Texture2D specTex : register(t2);
Texture2D normalTex : register(t3);
Texture2D tangentTex : register(t4);
Texture2D biNormalTex : register(t5);
Texture2D worldTex : register(t6);

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
	float3 col = colorTex.Sample(gsamPoint, vout.TexC).xyz;
	float3 diffuse = 0.0f;
	float3 spec = 0.0f;
	float3 worldPos = worldTex.Sample(gsamPoint, vout.TexC).xyz;
	for (int i = 0; i < numPointLights; i++) {
		float3 lightDir = lights[i].pos - worldPos;
		float3 reflectDir = reflect(-lightDir, normalTex.Sample(gsamPoint, vout.TexC).xyz);
		float attenuation = clamp(1.0 - dot(lightDir,lightDir) / (lights[i].strength * lights[i].strength), 0.0, 1.0);
		diffuse += lights[i].color * attenuation 
			* max(dot(normalTex.Sample(gsamPoint, vout.TexC).xyz, lightDir), 0.0);	
		spec += lights[i].color * attenuation
			* specTex.Sample(gsamPoint, vout.TexC).xyz
			* pow(max(dot(reflectDir, viewPos - worldPos), 0.0f), 32.0);
	}
	diffuse = clamp(diffuse + 0.2, 0.0f, 1.0f);
	spec = clamp(spec, 0.0f, 1.0f);
	float3 totalLight = clamp(diffuse + spec, 0.0f, 1.0f);
	pout.color = float4(ssaoTex.Sample(gsamPoint, vout.TexC).x * (totalLight * colorTex.Sample(gsamPoint, vout.TexC).xyz), 1.0f);
	return pout;
}