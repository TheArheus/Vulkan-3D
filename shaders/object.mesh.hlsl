
struct vertex
{
	float vx, vy, vz;
	float nx, ny, nz;
	float tu, tv;
};

struct meshlet
{
	uint Vertices[64];
	uint Indices[126]; 
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

[numthreads(1, 1, 1)]
[outputtopology("triangle")]
void main(uint3 WorkGroupID : SV_GroupID,
		  out vertices VsOutput OutVertices[64], out indices uint3 OutIndices[42])
{
	uint mi = WorkGroupID.x;
	meshlet CurrentMeshlet = MeshletBuffer.Load(mi);
	SetMeshOutputCounts(CurrentMeshlet.VertexCount, CurrentMeshlet.TriangleCount);

	for(uint VIndex = 0;
		VIndex < CurrentMeshlet.VertexCount;
		++VIndex)
	{
		uint CurrentVertex = CurrentMeshlet.Vertices[VIndex];

		vertex Vertex = VertexBuffer.Load(CurrentVertex);
		float3 Position = float3(Vertex.vx, Vertex.vy, Vertex.vz);
		float3 Normal = float3(Vertex.nx, Vertex.ny, Vertex.nz);
		float2 TexCoord = float2(Vertex.tu, Vertex.tv);

		OutVertices[VIndex].Position = float4(Position + float3(0, 0, 0.5), 1.0);
		OutVertices[VIndex].Color = float4(Normal * 0.5 + float3(0.5, 0.5, 0.5), 1.0f);
	}

	for(uint IIndex = 0;
		IIndex < CurrentMeshlet.TriangleCount;
		IIndex += 1)
	{
		OutIndices[IIndex] = uint3(CurrentMeshlet.Indices[IIndex*3+0], CurrentMeshlet.Indices[IIndex*3+1], CurrentMeshlet.Indices[IIndex*3+2]);
	}
}

