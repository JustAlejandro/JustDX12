// Basic SSAO Implementation
#include "Common.hlsl"


ConstantBuffer<LightData> LightData : register(b0);
ConstantBuffer<SSAOSettings> SSAOSettings : register(b1);
ConstantBuffer<PerPass> PerPass : register(b2);

Texture2D noiseTex : register(t0);
Texture2D depthTex : register(t1);
Texture2D normalTex : register(t2);
Texture2D tangentTex : register(t3);

RWTexture2D<float> outTex : register(u0);

#define N 8

static int2 resolution;
static int2 noiseTexResolution;

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
	float depth = depthTex[dispatchThreadID.xy].x;
	float4 worldPos = float4(positionFromDepthVal(depth, float2(dispatchThreadID.xy) / resolution, PerPass), 1.0f);
	
	float outCol = 0.0f;
	
	float perSample = 1.0 / SSAOSettings.rayCount;
	
	for (int i = 0; i < SSAOSettings.rayCount; i++)
	{
		float3 randVec = noiseTex[wrapNoiseEdges(int2(dispatchThreadID.x * SSAOSettings.rayCount + i, dispatchThreadID.y * SSAOSettings.rayCount + i))].xyz;
		randVec.xy = (randVec.xy - 0.5f * 2.0f);
		randVec.z += 0.3f;
		randVec = mul(normalize(randVec), TBN);
		float3 samplePoint = worldPos.xyz + randVec * SSAOSettings.rayLength;
		
		float4 result = mul(float4(samplePoint, 1.0f), SSAOSettings.ViewProj);
		result /= result.w;
		result.xy = result.xy * 0.5 + 0.5;
		result.y = result.y * -1.0 + 1.0;
		result.z = linDepth(result.z);
		float depthSample = linDepth(depthTex[clampEdges((int2) (result.xy * resolution), resolution)].x);
		if (depthSample >= result.z || result.z - depthSample > 0.1)
		{
			outCol += perSample;
		}
	}
	outCol = !SSAOSettings.showSSAO + (outCol * (float)SSAOSettings.showSSAO);

	outTex[dispatchThreadID.xy] = outCol;
}