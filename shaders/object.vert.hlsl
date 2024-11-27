
#include "mesh_headers.hlsl"

struct VsOutput
{
	float4 Position : SV_Position;
	float4 Color : COLOR;
};

[[vk::binding(0)]] StructuredBuffer<mesh_offset> MeshOffsetBuffer;
[[vk::binding(1)]] StructuredBuffer<mesh_draw_command> DrawCommands;
[[vk::binding(2)]] StructuredBuffer<vertex> VertexBuffer;
[[vk::push_constant]] ConstantBuffer<globals> Globals;

VsOutput main([[vk::builtin("DrawIndex")]] int DrawIndex : A, uint VertexIndex:SV_VertexID)
{
	mesh_offset MeshOffsetData = MeshOffsetBuffer[DrawCommands[DrawIndex].DrawIndex];

	vertex Vertex = VertexBuffer[VertexIndex];
	float3 DrawOffset = MeshOffsetData.Pos;
	float3 DrawScale  = float3(MeshOffsetData.Scale, MeshOffsetData.Scale, 1);
	float4x4 Projection = Globals.Proj;
	float4 Orientation = MeshOffsetData.Orient;

	float3 Position = float3(Vertex.vx, Vertex.vy, Vertex.vz);
	uint nx, ny, nz;
	nx = (Vertex.norm & 0xff000000) >> 24;
	ny = (Vertex.norm & 0x00ff0000) >> 16;
	nz = (Vertex.norm & 0x0000ff00) >>  8;
	float3 Normal = float3(nx, ny, nz) / 127.0 - 1.0;
	float2 TexCoord = float2(Vertex.tu, Vertex.tv);

	VsOutput Output;
	Output.Position = mul(Projection, float4(RotateQuat(Position, Orientation) * DrawScale + DrawOffset, 1.0));
	Output.Color = float4(Normal * 0.5 + float3(0.5, 0.5, 0.5), 1.0f);
	return Output;
}
