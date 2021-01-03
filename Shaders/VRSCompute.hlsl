struct Vrs {
	int vrsAvgLum;
	int vrsVarLum;
	float vrsAvgCut;
	float vrsVarianceCut;
	int vrsVarianceVotes;
}; 

Texture2D colorTex : register(t0);

RWTexture2D<uint> vrsOut : register (u0);

ConstantBuffer<Vrs> Vrs : register (b0);

groupshared float luminanceSquare[TILE_SIZE * TILE_SIZE];

float luminance(float3 color) {
		return 0.2126 * color.x + 0.7152 * color.y + 0.0722 * color.z;
}

uint2 tileClamp(uint2 texCoord) {
	return clamp(texCoord, uint2(0, 0), uint2(TILE_SIZE, TILE_SIZE));
}

[numthreads(N,1,1)]
void VRSOut(int3 groupID : SV_GroupID, int3 groupThreadID : SV_GroupThreadID, int3 dispatchThreadID : SV_DispatchThreadID, uint groupThreadIndex : SV_GroupIndex)
{
	if ((Vrs.vrsVarLum == 0) && (Vrs.vrsAvgLum == 0)) {
		vrsOut[groupID.xy] = 0x0;
		return;
	}

	uint vrsOutValue = 0x0;

	int samplesTotal = TILE_SIZE * TILE_SIZE;
	int samplesByThread = samplesTotal / N;
	if (samplesTotal % N > groupThreadIndex) {
		samplesByThread++;
	}

	float luminanceTotal = 0.0f;
	for (uint i = groupThreadIndex; i < samplesTotal; i += N) {
		uint2 texIndex = groupID.xy * TILE_SIZE;
		uint2 thread2dIndex = uint2(i % TILE_SIZE, i / TILE_SIZE);
		texIndex += thread2dIndex;

		float luminanceSample = luminance(colorTex[texIndex].xyz);
		luminanceTotal += luminanceSample;
		if (Vrs.vrsVarLum == 1) {
			luminanceSquare[i] = luminanceSample;
		}
	}

	if (Vrs.vrsVarLum == 1) {
		// Have to sync on the groupMemory, probably needed since WaveActiveSum doesn't guarantee thread-sync(?)
		GroupMemoryBarrierWithGroupSync();

		int exceedCutoffX = 0;
		int exceedCutoffY = 0;
		// Trying to do if neighbors exceed the variance cutoff, they default to 1x1 shading.
		for (uint j = groupThreadIndex; j < samplesTotal; j += N) {
			float lumCenter = luminanceSquare[j];
			float lumRight = luminanceSquare[min(j + 1, (samplesTotal - 1))];
			float lumDown = luminanceSquare[min(j + TILE_SIZE, (samplesTotal - 1))];

			if (abs(lumCenter - lumRight) > Vrs.vrsVarianceCut) {
				exceedCutoffX++;
			}
			if (abs(lumCenter - lumDown) > Vrs.vrsVarianceCut) {
				exceedCutoffY++;
			}
		}

		exceedCutoffX = WaveActiveSum(exceedCutoffX);
		exceedCutoffY = WaveActiveSum(exceedCutoffY);

		if (exceedCutoffX < Vrs.vrsVarianceVotes) {
			vrsOutValue += 0x4;
		}
		if (exceedCutoffY < Vrs.vrsVarianceVotes) {
			vrsOutValue += 0x1;
		}
	}

	if (Vrs.vrsAvgLum == 1) {
		luminanceTotal = WaveActiveSum(luminanceTotal);

		float luminanceAverage = luminanceTotal / samplesTotal;
		if (luminanceAverage < Vrs.vrsAvgCut) {
			vrsOutValue += 0x5;
		}
	}

	vrsOut[groupID.xy] = vrsOutValue;
}