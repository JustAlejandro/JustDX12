Texture2D colorTex : register(t0);

RWTexture2D<uint> vrsOut : register (u0);

[numthreads(N,N,1)]
void VRSOut(int3 groupThreadID : SV_GroupThreadID, int3 dispatchThreadID : SV_DispatchThreadID) 
{
	vrsOut[dispatchThreadID.xy] = 0xa;	
}