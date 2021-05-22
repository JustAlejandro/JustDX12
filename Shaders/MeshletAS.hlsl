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

bool IsVisible(CullData c, float4x4 world, float3 viewPos)
{
	if (PerPass.meshletCull == 0)
	{
		return true;
	}
	// Hopefully generating more aggressive culling data makes this less often
	if (IsConeDegenerate(c))
	{
		return true;
	}
	
	// This is all the same logic as the mesh shading culling example in
	// the DX12 graphics samples from Microsoft, we'll see how well it performs in
	// a real world environment and see if we can make the culling more aggressive.
	//float4 center = mul(float4(c.BoundingSphere.xyz, 1), world);
	float4 center = mul(float4(c.BoundingSphere.xyz, 1), world);
	
	float4 normalCone = UnpackCone(c.NormalCone);
	
	float3 axis = mul(float4(normalCone.xyz, 0), world).xyz;
	
	float3 apex = center.xyz - axis * c.ApexOffset;
	
	float3 view = normalize(viewPos - apex);
	
	if (dot(view, -normalize(axis)) > normalCone.w)
	{
		return false;		
	}
	
	return true;
}

[NumThreads(GROUP_SIZE, 1, 1)]
void AS(uint gtid : SV_GroupThreadID, uint dtid : SV_DispatchThreadID, uint gid : SV_GroupID)
{
	bool visible = false;

	if (dtid < MeshInfo.MeshletCount)
	{
		visible = IsVisible(MeshletCullData[dtid], PerObject.world[0], PerPass.EyePosW);
	}
	
	if (visible) {
		uint index = WavePrefixCountBits(visible);
		s_Payload.MeshletIndices[index] = dtid;
	}
	
	uint visibleCount = WaveActiveCountBits(visible);
	DispatchMesh(visibleCount, 1, 1, s_Payload);
}