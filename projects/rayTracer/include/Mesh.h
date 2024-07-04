#pragma once
#include <cassert>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <glm/vec3.hpp>

#include "ShaderStruct.h"
#include "ShaderStructs.h"


struct Mesh final : ShaderStruct{
	Mesh(int firstTriangleIndex, int nTriangle, int materialIndex, bool visible, std::string name):
	firstTriangleIndex(firstTriangleIndex), nTriangle(nTriangle), materialIndex(materialIndex), visible(visible), name(name){};

	int firstTriangleIndex, nTriangle, materialIndex;
	bool visible;
	std::string name;
	[[nodiscard]] std::vector<std::byte> GetBytes() override {
		return ConvertToBytes(firstTriangleIndex, nTriangle, materialIndex, visible);
	}
};

static std::vector<std::shared_ptr<Mesh>> loadMesh(const char* filePath, std::vector<std::shared_ptr<Triangle>> *triangles) {
	// Open the OBJ file
	std::ifstream file(filePath);
	assert(file.is_open());

	// Temporary storage for vertices and normals
	std::vector<glm::vec3> vertices;
	std::vector<glm::vec3> normals;

	// Temporary variables to compute bounds
	std::vector<std::shared_ptr<Mesh>> objects{};
	// Read each line of the file
	std::string line;

	while (std::getline(file, line)) {
		if (line.substr(0, 2) == "o ") {
			objects.push_back(
				std::make_shared<Mesh>(
					static_cast<int>(triangles->size()),
					0, 0,
					true,
					line.substr(2, line.size()))
			);
		}
		// Parse vertex data
		else if (line.substr(0, 2) == "v ") {
			std::istringstream iss(line.substr(2));
			float x, y, z;
			iss >> x >> y >> z;
			vertices.emplace_back(x, y, z);
		}
		else if (line.substr(0, 3) == "vn ") {
			std::istringstream iss(line.substr(3));
			float x, y, z;
			iss >> x >> y >> z;
			normals.emplace_back(x, y, z);
		}
		// Parse face data (triangles)
		else if (line.substr(0, 2) == "f ") {
			std::istringstream iss(line.substr(2));
			int idx1, idx2, idx3, normal1, normal2, normal3, tex1, tex2, tex3;
			char slash;

			iss >> idx1 >> slash >> tex1 >> slash >> normal1
				>> idx2 >> slash >> tex2 >> slash >> normal2
				>> idx3 >> slash >> tex3 >> slash >> normal3;

			// Add triangle to mesh, OBJ files are 1-indexed, so decrement indices
			triangles->push_back(std::make_shared<Triangle>(
				vertices[idx1 - 1],
				vertices[idx2 - 1],
				vertices[idx3 - 1],
				normals[normal1 - 1],
				normals[normal2 - 1],
				normals[normal3 - 1]
			));
			objects[objects.size()-1]->nTriangle++;
		}
	}
	// Close the file
	file.close();
	return objects;
}
