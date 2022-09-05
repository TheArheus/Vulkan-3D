
struct vertex
{
	float16_t vx, vy, vz;
	uint norm;
	float16_t tu, tv;
};

struct meshlet
{
	uint Vertices[64];
	uint Indices[126*3]; 
	uint TriangleCount;
	uint VertexCount;
};

struct VsOutput
{
	float4 Position : SV_Position;
	float4 Color : COLOR;
};

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

StructuredBuffer<vertex> VertexBuffer;
StructuredBuffer<meshlet> MeshletBuffer;

[numthreads(32, 1, 1)]
[outputtopology("triangle")]
void main(uint3 WorkGroupID : SV_GroupID, uint ThreadIndex : SV_GroupIndex,
		  out vertices VsOutput OutVertices[64], out indices uint3 OutIndices[126])
{
	uint mi = WorkGroupID.x;
	meshlet CurrentMeshlet = MeshletBuffer[mi];
	SetMeshOutputCounts(CurrentMeshlet.VertexCount, CurrentMeshlet.TriangleCount);

#if VK_DEBUG
	uint MeshletHash = Hash(mi);
	float3 Color = float3(float(MeshletHash & 255), float((MeshletHash >> 8) & 255), float((MeshletHash >> 16) & 255)) / 255.0f;
#endif

	uint nx, ny, nz;
	uint VertexCount = CurrentMeshlet.VertexCount;
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

		OutVertices[VIndex].Position = float4(Position + float3(0, 0, 0.5), 1.0);
#if VK_DEBUG
		OutVertices[VIndex].Color = float4(Color, 1.0f);
#else
		OutVertices[VIndex].Color = float4(Normal * 0.5 + float3(0.5, 0.5, 0.5), 1.0f);
#endif
	}

	uint TriangleCount = CurrentMeshlet.TriangleCount;
	for(uint IIndex = ThreadIndex;
		IIndex < TriangleCount;
		IIndex += 32)
	{
		OutIndices[IIndex] = uint3(CurrentMeshlet.Indices[IIndex*3+0], CurrentMeshlet.Indices[IIndex*3+1], CurrentMeshlet.Indices[IIndex*3+2]);
	}
}

