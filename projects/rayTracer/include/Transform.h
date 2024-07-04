#pragma once
#include <memory>
#include <glm/vec3.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>

class Transform {
public:
	Transform(
		glm::vec3 translation,
		glm::vec3 rotation,
		glm::vec3 scale,
		const glm::mat4 &matrix=glm::mat4(1),
		const bool dirty=false) :
	translation(translation), rotation(rotation), scale(scale), matrix(matrix), dirty(dirty){};

	glm::vec3 translation;
	glm::vec3 rotation;
	glm::vec3 scale;

	// Cached matrix
	mutable glm::mat4 matrix;
	mutable bool dirty;

	std::shared_ptr<Transform> parent;

	glm::mat4 GetRotationMatrix() const {
		auto matrix = glm::identity<glm::mat4>();
		matrix = rotate(matrix, rotation.y, glm::vec3(0, 1, 0));
		matrix = rotate(matrix, rotation.x, glm::vec3(1, 0, 0));
		matrix = rotate(matrix, rotation.z, glm::vec3(0, 0, 1));
		return matrix;
	}
};
