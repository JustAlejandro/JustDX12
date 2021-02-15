// Basic SSAO Implementation

// TAA Flag is included because ray sampling
// should be random if we denoise and constant
// if we don't, to avoid shimmering
#include "Common.hlsl"


ConstantBuffer<LightData> LightData : register(b0);
ConstantBuffer<SSAOSettings> SSAOSettings : register(b1);

Texture2D noiseTex : register(t0);
Texture2D depthTex : register(t1);
Texture2D normalTex : register(t2);
Texture2D tangentTex : register(t3);
Texture2D worldTex : register(t4);

RWTexture2D<float> outTex : register(u0);

#define N 8

static int2 resolution;
static int2 noiseTexResolution;

int2 clampEdges(int2 index)
{
	return clamp(index, int2(0, 0), resolution - int2(1, 1));
}

int2 wrapNoiseEdges(int2 index) {
	return index % noiseTexResolution;
}

float linDepth(float depth)
{
	return -SSAOSettings.rangeXNear / (depth - SSAOSettings.range);
}

[numthreads(N,N,1)]
void SSAO(int3 groupThreadID : SV_GroupThreadID, int3 dispatchThreadID : SV_DispatchThreadID)
{
	depthTex.GetDimensions(resolution.x, resolution.y);
	noiseTex.GetDimensions(noiseTexResolution.x, noiseTexResolution.y);
	
	float3 inNormal = normalize(normalTex[dispatchThreadID.xy].xyz);
	float3 inTan = normalize(tangentTex[dispatchThreadID.xy].xyz);
	float3 inBinormal = normalize(cross(inNormal, inTan));
	float3x3 TBN = float3x3(inTan, inBinormal, inNormal);
	
	float4 worldPos = worldTex[dispatchThreadID.xy] + float4(inNormal, 0.0);
	
	float outCol = 0.0f;
	
	float perSample = 1.0 / SSAOSettings.rayCount;
	
	for (int i = 0; i < SSAOSettings.rayCount; i++)
	{
		float3 sample = mul(noiseTex[wrapNoiseEdges(int2(dispatchThreadID.x * SSAOSettings.rayCount + i,dispatchThreadID.y * SSAOSettings.rayCount + i))].xyz, TBN);
		sample = worldPos.xyz + sample * SSAOSettings.rayLength;
		
		float4 result = mul(float4(sample, 1.0f), SSAOSettings.ViewProj);
		result /= result.w;
		result.xy = result.xy * 0.5 + 0.5;
		result.y = result.y * -1.0 + 1.0;
		result.z = linDepth(result.z);
		float depthSample = linDepth(depthTex[clampEdges((int2) (result.xy * resolution))].x);
		if (depthSample >= result.z || result.z - depthSample > 0.2)
		{
			outCol += perSample;
		}
	}
	outCol = !SSAOSettings.showSSAO + (outCol * (float)SSAOSettings.showSSAO);

	outTex[dispatchThreadID.xy] = outCol;
}