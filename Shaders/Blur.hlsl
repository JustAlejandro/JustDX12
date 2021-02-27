#include "Common.hlsl"
#ifdef HBLUR
Texture2D SSAOTex : register(t0);
RWTexture2D<float> outTex : register(u0);
#endif
#ifdef VBLUR
RWTexture2D<float> hBlurTex : register(u0);
RWTexture2D<float> outTex : register(u1);
#endif
#define N 32

#define blurSize 5

static const float weights[] = { 0.000003f, 0.000229f, 0.005977f, 0.060598f, 0.24173f,
					0.382925f, 0.24173f, 0.060598f, 0.005977f, 0.000229f, 0.000003f };

groupshared float cache[N + blurSize * 2];

static int2 resolution;

#ifdef HBLUR
[numthreads(N, 1, 1)]
void HBlur(int3 groupThreadID : SV_GroupThreadID, int3 dispatchThreadID : SV_DispatchThreadID) {
	SSAOTex.GetDimensions(resolution.x, resolution.y);

	if (groupThreadID.x < blurSize) {
		int2 samplePoint = dispatchThreadID.xy - int2(blurSize, 0);
		cache[groupThreadID.x] = SSAOTex[clampEdges(samplePoint, resolution)].x;
	}
	if (groupThreadID.x - N >= -blurSize) {
		int2 samplePoint = dispatchThreadID.xy + int2(blurSize, 0);
		cache[groupThreadID.x + 2 * blurSize] = SSAOTex[clampEdges(samplePoint, resolution)].x;
	}

	cache[groupThreadID.x + blurSize] = SSAOTex[clampEdges(dispatchThreadID.xy, resolution)].x;

	GroupMemoryBarrierWithGroupSync();

	float res = 0.0f;
	for (int i = -blurSize; i <= blurSize; i++) {
		int idx = groupThreadID.x + blurSize + i;

		res += cache[idx] * weights[i + blurSize];
	}

	outTex[dispatchThreadID.xy] = res;
}
#endif

#ifdef VBLUR
[numthreads(1, N, 1)]
void VBlur(int3 groupThreadID : SV_GroupThreadID, int3 dispatchThreadID : SV_DispatchThreadID) {
	hBlurTex.GetDimensions(resolution.x, resolution.y); 

	if (groupThreadID.y < blurSize) {
		int2 samplePoint = dispatchThreadID.xy - int2(0, blurSize);
		cache[groupThreadID.y] = hBlurTex[clampEdges(samplePoint, resolution)].x;
	}
	if (groupThreadID.y - N >= -blurSize) {
		int2 samplePoint = dispatchThreadID.xy + int2(0, blurSize);
		cache[groupThreadID.y + 2 * blurSize] = hBlurTex[clampEdges(samplePoint, resolution)].x;
	}

	cache[groupThreadID.y + blurSize] = hBlurTex[clampEdges(dispatchThreadID.xy, resolution)].x;

	GroupMemoryBarrierWithGroupSync();

	float res = 0.0f;
	for (int i = -blurSize; i <= blurSize; i++) {
		int idx = groupThreadID.y + blurSize + i;

		res += cache[idx] * weights[i + blurSize];
	}

	outTex[dispatchThreadID.xy] = res;
}
#endif