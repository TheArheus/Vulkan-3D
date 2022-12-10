
#include "mesh_headers.hlsl"

struct draw_cull_data
{
	float4 Data[6];
	uint LodEnabled;
	uint CullEnabled;
	uint OcclusionEnabled;

	float P00, P11, znear;
	float PyramidWidth, PyramidHeight;
};

struct draw_count
{
	uint Data;
};

struct draw_visibility
{
	uint IsVisible;
};

[[vk::binding(0)]] StructuredBuffer<mesh_offset> MeshOffsetBuffer;
[[vk::binding(1)]] StructuredBuffer<mesh> MeshDataBuffer;
[[vk::binding(2)]] RWStructuredBuffer<mesh_draw_command> DrawCommands;
[[vk::binding(3)]] RWStructuredBuffer<draw_count> DrawCount;
[[vk::binding(4)]] RWStructuredBuffer<draw_visibility> DrawVisibility;

[[vk::combinedImageSampler]][[vk::binding(5)]]
Texture2D<float> DepthPyramid;
[[vk::combinedImageSampler]][[vk::binding(5)]]
SamplerState DepthPyramidSampler;

[[vk::push_constant]] draw_cull_data DrawCullData;

struct project_sphere_result
{
	bool IsProjected;
	float4 aabb;
};

project_sphere_result ProjectSphere(in float3 C, // camera-space sphere center
								   in float  r, // sphere radius
								   in float  NearZ, // near clipping plane position (negative)
								   in float  P00,
								   in float  P11)
{
	project_sphere_result Result = {0, float4(0, 0, 0, 0)};
	if(C.z < r + NearZ)
	{
		Result.IsProjected = false;
		return Result;
	}

	float2 cx = -C.xz;
	float2 vx = float2(sqrt(dot(cx, cx) - r * r), r) / length(cx);
	float2 minx = mul(float2x2(vx.x, vx.y, -vx.y, vx.x), cx);
	float2 maxx = mul(float2x2(vx.x, -vx.y, vx.y, vx.x), cx);

	float2 cy = -C.yz;
	float2 vy = float2(sqrt(dot(cy, cy) - r * r), r) / length(cy);
	float2 miny = mul(float2x2(vy.x, -vy.y, vy.y, vy.x), cy);
	float2 maxy = mul(float2x2(vy.x, vy.y, -vy.y, vy.x), cy);

	Result.IsProjected = true;
	Result.aabb = float4(minx.x / minx.y * P00, miny.x / miny.y * P11, maxx.x / maxx.y * P00, maxy.x / maxy.y * P11) * 
		   float4(0.5f, -0.5f, 0.5f, -0.5f) + float4(0.5f, 0.5f, 0.5f, 0.5f);

	return Result;
}

[numthreads(64, 1, 1)]
void main(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
	uint di = GlobalInvocationID.x;

	uint MeshIndex = MeshOffsetBuffer[di].MeshIndex;

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

	if(IsVisible && (DrawCullData.OcclusionEnabled == 1))
	{
		float4 aabb;
		project_sphere_result ProjectionResult = ProjectSphere(Center, Radius, DrawCullData.znear, DrawCullData.P00, DrawCullData.P11);
		if(ProjectionResult.IsProjected)
		{
			aabb = ProjectionResult.aabb;

			float Width  = (aabb.z - aabb.x) * DrawCullData.PyramidWidth;
			float Height = (aabb.w - aabb.y) * DrawCullData.PyramidHeight;

			float Level = floor(log2(max(Width, Height)));

			float Depth = DepthPyramid.SampleLevel(DepthPyramidSampler, (aabb.xy + aabb.zw) * 0.5, Level).x;

			float DepthSphere = DrawCullData.znear / (Center.z - Radius);

			IsVisible = IsVisible && (DepthSphere > Depth);
		}
	}

	if(IsVisible && (DrawVisibility[di].IsVisible == 0))
	{
		uint DrawCommandIndex;
		InterlockedAdd(DrawCount[0].Data, 1, DrawCommandIndex);

		float LodDistance = log2(max(1, distance(Center, float3(0, 0, 0)) - Radius));
		uint LodIndex = clamp(int(LodDistance), 0, MeshDataBuffer[MeshIndex].LodCount - 1);

		LodIndex = (DrawCullData.LodEnabled == 1) ? LodIndex : 0;

		mesh_lod Lod = MeshDataBuffer[MeshIndex].Lods[LodIndex];

		DrawCommands[DrawCommandIndex].DrawIndex = di;
		DrawCommands[DrawCommandIndex].IndexCount = Lod.IndexCount;
		DrawCommands[DrawCommandIndex].InstanceCount = 1;
		DrawCommands[DrawCommandIndex].FirstIndex = Lod.IndexOffset;
		DrawCommands[DrawCommandIndex].VertexOffset = MeshDataBuffer[MeshIndex].VertexOffset;
		DrawCommands[DrawCommandIndex].FirstInstance = 0;

		DrawCommands[DrawCommandIndex].TaskCount = (Lod.MeshletCount + 31) / 32;
		DrawCommands[DrawCommandIndex].FirstTask = Lod.MeshletOffset / 32;
	}

	DrawVisibility[di].IsVisible = IsVisible ? 1 : 0;
}

