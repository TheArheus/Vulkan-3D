
struct vertex
{
	float vx, vy, vz;
	uint norm;
	float16_t tu, tv;
};

struct meshlet
{
	float4 Cone;
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
	uint CommandData[7];
};

float3 RotateQuat(float3 V, float4 Q)
{
	return V + 2.0f * cross(Q.xyz, cross(Q.xyz, V) + Q.w * V);
}

bool ConeCullTest(float3 Cone, float CutOff, float3 View)
{
	return dot(Cone, View) < CutOff;
}

