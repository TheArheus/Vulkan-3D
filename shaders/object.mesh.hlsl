
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

StructuredBuffer<vertex> VertexBuffer;
StructuredBuffer<meshlet> MeshletBuffer;

[numthreads(32, 1, 1)]
[outputtopology("triangle")]
void main(uint3 WorkGroupID : SV_GroupID, uint ThreadIndex : SV_GroupIndex,
		  out vertices VsOutput OutVertices[64], out indices uint3 OutIndices[126])
{
	uint mi = WorkGroupID.x;
	meshlet CurrentMeshlet = MeshletBuffer.Load(mi);
	SetMeshOutputCounts(CurrentMeshlet.VertexCount, CurrentMeshlet.TriangleCount);

	uint nx, ny, nz;
	uint VertexCount = CurrentMeshlet.VertexCount;
	for(uint VIndex = ThreadIndex;
		VIndex < VertexCount;
		VIndex += 32)
	{
		uint CurrentVertex = CurrentMeshlet.Vertices[VIndex];

		vertex Vertex = VertexBuffer.Load(CurrentVertex);
		float3 Position = float3(Vertex.vx, Vertex.vy, Vertex.vz);
		nx = (Vertex.norm & 0xff000000) >> 24;
		ny = (Vertex.norm & 0x00ff0000) >> 16;
		nz = (Vertex.norm & 0x0000ff00) >>  8;
		float3 Normal = float3(nx, ny, nz) / 127.0 - 1.0;
		float2 TexCoord = float2(Vertex.tu, Vertex.tv);

		OutVertices[VIndex].Position = float4(Position + float3(0, 0, 0.5), 1.0);
		OutVertices[VIndex].Color = float4(Normal * 0.5 + float3(0.5, 0.5, 0.5), 1.0f);
	}

	uint TriangleCount = CurrentMeshlet.TriangleCount;
	for(uint IIndex = ThreadIndex;
		IIndex < TriangleCount;
		IIndex += 32)
	{
		OutIndices[IIndex] = uint3(CurrentMeshlet.Indices[IIndex*3+0], CurrentMeshlet.Indices[IIndex*3+1], CurrentMeshlet.Indices[IIndex*3+2]);
	}
}

