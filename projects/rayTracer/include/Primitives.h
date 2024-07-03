#pragma once
#include <glm/ext/matrix_float4x4.hpp>

static void transformVec3(const glm::mat4 &matrix, glm::vec3 &vector) {
	const auto _vector = matrix * glm::vec4(vector, 1.0f);
	vector = glm::vec3(_vector / _vector.w);
}

struct BoundingBox {
	float top, bottom, left, right;
};

struct Sphere
{
	glm::vec3 center;
	float radius;
	int materialIndex;
	Sphere Transform(const glm::mat4 &matrix) {
		transformVec3(matrix, center);
		return *this;
	}
};

struct TriangleData {
	glm::vec4 posA, posB, posC;
	glm::vec4 normalA, normalB, normalC;
};

struct Triangle {
	glm::vec3 posA, posB, posC;
	glm::vec3 normalA, normalB, normalC;
	Triangle Transform(const glm::mat4 &matrix) {
		transformVec3(matrix, posA);
		transformVec3(matrix, posB);
		transformVec3(matrix, posC);
		return *this;
	}
	TriangleData GetData(){
		return {
				{posA, 0.0f},{posB, 0.0f},{posC, 1.0f},
			{normalA, 0.0f},{normalB, 0.0f},{normalC, 1.0f},
			};
	}
};
