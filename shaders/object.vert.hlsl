
struct vertex
{
	float vx, vy, vz;
	uint norm;
	float16_t tu, tv;
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

RWByteAddressBuffer VertexBuffer;
[[vk::push_constant]] ConstantBuffer<mesh_offset> MeshOffsetBuffer;

VsOutput main(uint VertexIndex:SV_VertexID)
{
	vertex Vertex = VertexBuffer.Load<vertex>(VertexIndex*20);
	float3 DrawOffset = float3(MeshOffsetBuffer.Pos.x, MeshOffsetBuffer.Pos.y, 0);
	float3 DrawScale  = float3(MeshOffsetBuffer.Scale.x, MeshOffsetBuffer.Scale.y, 1);

	float3 Position = float3(Vertex.vx, Vertex.vy, Vertex.vz);
	uint nx, ny, nz;
	nx = (Vertex.norm & 0xff000000) >> 24;
	ny = (Vertex.norm & 0x00ff0000) >> 16;
	nz = (Vertex.norm & 0x0000ff00) >>  8;
	float3 Normal = float3(nx, ny, nz) / 127.0 - 1.0;
	float2 TexCoord = float2(Vertex.tu, Vertex.tv);

	VsOutput Output;
	Output.Position = float4(Position * DrawScale + DrawOffset + float3(0, 0, 0.5), 1.0);
	Output.Color = float4(Normal * 0.5 + float3(0.5, 0.5, 0.5), 1.0f);
	return Output;
}
