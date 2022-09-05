
struct vertex
{
	float16_t vx, vy, vz;
	uint norm;
	float16_t tu, tv;
};

struct VsOutput
{
	float4 Position : SV_Position;
	float4 Color : COLOR;
};

RWByteAddressBuffer VertexBuffer;

VsOutput main(uint VertexIndex:SV_VertexID)
{
	vertex Vertex = VertexBuffer.Load<vertex>(VertexIndex*16);
	float3 Position = float3(Vertex.vx, Vertex.vy, Vertex.vz);
	uint nx, ny, nz;
	nx = (Vertex.norm & 0xff000000) >> 24;
	ny = (Vertex.norm & 0x00ff0000) >> 16;
	nz = (Vertex.norm & 0x0000ff00) >>  8;
	float3 Normal = float3(nx, ny, nz) / 127.0 - 1.0;
	float2 TexCoord = float2(Vertex.tu, Vertex.tv);

	VsOutput Output;
	Output.Position = float4(Position + float3(0, 0, 0.5), 1.0);
	Output.Color = float4(Normal * 0.5 + float3(0.5, 0.5, 0.5), 1.0f);
	return Output;
}
