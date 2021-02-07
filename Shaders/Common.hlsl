#define GROUP_SIZE 32

float2 flipYofUV(float2 uv) {
	float2 flipped = uv - float2(0.0f,0.5f);
	flipped.y *= -1;
	flipped.y += 0.5f;
	return flipped;
}

struct PerObject
{
	float4x4 world[16];
};

struct PerPass
{
	float4x4 View;
	float4x4 InvView;
	float4x4 Proj;
	float4x4 InvProj;
	float4x4 ViewProj;
	float4x4 InvViewProj;
	float3 EyePosW;
	int meshletCull;
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
};

struct SSAOSettings {
	bool showSSAO;
	bool showSSShadows;
	int2 padding;
	float4x4 ViewProj;
	int rayCount;
	float rayLength;
	int TAA;
	float range;
	float rangeXNear;
	int shadowSteps;
	float shadowStepSize;
	float4 lightPos;
};

struct VertexOut
{
	float4 PosH : SV_Position;
	float3 PosW : TEXCOORD1;
	float3 NormalW : NORMAL;
	float3 TangentW : TANGENT;
	float3 BiNormalW : BINORMAL;
	float2 TexC : TEXCOORD;
#ifdef VRS
	uint shadingRate : SV_ShadingRate;
#endif
};

struct PixelOut {
	float4 color : SV_Target0;
	float4 specular : SV_Target1;
	float4 normal : SV_Target2;
	float4 tangent : SV_Target3;
	float4 world : SV_Target4;
};


#ifdef MAX_LIGHTS
struct Light
{
	float3 pos;
	float strength;
	float3 dir;
	int padding;
	float3 color;
	float fov;
};

struct LightData
{
	float3 viewPos;
	float exposure;
	uint numPointLights;
	uint numDirectionalLights;
	uint numSpotLights;
	float gamma;
	Light lights[MAX_LIGHTS];
};
#endif