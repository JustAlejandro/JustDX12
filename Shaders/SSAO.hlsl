// Basic SSAO Implementation

// TAA Flag is included because ray sampling
// should be random if we denoise and constant
// if we don't, to avoid shimmering

cbuffer cbSSAOSettings : register(b0)
{
	float4x4 ViewProj;
	int rayCount;
	int TAA;
	float range;
	float rangeXNear;
	float4 lightPos;
	float4 rand[48];
};

Texture2D depthTex : register(t0);
Texture2D colorTex : register(t1);
Texture2D normalTex : register(t2);
Texture2D tangentTex : register(t3);
Texture2D biNormalTex : register(t4);
Texture2D worldTex : register(t5);

RWTexture2D<float> outTex : register(u0);

#define N 8

static int2 resolution;

int2 clampEdges(int2 index)
{
	return clamp(index, int2(0, 0), resolution - int2(1, 1));
}

float linDepth(float depth)
{
	return - rangeXNear / (depth - range);
}

[numthreads(N,N,1)]
void SSAO(int3 groupThreadID : SV_GroupThreadID, int3 dispatchThreadID : SV_DispatchThreadID)
{
	depthTex.GetDimensions(resolution.x, resolution.y);
	
	float3 inNormal = normalize(normalTex[dispatchThreadID.xy].xyz);
	float3 inTan = normalize(tangentTex[dispatchThreadID.xy].xyz);
	float3 inBinormal = normalize(biNormalTex[dispatchThreadID.xy].xyz);
	float3x3 TBN = float3x3(inTan, inBinormal, inNormal);
	
	float4 worldPos = worldTex[dispatchThreadID.xy] + float4(inNormal, 0.0);
	
	float outCol = 0.0f;
	
	float perSample = 1.0 / rayCount;
	
	for (int i = 0; i < rayCount; i++)
	{
		float3 sample = mul(normalize(rand[i].xyz), TBN);
		sample = worldPos.xyz + sample * rand[i].w * 5.0;
		
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
	
	float3 lightDir = normalize(lightPos.xyz - worldPos.xyz);
	float resColor = 1.0;
	int occludeCount = 20;
	for (int j = 0; j < 20; j++)
	{
		worldPos.xyz += lightDir * 0.1;
		float4 result = mul(worldPos, ViewProj);
		result /= result.w;
		result.xy = result.xy * 0.5 + 0.5;
		result.y = result.y * -1.0 + 1.0;
		if (depthTex[clampEdges((int2) (result.xy * resolution))].x < result.z)
		{
			occludeCount--;
		}
	}
	outCol *= (occludeCount / 20.0);
	outTex[dispatchThreadID.xy] = outCol;
}

/*

groupshared float cache[N + N][N + N];

float sampleDepth(int2 index, int2 rootIdx)
{
	int2 cacheIndex = index - rootIdx;
	[branch]
	if (any(cacheIndex < 0) || any(cacheIndex >= 2 * N))
	{
		return depthTex[clampEdges(index)].x;
	}
	else
	{
		return cache[cacheIndex.x][cacheIndex.y];
	}
}

// About half the speed of direct sampling... Maybe worth investigating. Possibly from sampling 4x per thead.
[numthreads(N, N, 1)]
void SSAOGroupShared(int3 groupThreadID : SV_GroupThreadID, int3 dispatchThreadID : SV_DispatchThreadID)
{
	depthTex.GetDimensions(resolution.x, resolution.y);
	
	
	int2 rootIdx = dispatchThreadID.xy - groupThreadID.xy - int2(N / 2, N / 2);
	cache[groupThreadID.x * 2][groupThreadID.y * 2] = depthTex[clampEdges(rootIdx + int2(groupThreadID.x * 2, groupThreadID.y * 2))].x;
	cache[groupThreadID.x * 2][groupThreadID.y * 2 + 1] = depthTex[clampEdges(rootIdx + int2(groupThreadID.x * 2, groupThreadID.y * 2 + 1))].x;
	cache[groupThreadID.x * 2 + 1][groupThreadID.y * 2] = depthTex[clampEdges(rootIdx + int2(groupThreadID.x * 2 + 1, groupThreadID.y * 2))].x;
	cache[groupThreadID.x * 2 + 1][groupThreadID.y * 2 + 1] = depthTex[clampEdges(rootIdx + int2(groupThreadID.x * 2 + 1, groupThreadID.y * 2 + 1))].x;
	
	GroupMemoryBarrierWithGroupSync();
	
	int2 baseIndex = groupThreadID.xy + int2(N / 2, N / 2);
	
	float3 inNormal = normalize(normalTex[dispatchThreadID.xy].xyz);
	float3 inTan = normalize(tangentTex[dispatchThreadID.xy].xyz);
	float3 inBinormal = normalize(biNormalTex[dispatchThreadID.xy].xyz);
	float3x3 TBN = float3x3(inTan, inBinormal, inNormal);
	
	float depth = cache[baseIndex.x][baseIndex.y];
	float4 worldPos = worldTex[dispatchThreadID.xy] + float4(inNormal, 0.0);
	
	float outCol = 0.0f;
	
	for (int i = 0; i < 48; i++)
	{
		float3 sample = mul(normalize(rand[i].xyz), TBN);
		sample = worldPos.xyz + sample * rand[i].w * 1.0;
		
		float4 result = mul(float4(sample, 1.0f), ViewProj);
		result /= result.w;
		result.xy = result.xy * 0.5 + 0.5;
		result.y = result.y * -1.0 + 1.0;
		
		if (sampleDepth(int2(result.xy * resolution), rootIdx) >= result.z)
		{
			outCol += 1.0 / 48.0;
		}
	}
	
	float3 lightDir = normalize(lightPos.xyz - worldPos.xyz);
	float resColor = 1.0;
	int occludeCount = 10;
	for (int j = 0; j < 10; j++)
	{
		worldPos.xyz += lightDir * 0.5;
		float4 result = mul(worldPos, ViewProj);
		result /= result.w;
		result.xy = result.xy * 0.5 + 0.5;
		result.y = result.y * -1.0 + 1.0;
		if (sampleDepth((int2) (result.xy * resolution), rootIdx) < result.z)
		{
			occludeCount--;
		}
	}
	outCol *= (occludeCount / 10.0);
	outTex[dispatchThreadID.xy] = float4(outCol, outCol, outCol, 1.0f);
}

// This is terrible, just putting it here for kicks.
[numthreads(N, N, 1)]
void SSAOBrute(int3 groupThreadID : SV_GroupThreadID, int3 dispatchThreadID : SV_DispatchThreadID)
{
	depthTex.GetDimensions(resolution.x, resolution.y);
	float baseDepth = depthTex[dispatchThreadID.xy].x;
	float outCol = 0.0f;
	for (int i = -4; i < 4; i++)
	{
		for (int j = -4; j < 4; j++)
		{
			float diff = depthTex[clamp(dispatchThreadID.xy + int2(i, j) * 4, int2(0, 0), resolution)].x - baseDepth;
			if (diff > 0)
			{
				outCol += 1.0 / 32.0;
			}
		}
	}
	outTex[dispatchThreadID.xy] = float4(outCol, outCol, outCol, 1.0f);
}
*/