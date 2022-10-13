
#include "mesh_headers.hlsl"
#define UseWave 1

struct draw_cull_data
{
	float4 Data[6];
	uint LodEnabled;
	uint CullEnabled;
};

struct draw_count
{
	uint Data;
};

[[vk::binding(0)]] StructuredBuffer<mesh_offset> MeshOffsetBuffer;
[[vk::binding(1)]] StructuredBuffer<mesh> MeshDataBuffer;
[[vk::binding(2)]] RWStructuredBuffer<mesh_draw_command> DrawCommands;
[[vk::binding(3)]] RWStructuredBuffer<draw_count> DrawCount;
[[vk::push_constant]] draw_cull_data DrawCullData;


[numthreads(32, 1, 1)]
void main(uint3 WorkGroupID : SV_GroupID, uint3 LocalInvocation : SV_GroupThreadID, uint ThreadIndex : SV_GroupIndex)
{
	uint ti = LocalInvocation.x;
	uint gi = WorkGroupID.x;
	uint di = gi * 32 + ti;

	mesh Mesh = MeshDataBuffer[MeshOffsetBuffer[di].MeshIndex];

	float3 Center = MeshOffsetBuffer[di].Center * MeshOffsetBuffer[di].Scale + MeshOffsetBuffer[di].Pos;
	float Radius = MeshOffsetBuffer[di].Radius * MeshOffsetBuffer[di].Scale;

	bool IsVisible = true;
	IsVisible = IsVisible && dot(DrawCullData.Data[0], float4(Center, 1)) > -Radius;
	IsVisible = IsVisible && dot(DrawCullData.Data[1], float4(Center, 1)) > -Radius;
	IsVisible = IsVisible && dot(DrawCullData.Data[2], float4(Center, 1)) > -Radius;
	IsVisible = IsVisible && dot(DrawCullData.Data[3], float4(Center, 1)) > -Radius;
	IsVisible = IsVisible && dot(DrawCullData.Data[4], float4(Center, 1)) > -Radius;
	IsVisible = IsVisible && dot(DrawCullData.Data[5], float4(Center, 1)) > -Radius;

	IsVisible = (DrawCullData.CullEnabled == 1) ? IsVisible : true;

#if UseWave
	uint VisibleCount = WaveActiveCountBits(IsVisible);

	if(VisibleCount == 0)
	{
		return;
	}

	uint DrawCommandGroupIndex = 0;
	
	if(ti == 0)
	{
		InterlockedAdd(DrawCount[0].Data, VisibleCount, DrawCommandGroupIndex);
	}

	uint Index = WavePrefixSum(IsVisible);
	uint DrawCommandIndex = WaveReadLaneFirst(DrawCommandGroupIndex) + Index;

#endif

	if(IsVisible)
	{
#if !UseWave
		uint DrawCommandIndex;
		InterlockedAdd(DrawCount[0].Data, 1, DrawCommandIndex);
#endif
		float LodDistance = log2(max(1, distance(Center, float3(0, 0, 0)) - Radius));
		uint LodIndex = clamp(int(LodDistance), 0, Mesh.LodCount - 1);

		LodIndex = (DrawCullData.LodEnabled == 1) ? LodIndex : 0;

		mesh_lod Lod = Mesh.Lods[LodIndex];

		DrawCommands[DrawCommandIndex].DrawIndex = di;
		DrawCommands[DrawCommandIndex].IndexCount = Lod.IndexCount;
		DrawCommands[DrawCommandIndex].InstanceCount = 1;
		DrawCommands[DrawCommandIndex].FirstIndex = Lod.IndexOffset;
		DrawCommands[DrawCommandIndex].VertexOffset = Mesh.VertexOffset;
		DrawCommands[DrawCommandIndex].FirstInstance = 0;

		DrawCommands[DrawCommandIndex].TaskCount = (Lod.MeshletCount + 31) / 32;
		DrawCommands[DrawCommandIndex].FirstTask = Lod.MeshletOffset / 32;
	}
}

