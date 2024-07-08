#pragma once
#include <functional>
#include <glm/vec3.hpp>
#include <glm/ext/matrix_float4x4.hpp>
namespace Geometry {
	struct GeometryObject {

		virtual ~GeometryObject() = default;

		virtual void Transform(const glm::mat4 &matrix) = 0;
	};

	template <typename T, typename = std::enable_if_t<std::is_base_of_v<GeometryObject, T> && !std::is_same_v<GeometryObject, T>>>
		static T Transform_copy(T geometryObject, const glm::mat4 &matrix) {
		T copy = geometryObject;
		copy.Transform(matrix);
		return copy;
	}

	static void Transform(const glm::mat4 &matrix, glm::vec3 &vector) {
		const auto _vector = matrix * glm::vec4(vector, 1.0f);
		vector = glm::vec3(_vector / _vector.w);
	}
	static void Transform(const glm::mat4 &matrix, glm::vec4 &vector) {
		vector = matrix * vector;
	}

	template<typename T, typename... Args>
	static void Transform(const glm::mat4 &matrix, T &first, Args&... args) {
		Transform(matrix, first);
		Transform(matrix, args...);
	}
}

