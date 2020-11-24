#include "MeshletCommon.hlsl"

ConstantBuffer<PerObject> PerObject : register(b1);

ConstantBuffer<PerPass> PerPass : register(b2);

// Have to start a bit later because the MeshletCommon uses all the other registers.
Texture2D gDiffuseMap : register(t5);
Texture2D gSpecularMap : register(t6);
Texture2D gNormalMap : register(t7);
SamplerState anisoWrap : register(s4);

// The groupshared payload data to export to dispatched mesh shader threadgroups
groupshared Payload s_Payload;

[NumThreads(GROUP_SIZE, 1, 1)]
void AS(uint gtid : SV_GroupThreadID, uint dtid : SV_DispatchThreadID, uint gid : SV_GroupID)
{
	bool visible = true;
	if (dtid >= MeshInfo.MeshletCount)
	{
		visible = false;
	}
	
	if (visible) {
		uint index = WavePrefixCountBits(visible);
		s_Payload.MeshletIndices[index] = dtid;
	}
	
	uint visibleCount = WaveActiveCountBits(visible);
	DispatchMesh(visibleCount, 1, 1, s_Payload);
}