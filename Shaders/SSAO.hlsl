// Basic SSAO Implementation

// TAA Flag is included because ray sampling
// should be random if we denoise and constant
// if we don't, to avoid shimmering
/*
cbuffer cbSSAOSettings : register(b0)
{
    int rayCount;
    float maxRange;
    float minRange;
    int TAA;
};*/

Texture2D depthTex : register(t0);
RWTexture2D<float4> outTex : register(u0);

#define N 256

[numthreads(N,1,1)]
void SSAO(int3 groupThreadID : SV_GroupThreadID, int3 dispatchThreadID : SV_DispatchThreadID)
{
	float4 outCol = depthTex[int2(dispatchThreadID.x, dispatchThreadID.y)];
	outCol.xyz = outCol.xyz * float3(0.5f, 0.5f, 0.5f);
	outTex[dispatchThreadID.xy] = outCol;
}