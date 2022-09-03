
struct vertex
{
	float vx, vy, vz;
	float nx, ny, nz;
	float tu, tv;
};

struct VsOutput
{
	float4 Position : SV_Position;
	float4 Color : COLOR;
};

StructuredBuffer<vertex> VertexBuffer;

VsOutput main(uint VertexIndex:SV_VertexID)
{
	vertex Vertex = VertexBuffer.Load(VertexIndex);
	float3 Position = float3(Vertex.vx, Vertex.vy, Vertex.vz);
	float3 Normal = float3(Vertex.nx, Vertex.ny, Vertex.nz);
	float2 TexCoord = float2(Vertex.tu, Vertex.tv);

	VsOutput Output;
	Output.Position = float4(Position + float3(0, 0, 0.5), 1.0);
	Output.Color = float4(Normal * 0.5 + float3(0.5, 0.5, 0.5), 1.0f);
	return Output;
}
