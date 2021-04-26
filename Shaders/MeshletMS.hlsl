#include "MeshletCommon.hlsl"

ConstantBuffer<PerObject> PerObject : register(b1);

ConstantBuffer<PerPass> PerPass : register(b2);

// Have to start a bit later because the MeshletCommon uses all the other registers.
Texture2D gDiffuseMap : register(t5);
Texture2D gSpecularMap : register(t6);
Texture2D gNormalMap : register(t7);
Texture2D gEmissiveMap : register(t8);
SamplerState anisoWrap : register(s4);

// Flipping tris because the loader defaults to OpenGL winding order.
uint3 UnpackPrimitive(uint primitive) {
	return uint3(primitive & 0x3FF,
		(primitive >> 10) & 0x3FF,
		(primitive >> 20) & 0x3FF);	
}

uint GetVertexIndex(Meshlet m, uint localIndex) {
	localIndex = m.VertOffset + localIndex;
	
	if (MeshInfo.IndexSize == 4) {
		return UniqueVertexIndices.Load(localIndex * 4);	
	}
	else {
		uint wordOffset = (localIndex & 0x1);
		uint byteOffset = (localIndex / 2) * 4;
		
		uint indexPair = UniqueVertexIndices.Load(byteOffset);
		uint index = (indexPair >> (wordOffset * 16)) & 0xFFFF;
		
		return index;
	}
}

uint3 GetPrimitive(Meshlet m, uint index) {
	return UnpackPrimitive(PrimitiveIndices[m.PrimOffset + index]);	
}

// Different 'VertexOut' than the one in sample.
VertexOut GetVertexAttributes(uint meshletIndex, uint vertexIndex) {
	Vertex v = Vertices[vertexIndex];
	
	float4 pos = mul(float4(v.Position,1.0f), PerObject.world[0]);//mul(float4(v.Position, 1.0f), PerObject.world);
	
	VertexOut vout;
	vout.PosW = pos.xyz;
	vout.PosH = mul(pos, PerPass.ViewProj);
	vout.NormalW = normalize(mul(-v.Normal, (float3x3) PerObject.world[0]));
	vout.BiNormalW = normalize(mul(v.Bitangent, (float3x3) PerObject.world[0]));
	vout.TangentW = normalize(mul(v.Tangent, (float3x3) PerObject.world[0]));
	vout.TexC = v.Texcoord;
	
	return vout;
}

[NumThreads(GROUP_SIZE, 1, 1)]
[OutputTopology("triangle")]
void MS(uint gtid : SV_GroupThreadID,
	uint gid : SV_GroupID,
	in payload Payload payload,
	out vertices VertexOut verts[GROUP_SIZE],
	out indices uint3 tris[GROUP_SIZE]) {
	
	uint meshletIndex = payload.MeshletIndices[gid];
	
	if (meshletIndex >= MeshInfo.MeshletCount)
	{
		return;	
	}
	
	Meshlet m = Meshlets[meshletIndex];
	
	SetMeshOutputCounts(m.VertCount, m.PrimCount);
	
	if (gtid < m.VertCount) {
		uint vertexIndex = GetVertexIndex(m, gtid);
		verts[gtid] = GetVertexAttributes(gid, vertexIndex);
	}
	
	if (gtid < m.PrimCount) {
		tris[gtid] = GetPrimitive(m, gtid);	
	}
}

PixelOut PS(VertexOut pin)
{
	PixelOut p;

	p.color = gDiffuseMap.Sample(anisoWrap, flipYofUV(pin.TexC));
	
	p.specular = gSpecularMap.Sample(anisoWrap, flipYofUV(pin.TexC));
	
	float2 texNormal = gNormalMap.Sample(anisoWrap, flipYofUV(pin.TexC)).xy;
	texNormal = texNormal * 2.0f - 1.0f;
	float3 inNormal = (float3(texNormal.xy, 1.0f - dot(texNormal, texNormal)));
	float3x3 TBN = float3x3(pin.TangentW, pin.BiNormalW, pin.NormalW);
	
	p.normal = float4(-normalize(mul(inNormal, TBN)), 0.0f);
	p.tangent = float4(pin.TangentW, 0.0);
	p.emissive = gEmissiveMap.Sample(anisoWrap, pin.TexC);
	return p;
}