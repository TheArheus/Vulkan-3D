
struct TsOutput
{
	uint Meshlets[32];
};

struct vertex
{
	float vx, vy, vz;
	uint norm;
	float16_t tu, tv;
};

struct meshlet
{
	float4 Cone;
	uint Vertices[64];
	uint Triangles[126][3];
	uint VertexCount;
	uint TriangleCount;
};

struct mesh_offset
{
	float2 Pos;
	float2 Scale;
};

struct VsOutput
{
	float4 Position : SV_Position;
	float4 Color : COLOR;
};

[[vk::binding(0)]] StructuredBuffer<vertex> VertexBuffer;
[[vk::binding(1)]] StructuredBuffer<meshlet> MeshletBuffer;
[[vk::push_constant]] ConstantBuffer<mesh_offset> MeshOffsetBuffer;

uint Hash(uint a)
{
   a = (a+0x7ed55d16) + (a<<12);
   a = (a^0xc761c23c) ^ (a>>19);
   a = (a+0x165667b1) + (a<<5);
   a = (a+0xd3a2646c) ^ (a<<9);
   a = (a+0xfd7046c5) + (a<<3);
   a = (a^0xb55a4f09) ^ (a>>16);
   return a;
}

bool ConeCullTest(float4 Cone, float3 View)
{
	return dot(Cone.xyz, View) < Cone.w;
}

[numthreads(32, 1, 1)]
[outputtopology("triangle")]
void main(uint3 WorkGroupID : SV_GroupID, uint3 LocalInvocation : SV_GroupThreadID, uint ThreadIndex : SV_GroupIndex, in payload TsOutput TaskOutput,
		  out vertices VsOutput OutVertices[64], out indices uint3 OutIndices[126])
{
	uint mi = TaskOutput.Meshlets[WorkGroupID.x];
	meshlet CurrentMeshlet = MeshletBuffer[mi];

	SetMeshOutputCounts(CurrentMeshlet.VertexCount, CurrentMeshlet.TriangleCount);
#if VK_DEBUG
	uint MeshletHash = Hash(mi);
	float3 Color = float3(float(MeshletHash & 255), float((MeshletHash >> 8) & 255), float((MeshletHash >> 16) & 255)) / 255.0f;
#endif

	uint nx, ny, nz;
	uint VertexCount = CurrentMeshlet.VertexCount;
	uint TriangleCount = CurrentMeshlet.TriangleCount;
	float3 DrawOffset = float3(MeshOffsetBuffer.Pos.x, MeshOffsetBuffer.Pos.y, 0);
	float3 DrawScale  = float3(MeshOffsetBuffer.Scale.x, MeshOffsetBuffer.Scale.y, 1);

	for(uint VIndex = ThreadIndex;
		VIndex < VertexCount;
		VIndex += 32)
	{
		uint CurrentVertex = CurrentMeshlet.Vertices[VIndex];

		vertex Vertex = VertexBuffer[CurrentVertex];
		float3 Position = float3(Vertex.vx, Vertex.vy, Vertex.vz);
		nx = (Vertex.norm & 0xff000000) >> 24;
		ny = (Vertex.norm & 0x00ff0000) >> 16;
		nz = (Vertex.norm & 0x0000ff00) >>  8;
		float3 Normal = float3(nx, ny, nz) / 127.0 - 1.0;
		float2 TexCoord = float2(Vertex.tu, Vertex.tv);

		OutVertices[VIndex].Position = float4(Position * DrawScale + DrawOffset + float3(0, 0, 0.5), 1.0);
#if VK_DEBUG
		OutVertices[VIndex].Color = float4(Color, 1.0f);
#else
		OutVertices[VIndex].Color = float4(Normal * 0.5 + float3(0.5, 0.5, 0.5), 1.0f);
#endif
	}

	for(uint IIndex = ThreadIndex;
		IIndex < TriangleCount;
		IIndex += 32)
	{
		OutIndices[IIndex] = uint3(CurrentMeshlet.Triangles[IIndex][0], CurrentMeshlet.Triangles[IIndex][1], CurrentMeshlet.Triangles[IIndex][2]);
	}
}

