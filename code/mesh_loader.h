
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

	u32 VertexOffset;

	u32 IndexOffset;
	u32 IndexCount;

	u32 MeshletOffset;
	u32 MeshletCount;
};

struct mesh_draw_command
{
	VkDrawIndexedIndirectCommand DrawCommand;
	VkDrawMeshTasksIndirectCommandNV MeshletDrawCommand;
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
	glm::vec3 Center;
	float Radius;

	u32 VertexOffset;
	u32 VertexCount;

	u32 MeshletOffset;
	u32 MeshletCount;

	u32 IndexOffset;
	u32 IndexCount;
};

struct geometry
{
	std::vector<vertex> Vertices;
	std::vector<u32> Indices;
	std::vector<meshlet> Meshlets;
	std::vector<mesh> Meshes;
};

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

	u32 ResultVertexOffset = u32(Result.Vertices.size());
	u32 ResultVertexCount = (u32)VertexCount;

	u32 ResultIndexOffset = u32(Result.Indices.size());
	u32 ResultIndexCount = (u32)IndexCount;

	Result.Vertices.insert(Result.Vertices.end(), Vertices.begin(), Vertices.end());
	Result.Indices.insert(Result.Indices.end(), Indices.begin(), Indices.end());

	u32 ResultMeshletOffset = u32(Result.Meshlets.size());
	u32 ResultMeshletCount = 0;

	if(MakeMeshlets)
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

		ResultMeshletCount = u32(BuildMeshlets.size());
	}

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

	mesh NewMeshData = {};
	NewMeshData.Radius		  = Radius;
	NewMeshData.Center		  = Center;
	NewMeshData.VertexCount   = ResultVertexCount;
	NewMeshData.VertexOffset  = ResultVertexOffset;
	NewMeshData.IndexCount    = ResultIndexCount;
	NewMeshData.IndexOffset   = ResultIndexOffset;
	NewMeshData.MeshletCount  = ResultMeshletCount;
	NewMeshData.MeshletOffset = ResultMeshletOffset;
	Result.Meshes.push_back(NewMeshData);

	return true;
}

