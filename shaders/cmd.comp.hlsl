
#include "mesh_headers.hlsl"

struct frustrum
{
	float4 Data[6];
};

[[vk::binding(0)]] StructuredBuffer<mesh_offset> MeshOffsetBuffer;
[[vk::binding(1)]] RWStructuredBuffer<mesh_draw_command> DrawCommands;
[[vk::push_constant]] frustrum Frustrum;


[numthreads(32, 1, 1)]
void main(uint3 WorkGroupID : SV_GroupID, uint3 LocalInvocation : SV_GroupThreadID, uint ThreadIndex : SV_GroupIndex)
{
	uint ti = LocalInvocation.x;
	uint gi = WorkGroupID.x;
	uint di = gi * 32 + ti;

	float3 Center = MeshOffsetBuffer[di].Center * MeshOffsetBuffer[di].Scale + MeshOffsetBuffer[di].Pos;
	float Radius = MeshOffsetBuffer[di].Radius * MeshOffsetBuffer[di].Scale;

	bool IsVisible = true;
	IsVisible = IsVisible && dot(Frustrum.Data[0], float4(Center, 1)) > -Radius;
	IsVisible = IsVisible && dot(Frustrum.Data[1], float4(Center, 1)) > -Radius;
	IsVisible = IsVisible && dot(Frustrum.Data[2], float4(Center, 1)) > -Radius;
	IsVisible = IsVisible && dot(Frustrum.Data[3], float4(Center, 1)) > -Radius;
	IsVisible = IsVisible && dot(Frustrum.Data[4], float4(Center, 1)) > -Radius;
	IsVisible = IsVisible && dot(Frustrum.Data[5], float4(Center, 1)) > -Radius;

	DrawCommands[di].IndexCount = MeshOffsetBuffer[di].IndexCount;
	DrawCommands[di].InstanceCount = IsVisible ? 1 : 0;
	DrawCommands[di].FirstIndex = MeshOffsetBuffer[di].IndexOffset;
	DrawCommands[di].VertexOffset = MeshOffsetBuffer[di].VertexOffset;
	DrawCommands[di].FirstInstance = 0;

	DrawCommands[di].TaskCount = IsVisible ? (MeshOffsetBuffer[di].MeshletCount + 31) / 32 : 0;
	DrawCommands[di].FirstTask = 0;
}
