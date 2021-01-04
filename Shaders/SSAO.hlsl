// Basic SSAO Implementation

// TAA Flag is included because ray sampling
// should be random if we denoise and constant
// if we don't, to avoid shimmering

cbuffer cbSSAOSettings : register(b0)
{
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

Texture2D noiseTex : register(t0);
Texture2D depthTex : register(t1);
Texture2D colorTex : register(t2);
Texture2D normalTex : register(t3);
Texture2D tangentTex : register(t4);
Texture2D biNormalTex : register(t5);
Texture2D worldTex : register(t6);

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
	return - rangeXNear / (depth - range);
}

[numthreads(N,N,1)]
void SSAO(int3 groupThreadID : SV_GroupThreadID, int3 dispatchThreadID : SV_DispatchThreadID)
{
	depthTex.GetDimensions(resolution.x, resolution.y);
	noiseTex.GetDimensions(noiseTexResolution.x, noiseTexResolution.y);
	
	float3 inNormal = normalize(normalTex[dispatchThreadID.xy].xyz);
	float3 inTan = normalize(tangentTex[dispatchThreadID.xy].xyz);
	float3 inBinormal = normalize(biNormalTex[dispatchThreadID.xy].xyz);
	float3x3 TBN = float3x3(inTan, inBinormal, inNormal);
	
	float4 worldPos = worldTex[dispatchThreadID.xy] + float4(inNormal, 0.0);
	
	float outCol = 0.0f;
	
	float perSample = 1.0 / rayCount;
	
	for (int i = 0; i < rayCount; i++)
	{
		float3 sample = mul(noiseTex[wrapNoiseEdges(int2(dispatchThreadID.x * rayCount + i,dispatchThreadID.y * rayCount + i))].xyz, TBN);
		sample = worldPos.xyz + sample * rayLength;
		
		float4 result = mul(float4(sample, 1.0f), ViewProj);
		result /= result.w;
		result.xy = result.xy * 0.5 + 0.5;
		result.y = result.y * -1.0 + 1.0;
		result.z = linDepth(result.z);
		float depthSample = linDepth(depthTex[clampEdges((int2) (result.xy * resolution))].x);
		if (depthSample >= result.z || result.z - depthSample > 30.0)
		{
			outCol += perSample;
		}
	}
	outCol = !showSSAO + (outCol * (float)showSSAO);
	float3 lightDir = normalize(lightPos.xyz - worldPos.xyz);
	float resColor = 1.0;
	int occludeCount = 20;
	for (int j = 0; j < shadowSteps; j++)
	{
		worldPos.xyz += lightDir * shadowStepSize;
		float4 result = mul(worldPos, ViewProj);
		result /= result.w;
		result.xy = result.xy * 0.5 + 0.5;
		result.y = result.y * -1.0 + 1.0;
		result.z = linDepth(result.z);
		float compareDepth = linDepth(depthTex[clampEdges((int2) (result.xy * resolution))].x);
		if (compareDepth < result.z && result.z - compareDepth < 60.0)
		{
			occludeCount--;
		}
	}
	outCol *= (!showSSShadows + ((occludeCount / 20.0f) * (float)showSSShadows));
	outTex[dispatchThreadID.xy] = outCol;
}