Texture2D colorTex : register(t0);

RWTexture2D<uint> vrsOut : register (u0);

float luminance(float3 color) {
		return 0.2126 * color.x + 0.7152 * color.y + 0.0722 * color.z;
}

[numthreads(N,N,1)]
void VRSOut(int3 groupThreadID : SV_GroupThreadID, int3 dispatchThreadID : SV_DispatchThreadID) 
{
	float luminanceTotal = 0.0f;
	int2 texIndex = dispatchThreadID.xy * TILE_SIZE - int2(EXTRA_SAMPLES/2, EXTRA_SAMPLES/2);
	for (int i = 0; i < TILE_SIZE + EXTRA_SAMPLES; i++) {
		for (int j = 0; j < TILE_SIZE + EXTRA_SAMPLES; j++) {
			luminanceTotal += luminance(colorTex[texIndex + int2(i,j)].xyz);		
		}		
	}
	luminanceTotal /= ((TILE_SIZE+EXTRA_SAMPLES)*(TILE_SIZE+EXTRA_SAMPLES));
	if (luminanceTotal > 0.4) {
		vrsOut[dispatchThreadID.xy] = 0x0;	
		return;
	}
	if (luminanceTotal > 0.2) {
		vrsOut[dispatchThreadID.xy] = 0x4;	
		return;
	}
	if (luminanceTotal > 0.1) {
		vrsOut[dispatchThreadID.xy] = 0x5;	
		return;
	}
	vrsOut[dispatchThreadID.xy] = 0xa;
}