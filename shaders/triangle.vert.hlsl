
struct VsInput
{
	[[vk::location(0)]]float3 Position : POSITION;
	[[vk::location(1)]]float3 Normal : NORMAL;
	[[vk::location(2)]]float2 TextCoord : TEXCOORD;
};

struct VsOutput
{
	float4 Position : SV_Position;
	float4 Color : COLOR;
};

VsOutput main(VsInput Input, uint VertexIndex:SV_VertexID)
{
	VsOutput Output;
	Output.Position = float4(Input.Position, 1.0);
	Output.Color = float4(Input.Normal * 0.5 + float3(0.5, 0.5, 0.5), 1.0f);
	return Output;
}
