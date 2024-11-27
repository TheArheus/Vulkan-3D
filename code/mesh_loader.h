
struct vertex
{
	float vx, vy, vz;
	u32 norm;
	uint16_t tu, tv;
};

struct meshlet
{
	float Center[3];
	float Radius;
	float ConeAxis[3];
	float ConeCutoff;

	u32 Vertices[64];
	u32 Indices[126*3];
	u32 VertexCount;
	u32 TriangleCount;
};

struct alignas(16) globals
{
	glm::mat4x4 Projection;
};

struct alignas(16) mesh_offset
{
	float Pos[3];
	float Scale;
	glm::quat Orient;

	glm::vec3 Center;
	float Radius;

	u32 MeshIndex;
	u32 VertexOffset;
};

struct mesh_draw_command
{
	u32 DrawIndex;
	VkDrawIndexedIndirectCommand DrawCommand;
	VkDrawMeshTasksIndirectCommandNV MeshletDrawCommand;
};

struct mesh_lod
{
	u32 IndexOffset;
	u32 IndexCount;

	u32 MeshletOffset;
	u32 MeshletCount;
};

struct alignas(16) mesh
{
	glm::vec3 Center;
	float Radius;

	u32 VertexOffset;
	u32 VertexCount;

	u32 LodCount;
	mesh_lod Lods[8];
};

struct geometry
{
	std::vector<vertex> Vertices;
	std::vector<u32> Indices;
	std::vector<meshlet> Meshlets;
	std::vector<mesh> Meshes;
};

internal size_t
BuildMeshlets(geometry& Result, std::vector<vertex>& Vertices, std::vector<u32>& Indices)
{
	u32 MaxVertices = 64;
	u32 MaxTriangles = 126;
	std::vector<meshopt_Meshlet> BuildMeshlets(meshopt_buildMeshletsBound(Indices.size(), MaxVertices, MaxTriangles));

	BuildMeshlets.resize(meshopt_buildMeshlets(BuildMeshlets.data(), Indices.data(), Indices.size(), Vertices.size(), MaxVertices, MaxTriangles));

	for(meshopt_Meshlet& BuildMeshletData : BuildMeshlets)
	{
		meshlet NewMeshlet = {};
		meshopt_Bounds Cone = meshopt_computeMeshletBounds(&BuildMeshletData, &Vertices[0].vx, Vertices.size(), sizeof(vertex));

		NewMeshlet.Center[0] = Cone.center[0];
		NewMeshlet.Center[1] = Cone.center[1];
		NewMeshlet.Center[2] = Cone.center[2];
		NewMeshlet.Radius = Cone.radius;
		NewMeshlet.ConeAxis[0] = Cone.cone_axis[0];
		NewMeshlet.ConeAxis[1] = Cone.cone_axis[1];
		NewMeshlet.ConeAxis[2] = Cone.cone_axis[2];
		NewMeshlet.ConeCutoff = Cone.cone_cutoff;
		NewMeshlet.VertexCount   = BuildMeshletData.vertex_count;
		NewMeshlet.TriangleCount = BuildMeshletData.triangle_count;

		for(u32 VertexIndex = 0;
			VertexIndex < BuildMeshletData.vertex_count;
			++VertexIndex)
		{
			NewMeshlet.Vertices[VertexIndex] = BuildMeshletData.vertices[VertexIndex];
		}

		for(u32 TriangleIndex = 0;
			TriangleIndex < BuildMeshletData.triangle_count;
			++TriangleIndex)
		{
			NewMeshlet.Indices[TriangleIndex*3+0] = BuildMeshletData.indices[TriangleIndex][0];
			NewMeshlet.Indices[TriangleIndex*3+1] = BuildMeshletData.indices[TriangleIndex][1];
			NewMeshlet.Indices[TriangleIndex*3+2] = BuildMeshletData.indices[TriangleIndex][2];
		}

		Result.Meshlets.push_back(NewMeshlet);
	}

	while(Result.Meshlets.size() % 32)
	{
		Result.Meshlets.push_back(meshlet());
	}

	return BuildMeshlets.size();
}

internal bool
LoadMesh(geometry& Result, const char* Path, bool MakeMeshlets)
{
	ObjFile File;
	if(!objParseFile(File, Path))
	{
		return false;
	}
	size_t IndexCount = File.f_size / 3;
	std::vector<vertex> TriangleVertices(IndexCount);

	for(u32 VertexIndex = 0;
		VertexIndex < IndexCount;
		++VertexIndex)
	{
		vertex& V = TriangleVertices[VertexIndex];

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
	size_t VertexCount = meshopt_generateVertexRemap(Remap.data(), 0, IndexCount, TriangleVertices.data(), IndexCount, sizeof(vertex));

	std::vector<vertex> Vertices(VertexCount);
	std::vector<u32> Indices(IndexCount);

	meshopt_remapVertexBuffer(Vertices.data(), TriangleVertices.data(), IndexCount, sizeof(vertex), Remap.data());
	meshopt_remapIndexBuffer(Indices.data(), 0, IndexCount, Remap.data());

	meshopt_optimizeVertexCache(Indices.data(), Indices.data(), IndexCount, VertexCount);
	meshopt_optimizeVertexFetch(Vertices.data(), Indices.data(), IndexCount, Vertices.data(), VertexCount, sizeof(vertex));

	mesh NewMeshData = {};

	//meshopt_simplify();

	NewMeshData.VertexOffset = u32(Result.Vertices.size());
	NewMeshData.VertexCount = (u32)VertexCount;
	Result.Vertices.insert(Result.Vertices.end(), Vertices.begin(), Vertices.end());

	glm::vec3 Center(0);
	for(const vertex& Vertex : Vertices)
	{
		Center += glm::vec3(Vertex.vx, Vertex.vy, Vertex.vz);
	}

	Center /= float(Vertices.size());

	float Radius = 0;
	for(const vertex& Vertex : Vertices)
	{
		float NewRadius = glm::distance(Center, glm::vec3(Vertex.vx, Vertex.vy, Vertex.vz));
		Radius += Radius < NewRadius ? Radius : NewRadius;
	}

	NewMeshData.Radius   = Radius;
	NewMeshData.Center   = Center;

	u32 LodIndex = 0;
	std::vector<u32> LodIndices = Indices;

	while(NewMeshData.LodCount < ArraySize(NewMeshData.Lods))
	{
		mesh_lod& Lod = NewMeshData.Lods[NewMeshData.LodCount++];

		Lod.IndexOffset   = u32(Result.Indices.size());
		Lod.IndexCount    = u32(LodIndices.size());

		Result.Indices.insert(Result.Indices.end(), LodIndices.begin(), LodIndices.end());

		Lod.MeshletOffset = u32(Result.Meshlets.size());
		Lod.MeshletCount  = MakeMeshlets ? (u32)BuildMeshlets(Result, Vertices, LodIndices) : 0;

		if(NewMeshData.LodCount < ArraySize(NewMeshData.Lods))
		{
			size_t NextIndicesTarget = size_t(LodIndices.size() * 0.75);
			size_t NextIndices = meshopt_simplify(LodIndices.data(), LodIndices.data(), LodIndices.size(), &Vertices[0].vx, Vertices.size(), sizeof(vertex), NextIndicesTarget, 1e-4f);

			if(NextIndices == LodIndices.size())
			{
				break;
			}

			LodIndices.resize(NextIndices);
			meshopt_optimizeVertexCache(LodIndices.data(), LodIndices.data(), LodIndices.size(), VertexCount);
		}
	}

	Result.Meshes.push_back(NewMeshData);

	return true;
}

