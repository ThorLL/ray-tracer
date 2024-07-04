#pragma once
#include <glm/ext/matrix_float4x4.hpp>
#include <numbers>

#include "Transform.h"

class Camera {
public:
	Camera() : viewMatrix(1), projMatrix(1), transform{
		ExtractTranslation(),
		ExtractRotation(),
		ExtractScale()
	}{}

	Camera(const glm::mat4 &view, const glm::mat4 &proj) :
		viewMatrix(view),
		projMatrix(proj),
		transform{
			ExtractTranslation(),
			ExtractRotation(),
			ExtractScale()
		}
	{}
	glm::mat4 viewMatrix, projMatrix;
	Transform transform;

	glm::vec3 postion() {
		return glm::vec3(inverse(viewMatrix)[3]);
	};

	[[nodiscard]] glm::vec3 ExtractTranslation() const
	{
		return transpose(viewMatrix) * -viewMatrix[3];
	}

	[[nodiscard]] glm::vec3 ExtractRotation() const
	{
		glm::vec3 rotation(0.0f);
		if (const float f = viewMatrix[1][2]; std::abs(std::abs(f) - 1.0f) < 0.00001f)
		{
			rotation.x = -f * static_cast<float>(std::numbers::pi) * 0.5f;
			rotation.y = std::atan2(-f * viewMatrix[0][1], -f * viewMatrix[0][0]);
			rotation.z = 0.0f;
		}
		else
		{
			rotation.x = -std::asin(f);
			const float cosX = std::cos(rotation.x);
			rotation.y = std::atan2(viewMatrix[0][2] / cosX, viewMatrix[2][2] / cosX);
			rotation.z = std::atan2(viewMatrix[1][0] / cosX, viewMatrix[1][1] / cosX);
		}
		return rotation;
	}

	[[nodiscard]] glm::vec3 ExtractScale() const
	{
		return {viewMatrix[0].length(), viewMatrix[1].length(), viewMatrix[2].length()};
	}

	void ExtractVectors(glm::vec3& right, glm::vec3& up, glm::vec3& forward) const
	{
		glm::mat3 transposed = transpose(viewMatrix);

		right = transposed[0];
		up = transposed[1];
		forward = transposed[2];
	}
};
