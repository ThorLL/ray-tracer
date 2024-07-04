#pragma once
#include <glm/ext/matrix_float4x4.hpp>

#include "Camera.h"

static void transformVec3(const glm::mat4 &matrix, glm::vec3 &vector) {
	const auto _vector = matrix * glm::vec4(vector, 1.0f);
	vector = glm::vec3(_vector / _vector.w);
}

struct Sphere : ShaderStruct
{
	Sphere(const glm::vec3 &center, float radius, int material_index)
		: center(center),
		  radius(radius),
		  materialIndex(material_index) {
	}

	glm::vec3 center;
	float radius;
	int materialIndex;

	[[nodiscard]] Sphere Transform(const glm::mat4 &matrix) const {
		Sphere sphere = *this;
		transformVec3(matrix, sphere.center);
		return sphere;
	}

	[[nodiscard]] std::vector<std::byte> GetBytes() override {
		return ConvertToBytes(center, radius, materialIndex);
	}
};

struct Triangle final : ShaderStruct
{
	Triangle(const glm::vec3 &pos_a, const glm::vec3 &pos_b, const glm::vec3 &pos_c, const glm::vec3 &normal_a,
		const glm::vec3 &normal_b, const glm::vec3 &normal_c)
		: posA(pos_a),
		  posB(pos_b),
		  posC(pos_c),
		  normalA(normal_a),
		  normalB(normal_b),
		  normalC(normal_c) {
	}

	glm::vec3 posA, posB, posC;
	glm::vec3 normalA, normalB, normalC;

	[[nodiscard]] Triangle Transform(const glm::mat4 &matrix) const {
		Triangle triangle = *this;
		transformVec3(matrix, triangle.posA);
		transformVec3(matrix, triangle.posB);
		transformVec3(matrix, triangle.posC);
		return triangle;
	}

	[[nodiscard]] std::vector<std::byte> GetBytes() override {
		return ConvertToBytes(posA, posB, posC, normalA, normalB, normalC);
	}
};

struct Material final : ShaderStruct
{
	Material(const glm::vec3 &albedo, const glm::vec3 &emission_color, float strength, float roughness, float metallic,
		float ior, std::string name, int index)
		: albedo(albedo),
		  emissionColor(emission_color),
		  strength(strength),
		  roughness(roughness),
		  metallic(metallic),
		  ior(ior),
	      name(name),
		  index(index)
	{
	}

	glm::vec3 albedo{}, emissionColor{};
	float strength{}, roughness{}, metallic{}, ior{};
	std::string name;
	int index;
	[[nodiscard]] std::vector<std::byte> GetBytes() override {
		return ConvertToBytes(albedo, emissionColor, strength, roughness, metallic, ior);
	}
};