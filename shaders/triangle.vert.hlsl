struct VsOutput
{
	float4 Position : SV_Position;
};

VsOutput main(uint VertexIndex:SV_VertexID)
{
	float3 TriangleVertices[] = 
	{
		float3( 0.0,  0.5, 0.0),
		float3( 0.5, -0.5, 0.0),
		float3(-0.5, -0.8, 0.0),
	};

	VsOutput Output;
	Output.Position = float4(TriangleVertices[VertexIndex], 1.0);
	return Output;
}
