
struct vertex
{
	float vx, vy, vz;
	uint norm;
	float16_t tu, tv;
};

struct mesh_lod
{
	uint IndexOffset;
	uint IndexCount;

	uint MeshletOffset;
	uint MeshletCount;
};

struct mesh
{
	float3 Center;
	float Radius;

	uint VertexOffset;
	uint VertexCount;

	uint LodCount;
	mesh_lod Lods[8];
};

struct meshlet
{
	float3 Center;
	float Radius;
	float3 ConeAxis;
	float ConeCutoff;

	uint Vertices[64];
	uint Triangles[126][3];
	uint VertexCount;
	uint TriangleCount;
};

struct globals
{
	float4x4 Proj;
};

struct mesh_offset
{
	float3 Pos;
	float Scale;
	float4 Orient;

	float3 Center;
	float Radius;

	uint MeshIndex;
	uint VertexOffset;
};

struct mesh_draw_command
{
	uint DrawIndex;

	uint IndexCount;
	uint InstanceCount;
	uint FirstIndex;
	uint VertexOffset;
	uint FirstInstance;

	uint TaskCount;
	uint FirstTask;
};

float3 RotateQuat(float3 V, float4 Q)
{
	return V + 2.0f * cross(Q.xyz, cross(Q.xyz, V) + Q.w * V);
}

bool ConeCullTest(float3 Center, float Radius, float3 ConeAxis, float ConeCutoff, float3 CameraPos)
{
	return dot(Center - CameraPos, ConeAxis) > ConeCutoff * length(Center - CameraPos) + Radius;
}

