
#include "mesh_headers.hlsl"

#define CULL 1

struct TsOutput
{
	uint Meshlets[32];
};


[[vk::binding(0)]] StructuredBuffer<mesh_offset> MeshOffsetBuffer;
[[vk::binding(1)]] StructuredBuffer<mesh_draw_command> DrawCommands;
[[vk::binding(3)]] StructuredBuffer<meshlet> MeshletBuffer;
[[vk::push_constant]] ConstantBuffer<globals> Globals;

groupshared TsOutput TaskOutput;
groupshared uint MeshletCount;

[numthreads(32, 1, 1)]
[outputtopology("triangle")]
void main([[vk::builtin("DrawIndex")]] int DrawIndex : A, 
		  uint3 WorkGroupID : SV_GroupID, uint3 LocalInvocation : SV_GroupThreadID, uint ThreadIndex : SV_GroupIndex)
{
	uint ti  = LocalInvocation.x;
	uint mgi = WorkGroupID.x;

	mesh_offset MeshOffsetData = MeshOffsetBuffer[DrawCommands[DrawIndex].DrawIndex];

	uint mi = mgi * 32 + ti;

#if CULL
	meshlet CurrentMeshlet = MeshletBuffer[mi];
	uint Accepted = !ConeCullTest(RotateQuat(CurrentMeshlet.Center, MeshOffsetData.Orient) * MeshOffsetData.Scale + MeshOffsetData.Pos, 
							 CurrentMeshlet.Radius,
							 RotateQuat(CurrentMeshlet.ConeAxis, MeshOffsetData.Orient), CurrentMeshlet.ConeCutoff, float3(0, 0, 0));

	uint CurrentIndex = WavePrefixSum(Accepted);

	if(Accepted)
	{
		TaskOutput.Meshlets[CurrentIndex] = mi;
	}

	uint Count = WaveActiveCountBits(Accepted);

    if(ti == 0)
    {
        DispatchMesh(Count, 1, 1, TaskOutput);
    }
#else
	TaskOutput.Meshlets[ti] = mi;

	if(ti == 0)
	{
		DispatchMesh(32, 1, 1, TaskOutput);
	}
#endif
}

