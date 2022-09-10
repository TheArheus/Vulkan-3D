
struct vertex
{
	float vx, vy, vz;
	u32 norm;
	uint16_t tu, tv;
};

struct meshlet
{
	float Cone[4];
	u32 Vertices[64];
	u32 Indices[126*3];
	u32 VertexCount;
	u32 TriangleCount;
};

struct alignas(16) mesh_offset
{
	float Pos[2];
	float Scale[2];
};

struct meshlet_cone
{
	float Cone[3];
	float Angle;
};

struct build_meshlet_data
{
	u32 Vertices[64];
	u32 Indices[126*3];
	u32 VertexCount;
	u32 TriangleCount;
};

struct mesh
{
	std::vector<vertex> Vertices;
	std::vector<u32> Indices;
	std::vector<meshlet> Meshlets;
};

internal bool
LoadMesh(mesh& Result, const char* Path)
{
	ObjFile File;
	if(!objParseFile(File, Path))
	{
		return false;
	}
	size_t IndexCount = File.f_size / 3;
	std::vector<vertex> Vertices(IndexCount);

	for(u32 VertexIndex = 0;
		VertexIndex < IndexCount;
		++VertexIndex)
	{
		vertex& V = Vertices[VertexIndex];

		int VIndex = File.f[VertexIndex * 3 + 0];
		int VTextureIndex = File.f[VertexIndex * 3 + 1];
		int VNormalIndex = File.f[VertexIndex * 3 + 2];

		V.vx = File.v[VIndex * 3 + 0];
		V.vy = File.v[VIndex * 3 + 1];
		V.vz = File.v[VIndex * 3 + 2];

		float nx = VNormalIndex < 0 ? 0.f : File.vn[VNormalIndex * 3 + 0];
		float ny = VNormalIndex < 0 ? 0.f : File.vn[VNormalIndex * 3 + 1];
		float nz = VNormalIndex < 0 ? 0.f : File.vn[VNormalIndex * 3 + 2];

		V.norm = ((u8)(nx * 127 + 127) << 24) | ((u8)(ny * 127 + 127) << 16) | ((u8)(nz * 127 + 127) << 8) | 0;

		V.tu = meshopt_quantizeHalf(VTextureIndex < 0 ? 0.f : File.vt[VTextureIndex * 3 + 0]);
		V.tv = meshopt_quantizeHalf(VTextureIndex < 0 ? 0.f : File.vt[VTextureIndex * 3 + 1]);
	}

	std::vector<u32> Remap(IndexCount);
	size_t VertexCount = meshopt_generateVertexRemap(Remap.data(), 0, IndexCount, Vertices.data(), IndexCount, sizeof(vertex));

	Result.Vertices.resize(VertexCount);
	Result.Indices.resize(IndexCount);

	meshopt_remapVertexBuffer(Result.Vertices.data(), Vertices.data(), IndexCount, sizeof(vertex), Remap.data());
	meshopt_remapIndexBuffer(Result.Indices.data(), 0, IndexCount, Remap.data());

	meshopt_optimizeVertexCache(Result.Indices.data(), Result.Indices.data(), IndexCount, VertexCount);
	meshopt_optimizeVertexFetch(Result.Vertices.data(), Result.Indices.data(), IndexCount, Result.Vertices.data(), VertexCount, sizeof(vertex));

	return true;
}

internal meshlet_cone
BuildMeshletCone(mesh& Mesh, build_meshlet_data& Meshlet)
{
	meshlet_cone Result = {};

	float Normals[126][3] = {};
	for(u32 TriangleIndex = 0;
		TriangleIndex < Meshlet.TriangleCount;
		++TriangleIndex)
	{
		u32 a = Meshlet.Indices[TriangleIndex * 3 + 0];
		u32 b = Meshlet.Indices[TriangleIndex * 3 + 1];
		u32 c = Meshlet.Indices[TriangleIndex * 3 + 2];

		const vertex& va = Mesh.Vertices[Meshlet.Vertices[a]];
		const vertex& vb = Mesh.Vertices[Meshlet.Vertices[b]];
		const vertex& vc = Mesh.Vertices[Meshlet.Vertices[c]];

		float p0[3] = {va.vx, va.vy, va.vz};
		float p1[3] = {vb.vx, vb.vy, vb.vz};
		float p2[3] = {vc.vx, vc.vy, vc.vz};

		float p10[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
		float p20[3] = {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};

		float nx = p10[1] * p20[2] - p10[2] * p20[1];
		float ny = p10[2] * p20[0] - p10[0] * p20[2];
		float nz = p10[0] * p20[1] - p10[1] * p20[0];

		float Length = sqrtf(nx * nx + ny * ny + nz * nz);
		float InvLength = Length == 0.0f ? 0.0f : 1.0f / Length;

		Normals[TriangleIndex][0] = nx * InvLength;
		Normals[TriangleIndex][1] = ny * InvLength;
		Normals[TriangleIndex][2] = nz * InvLength;
	}

	float AvgNormal[3] = {};

	for(u32 TriangleIndex = 0;
		TriangleIndex < Meshlet.TriangleCount;
		++TriangleIndex)
	{
		AvgNormal[0] += Normals[TriangleIndex][0];
		AvgNormal[1] += Normals[TriangleIndex][1];
		AvgNormal[2] += Normals[TriangleIndex][2];
	}

	float AvgLength = sqrtf(AvgNormal[0] * AvgNormal[0] + 
							AvgNormal[1] * AvgNormal[1] + 
							AvgNormal[2] * AvgNormal[2]);

	if(AvgLength == 0.0f)
	{
		AvgNormal[0] = 0.0f;
		AvgNormal[1] = 0.0f;
		AvgNormal[2] = 0.0f;
	}
	else
	{
		AvgNormal[0] /= AvgLength;
		AvgNormal[1] /= AvgLength;
		AvgNormal[2] /= AvgLength;
	}

	float MinDotProd = 1.0f;
	for(u32 TriangleIndex = 0;
		TriangleIndex < Meshlet.TriangleCount;
		++TriangleIndex)
	{
		float DotProd = Normals[TriangleIndex][0] * AvgNormal[0] + 
						Normals[TriangleIndex][1] * AvgNormal[1] + 
						Normals[TriangleIndex][2] * AvgNormal[2];

		MinDotProd = DotProd < MinDotProd ? DotProd : MinDotProd;
	}

	float ConeW = MinDotProd <= 0.0f ? -1.0f : -sqrtf(1.0f - MinDotProd * MinDotProd);

	Result.Cone[0] = AvgNormal[0];
	Result.Cone[1] = AvgNormal[1];
	Result.Cone[2] = AvgNormal[2];
	Result.Angle = ConeW;

	return Result;
}

internal void
BuildMeshlets(mesh& Mesh)
{
	build_meshlet_data NewMeshletBuildData = {};

	std::vector<build_meshlet_data> BuildMeshlets;
	std::vector<u32> MeshletVertices(Mesh.Vertices.size(), 0xffffffff);

	for(size_t TriangleIndex = 0;
		TriangleIndex < Mesh.Indices.size(); 
		TriangleIndex += 3)
	{
		u32 a = Mesh.Indices[TriangleIndex + 0];
		u32 b = Mesh.Indices[TriangleIndex + 1];
		u32 c = Mesh.Indices[TriangleIndex + 2];

		// NOTE: This is not optimal as it is using a lot of space
		// try to use atleast 16 bit data
		u32& VertexA = MeshletVertices[a];
		u32& VertexB = MeshletVertices[b];
		u32& VertexC = MeshletVertices[c];

		if((NewMeshletBuildData.VertexCount + (VertexA == -1) + (VertexB == -1) + (VertexC == -1) > 64) || (NewMeshletBuildData.TriangleCount >= 126))
		{
			BuildMeshlets.push_back(NewMeshletBuildData);
			for (size_t InnerIndex = 0; InnerIndex < NewMeshletBuildData.VertexCount; InnerIndex++) 
			{
				MeshletVertices[NewMeshletBuildData.Vertices[InnerIndex]] = 0xffffffff;
			}
			NewMeshletBuildData = {};
			
		}

		if(VertexA == -1)
		{
			VertexA = NewMeshletBuildData.VertexCount;
			NewMeshletBuildData.Vertices[NewMeshletBuildData.VertexCount++] = a;
		}

		if(VertexB == -1)
		{
			VertexB = NewMeshletBuildData.VertexCount;
			NewMeshletBuildData.Vertices[NewMeshletBuildData.VertexCount++] = b;
		}

		if(VertexC == -1)
		{
			VertexC = NewMeshletBuildData.VertexCount;
			NewMeshletBuildData.Vertices[NewMeshletBuildData.VertexCount++] = c;
		}

		NewMeshletBuildData.Indices[NewMeshletBuildData.TriangleCount*3+0] = VertexA;
		NewMeshletBuildData.Indices[NewMeshletBuildData.TriangleCount*3+1] = VertexB;
		NewMeshletBuildData.Indices[NewMeshletBuildData.TriangleCount*3+2] = VertexC;
		NewMeshletBuildData.TriangleCount++;
	}

	if(NewMeshletBuildData.TriangleCount)
	{
		BuildMeshlets.push_back(NewMeshletBuildData);
	}

	for(build_meshlet_data BuildMeshletData : BuildMeshlets)
	{
		meshlet NewMeshlet = {};
		meshlet_cone Cone = BuildMeshletCone(Mesh, BuildMeshletData);

		NewMeshlet.Cone[0] = Cone.Cone[0];
		NewMeshlet.Cone[1] = Cone.Cone[1];
		NewMeshlet.Cone[2] = Cone.Cone[2];
		NewMeshlet.Cone[3] = Cone.Angle;
		NewMeshlet.VertexCount   = BuildMeshletData.VertexCount;
		NewMeshlet.TriangleCount = BuildMeshletData.TriangleCount;

		for(u32 VertexIndex = 0;
			VertexIndex < BuildMeshletData.VertexCount;
			++VertexIndex)
		{
			NewMeshlet.Vertices[VertexIndex] = BuildMeshletData.Vertices[VertexIndex];
		}

		for(u32 TriangleIndex = 0;
			TriangleIndex < BuildMeshletData.TriangleCount;
			++TriangleIndex)
		{
			NewMeshlet.Indices[TriangleIndex*3+0] = BuildMeshletData.Indices[TriangleIndex*3+0];
			NewMeshlet.Indices[TriangleIndex*3+1] = BuildMeshletData.Indices[TriangleIndex*3+1];
			NewMeshlet.Indices[TriangleIndex*3+2] = BuildMeshletData.Indices[TriangleIndex*3+2];
		}

		Mesh.Meshlets.push_back(NewMeshlet);
	}

	while (Mesh.Meshlets.size() % 32)
	{
		meshlet NullMeshlet = {};
		Mesh.Meshlets.push_back(NullMeshlet);
	}
}

