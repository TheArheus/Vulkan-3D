
#define CULL 1

struct TsOutput
{
	uint Meshlets[32];
};

struct meshlet
{
	float4 Cone;
	uint Vertices[64];
	uint Triangles[126][3];
	uint VertexCount;
	uint TriangleCount;
};

bool ConeCullTest(float4 Cone, float3 View)
{
	return dot(Cone.xyz, View) < Cone.w;
}

[[vk::binding(1)]] StructuredBuffer<meshlet> MeshletBuffer;

groupshared TsOutput TaskOutput;
groupshared uint MeshletCount;

[numthreads(32, 1, 1)]
[outputtopology("triangle")]
void main(uint3 WorkGroupID : SV_GroupID, uint3 LocalInvocation : SV_GroupThreadID, uint ThreadIndex : SV_GroupIndex)
{
	uint ti  = LocalInvocation.x;
	uint mgi = WorkGroupID.x;
	uint mi = mgi * 32 + ti;

#if CULL
    MeshletCount = 0;

    meshlet CurrentMeshlet = MeshletBuffer[mi];
    uint Accepted = !ConeCullTest(CurrentMeshlet.Cone, float3(0, 0, 1));
	uint CurrentIndex = WavePrefixSum(Accepted);

	if(Accepted)
	{
		TaskOutput.Meshlets[CurrentIndex] = mi;
	}

	MeshletCount = WaveActiveCountBits(Accepted);

    if(ti == 0)
    {
        DispatchMesh(MeshletCount, 1, 1, TaskOutput);
    }
#else
	TaskOutput.Meshlets[ti] = mi;
	if(ti == 0)
	{
		DispatchMesh(32, 1, 1, TaskOutput);
	}
#endif
}

