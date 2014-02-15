#include "Exporter.hpp"

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <random>
#include <sstream>
#include <memory>
#include <map>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

struct Material
{
	int index;

	std::string name;
	std::string diffuseTexture;
	std::string specularTexture;
	std::string normalTexture;
	std::string heightTexture;

	aiColor4D   diffuse;
	aiColor4D   ambient;
	aiColor4D   specular;
	aiColor4D   emissive;

	float shininess;
};

struct ExportMesh
{
	aiVector3D position;
	float      radius;

	uint32_t materialIndex;
	uint32_t numVertices;
	uint32_t verticesOffset;
	uint32_t verticesSize;
	uint32_t numIndices;
	uint32_t indicesOffset;
	uint32_t indicesSize;
};

struct ConvertedMesh
{
	std::vector<ExportVertex> vertices;
	std::vector<IndexType> indices;
	aiVector3D position;
	float radiusFromOrigo;
};

namespace
{
	std::array<float, 3> aiVector3DToArray(aiVector3D vec)
	{
		return{{ vec.x, vec.y, vec.z }};
	}

	std::array<float, 2> aiVector3DToArray2D(aiVector3D vec)
	{
		return{ { vec.x, vec.y } };
	}

	ExportVertex aiVertexToExportVertex(const aiMesh* const mesh, int index)
	{
		ExportVertex v;
		v.position = aiVector3DToArray(mesh->mVertices[index]);
		v.normal = aiVector3DToArray(mesh->mNormals[index]);

		if (mesh->HasTangentsAndBitangents())
		{
			v.tangent = aiVector3DToArray(mesh->mTangents[index]);
			v.bitangent = aiVector3DToArray(mesh->mBitangents[index]);
		}

#if 0
		if (aiMesh->HasVertexColors(0))
		{
			aiColor4D color = aiMesh->mColors[0][i];
			v.color = aiColor4DToGLM(color).swizzle(glm::comp::R, glm::comp::G, glm::comp::B);
		}
		else v.color = glm::vec3(1, 1, 1);
#endif

		if (mesh->HasTextureCoords(0))
		{
			v.uv = aiVector3DToArray2D(mesh->mTextureCoords[0][index]);
		}

		return std::move(v);
	}

	std::string GetTexture(const aiMaterial * const mat, aiTextureType type)
	{
		aiString path;

		if (AI_SUCCESS == mat->GetTexture(type, 0, &path))
		{
			std::string str = std::string(path.C_Str());
			std::replace(str.begin(), str.end(), '\\', '/');
			return str;
		}

		return std::string("");
	}

	// Update sphere to contain point if it doesn't -- from Ericson, see below.
	void SphereOfSphereAndPt(aiVector3D& sphereCenter, float& sphereRadius, const aiVector3D& p)
	{
		auto d = p - sphereCenter;
		float distSqrd = d * d; // dot product

		// If point outside sphere
		if (distSqrd > sphereRadius*sphereRadius)
		{
			float dist = sqrt(distSqrd);
			float newRadius = (sphereRadius + dist) * 0.5f;
			float k = (newRadius - sphereRadius) / dist;
			sphereRadius = newRadius;
			sphereCenter += d * k;
		}
	}

	aiVector3D FindCenterOfBoundingSphere(const aiMesh* const aiMesh)
	{
#if 0 // Calculate center using the average of positions
		glm::vec3 averagePos;

		const int numVertices = aiMesh->mNumVertices;

		for (int i = 0; i < numVertices; ++i)
		{
			aiVector3D const* const pPos = &(aiMesh->mVertices[i]);
			averagePos += glm::vec3(pPos->x, pPos->y, pPos->z);
		}

		return averagePos / (float)numVertices;
#else // Calculates a bounding sphere using the iterative-refinement-version of Ritter's method from Real-Time Collision Detection by Christer Ericson.
		const int numVertices = aiMesh->mNumVertices;

		// Compute indices to the two most separated points of the (up to) six points
		// defining the AABB encompassing the point set.
		int min, max;
		{
			int minx = 0, maxx = 0, miny = 0, maxy = 0, minz = 0, maxz = 0;
			for (int i = 1; i < numVertices; ++i)
			{
				const auto p = aiMesh->mVertices[i];
				if (p.x < aiMesh->mVertices[minx].x) minx = i;
				if (p.x > aiMesh->mVertices[maxx].x) maxx = i;
				if (p.y < aiMesh->mVertices[miny].y) miny = i;
				if (p.y > aiMesh->mVertices[maxy].y) maxy = i;
				if (p.z < aiMesh->mVertices[minz].z) minz = i;
				if (p.z > aiMesh->mVertices[maxz].z) maxz = i;
			}

			auto dx = aiMesh->mVertices[maxx] - aiMesh->mVertices[minx];
			auto dy = aiMesh->mVertices[maxy] - aiMesh->mVertices[miny];
			auto dz = aiMesh->mVertices[maxz] - aiMesh->mVertices[minz];

			// Squared distances (dot product)
			float distSqrdX = dx * dx;
			float distSqrdY = dy * dy;
			float distSqrdZ = dz * dz;

			// Pick pair (min,max) of points most distant
			min = minx;
			max = maxx;
			if (distSqrdY > distSqrdX && distSqrdY > distSqrdZ) {
				max = maxy;
				min = miny;
			}
			if (distSqrdZ > distSqrdX && distSqrdZ > distSqrdY) {
				max = maxz;
				min = minz;
			}
		}

		aiVector3D sphereCenter = (aiMesh->mVertices[min] + aiMesh->mVertices[max]) * 0.5f;
		const auto dc = aiMesh->mVertices[max] - sphereCenter;
		float sphereRadius = sqrt(dc * dc);

		// Grow sphere from found center to include all points
		for (int i = 0; i < numVertices; ++i)
		{
			SphereOfSphereAndPt(sphereCenter, sphereRadius, aiMesh->mVertices[i]);
		}

		// Do iterative refinement for better approximation of optimal center

		// Need copy of vertices for swapping
		std::unique_ptr<aiVector3D[]> copy(new aiVector3D[numVertices]);
		memcpy(copy.get(), aiMesh->mVertices, sizeof(aiVector3D)* numVertices);

		float sphereRadius2 = sphereRadius;
		aiVector3D sphereCenter2 = sphereCenter;

		const int NUM_ITER = 16;
		for (int k = 0; k < NUM_ITER; ++k)
		{
			sphereRadius2 *= 0.95f;

			// Note: numVertices - 1
			for (int i = 0; i < numVertices-1; ++i)
			{
				static std::mt19937 mt;
				std::uniform_int_distribution<> dis(i+1, numVertices-1);

				// Swap p[i] with p[j], where j randomly from interval [i+1, numPts-1]
				std::swap(copy[i], copy[dis(mt)]);

				SphereOfSphereAndPt(sphereCenter2, sphereRadius2, copy[i]);
			}

			// Include the last point as well (no swapping here)
			SphereOfSphereAndPt(sphereCenter2, sphereRadius2, copy[numVertices - 1]);

			// Check if an improvement
			if (sphereRadius2 < sphereRadius)
			{
				sphereRadius = sphereRadius2;
				sphereCenter = sphereCenter2;
			}
		}

		return sphereCenter;
#endif
	}

	void ConvertMesh(const aiMesh* const mesh, bool placeAtOrigio, ConvertedMesh& outConvertedMesh)
	{
		float maxVertexDistanceFromOrigo = 0.0f;

		aiVector3D vertexOffset{ 0, 0, 0 };

		if (placeAtOrigio)
			vertexOffset = FindCenterOfBoundingSphere(mesh);

		const int numVertices = mesh->mNumVertices;
		outConvertedMesh.vertices.resize(numVertices);

		for (int i = 0; i < numVertices; ++i)
		{
			ExportVertex exportVertex = aiVertexToExportVertex(mesh, i);
			exportVertex.position[0] -= vertexOffset.x;
			exportVertex.position[1] -= vertexOffset.y;
			exportVertex.position[2] -= vertexOffset.z;
			outConvertedMesh.vertices[i] = exportVertex;

			float distanceFromOrigo = sqrt(exportVertex.position[0] * exportVertex.position[0] + exportVertex.position[1] * exportVertex.position[1] + exportVertex.position[2] * exportVertex.position[2]);
			maxVertexDistanceFromOrigo = std::max(maxVertexDistanceFromOrigo, distanceFromOrigo);
		}

		for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
		{
			const aiFace& face = mesh->mFaces[i];
			for (unsigned int j = 0; j < face.mNumIndices; ++j)
			{
				outConvertedMesh.indices.push_back(face.mIndices[j]);
			}
		}

		outConvertedMesh.position = vertexOffset;
		outConvertedMesh.radiusFromOrigo = maxVertexDistanceFromOrigo;
	}

	std::string ToLuaString(const aiColor4D& c)
	{
		std::stringstream ss;
		ss << "{r=" << c.r << ", g=" << c.g << ",b=" << c.b << ", a=" << c.a << "}";
		return ss.str();
	}

	std::string ToLuaString(const aiVector3D& c)
	{
		std::stringstream ss;
		ss << "{x=" << c.x << ", y=" << c.y << ", z=" << c.z << "}";
		return ss.str();
	}

	void ExportScene(const std::string& luaFile, const std::vector<ExportMesh>& meshes, const std::vector<Material>& materials)
	{
		std::stringstream ss;
		ss << luaFile << ".lua";

		std::ofstream file;
		file.open(ss.str(), std::ios::trunc | std::ios::out);

		if (file.is_open())
		{
			file << "return {" << std::endl;

			file << "\tdatafile='" << "meshdata.bin" << "'," << std::endl;

			// Write mesh-information
			file << "\tmeshes = {" << std::endl;
			for (auto& mod : meshes)
			{
				file << "\t\t{ ";
				file << "material_index=" << mod.materialIndex + 1 << ", ";
				file << "position=" << ToLuaString(mod.position) << ", ";
				file << "vertices_size=" << mod.verticesSize << ", ";
				file << "vertices_offset = " << mod.verticesOffset << ", ";
				file << "num_vertices=" << mod.numVertices << ", ";
				file << "indices_size=" << mod.indicesSize << ", ";
				file << "indices_offset = " << mod.indicesOffset << ", ";
				file << "num_indices=" << mod.numIndices << ", ";
				file << "radius=" << mod.radius << ",";
				file << " }," << std::endl;
			}
			file << "\t},\n" << std::endl;

			// Write material-information
			file << "\tmaterials = {" << std::endl;
			for (auto& material : materials)
			{
				file << "\t\t{ diffuse=" << ToLuaString(material.diffuse) << ", ";

				if (material.diffuseTexture != "")
					file << "diffuse_texture='" << material.diffuseTexture << "', ";

				file << "specular=" << ToLuaString(material.specular) << ", ";

				if (material.specularTexture != "")
					file << "specular_texture='" << material.specularTexture << "', ";

				file << "ambient=" << ToLuaString(material.ambient) << ", ";

				if (material.normalTexture != "")
					file << "normal_texture='" << material.normalTexture << "', ";
				if (material.heightTexture != "")
					file << "height_texture='" << material.heightTexture << "', ";

				file << "emissive=" << ToLuaString(material.emissive) << ", ";
				file << "shininess=" << material.shininess;
				file << " },\n";
			}
			file << "\t}\n}" << std::endl;
		}

		file.close();
	}

	std::vector<Material> GetMaterials(const aiScene* const scene)
	{
		std::vector<Material> materials;

		for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
		{
			auto& mat = scene->mMaterials[i];

			aiString name;
			mat->Get(AI_MATKEY_NAME, name);

			aiColor4D tempColor;

			aiColor4D diffuse(0.8f, 0.8f, 0.8f, 1.0f);
			if (AI_SUCCESS == aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &tempColor))
				diffuse = tempColor;

			aiColor4D ambient(0.01f, 0.01f, 0.01f, 1.0f);
			if (AI_SUCCESS == aiGetMaterialColor(mat, AI_MATKEY_COLOR_AMBIENT, &tempColor))
				ambient = tempColor;

			aiColor4D specular(1.0f, 1.0f, 1.0f, 1.0f);
			if (AI_SUCCESS == aiGetMaterialColor(mat, AI_MATKEY_COLOR_SPECULAR, &tempColor))
				specular = tempColor;

			aiColor4D emissive(0.0f, 0.0f, 0.0f, 1.0f);
			if (AI_SUCCESS == aiGetMaterialColor(mat, AI_MATKEY_COLOR_EMISSIVE, &tempColor))
				emissive = tempColor;

			float shininess = 0.0;
			unsigned int max;
			if (AI_SUCCESS != aiGetMaterialFloatArray(mat, AI_MATKEY_SHININESS, &shininess, &max))
			{
				shininess = 0.0;
			}

			if (shininess == 0.0)
				shininess = 40.0f * 4.0f;

			std::string diffTex = GetTexture(mat, aiTextureType_DIFFUSE);
			std::string normalTex = GetTexture(mat, aiTextureType_NORMALS);
			std::string heightTex = GetTexture(mat, aiTextureType_HEIGHT);
			std::string specTex = GetTexture(mat, aiTextureType_SPECULAR);

			Material exportMaterial;
			exportMaterial.index = i;
			exportMaterial.name = name.C_Str();
			exportMaterial.diffuse = diffuse;
			exportMaterial.specular = specular;
			exportMaterial.emissive = emissive;
			exportMaterial.ambient = ambient;
			exportMaterial.shininess = shininess / 4.0f; // Assimp scales by 4 apparently?
			exportMaterial.diffuseTexture = diffTex;
			exportMaterial.specularTexture = specTex;
			exportMaterial.normalTexture = normalTex;
			exportMaterial.heightTexture = heightTex;

			materials.push_back(std::move(exportMaterial));
		}

		return materials;
	}

	std::vector<ExportMesh> ConvertAndExportMeshes(const aiScene* const scene, ModelExporter::ExportOptions options, std::ofstream& meshDataStream)
	{
		std::vector<ExportMesh> exportMeshes;

		uint32_t currentFileOffset = 0;
		ConvertedMesh convertedMesh;

		for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
		{
			const aiMesh* const aiMesh = scene->mMeshes[i];

			// Reuse convertedMesh for less memory allocations
			convertedMesh.vertices.clear();
			convertedMesh.indices.clear();

			ConvertMesh(aiMesh, options == ModelExporter::ExportOptions::MeshesCentered, convertedMesh);

			ExportMesh exp;
			exp.materialIndex = aiMesh->mMaterialIndex;
			exp.radius = convertedMesh.radiusFromOrigo;
			exp.position = convertedMesh.position;

			exp.numVertices = convertedMesh.vertices.size();
			exp.numIndices = convertedMesh.indices.size();

			exp.verticesSize = convertedMesh.vertices.size() * sizeof(convertedMesh.vertices[0]);
			exp.indicesSize = convertedMesh.indices.size() * sizeof(convertedMesh.indices[0]);

			exp.verticesOffset = currentFileOffset;
			meshDataStream.write((char*)convertedMesh.vertices.data(), exp.verticesSize);
			currentFileOffset += exp.verticesSize;

			exp.indicesOffset = currentFileOffset;
			meshDataStream.write((char*)convertedMesh.indices.data(), convertedMesh.indices.size() * sizeof(convertedMesh.indices[0]));
			currentFileOffset += exp.indicesSize;

			exportMeshes.push_back(std::move(exp));

			std::cout << i << " done\n";
		}

		return std::move(exportMeshes);
	}
}

namespace ModelExporter
{
	bool Export(const std::string& file, ExportOptions options)
	{
		Assimp::Importer m_importer;

		const aiScene* scene = m_importer.ReadFile(file,
			aiProcess_CalcTangentSpace |
			aiProcess_FlipWindingOrder |
			aiProcess_Triangulate |
			aiProcess_GenSmoothNormals |
			//aiProcess_GenNormals | 
			//aiProcess_JoinIdenticalVertices  |
			//aiProcess_FlipUVs |
			aiProcess_SortByPType |
			aiProcess_GenUVCoords |
			aiProcess_FindInvalidData
			//aiProcess_FixInfacingNormals  
			//aiProcess_MakeLeftHanded  |
			//aiProcess_LimitBoneWeights
			);

		if (!scene)
		{
			std::cout << "Error loading mesh from " << file << std::endl;
			std::cout << m_importer.GetErrorString() << std::endl;
			return false;
		}

		if (scene->mNumMeshes < 1)
		{
			std::cout << "Error loading mesh from " << file << std::endl;
			std::cout << "File contains no loadable meshes." << std::endl;
			return false;
		}

		std::cout << "Loaded mesh from " << file << ":" << std::endl;
		std::cout << "\tmNumMeshes:" << scene->mNumMeshes << std::endl;
		std::cout << "\tmNumMatrials:" << scene->mNumMaterials << std::endl << std::endl;

		// Prepare mesh-data stream
		const std::string meshDataFilename = "meshdata.bin";

		std::ofstream meshDataStream;
		meshDataStream.open(meshDataFilename, std::ios::binary | std::ios::out);

		if (!meshDataStream.is_open())
		{
			fprintf(stderr, "Couldn't create %s\n", meshDataFilename.c_str());
			return false;
		}

		// Convert and export meshes
		std::vector<ExportMesh> exportMeshes = ConvertAndExportMeshes(scene, options, meshDataStream);
		meshDataStream.close();

		// Convert materials
		std::vector<Material> materials = GetMaterials(scene);

		// Write scene-data to LUA
		ExportScene("scene", exportMeshes, materials);

		return true;
	}
}