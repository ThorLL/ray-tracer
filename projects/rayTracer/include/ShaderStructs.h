#pragma once
#include <glm/ext/matrix_float4x4.hpp>
#include <utility>


#include "GeometryObject.h"

struct Sphere final : ShaderStruct, Geometry::GeometryObject
{
	Sphere(const glm::vec3 &center, float radius, int material_index)
		: center(center),
		  radius(radius),
		  materialIndex(material_index){
	}

	glm::vec3 center;
	float radius;
	int materialIndex;


	[[nodiscard]] std::vector<std::byte> GetBytes() override {
		return ConvertToBytes(center, radius, materialIndex);
	}

	void Transform(const glm::mat4 &matrix) override {
		Geometry::Transform(matrix, center);
	}
};

struct Triangle final : ShaderStruct, Geometry::GeometryObject
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

	void Transform(const glm::mat4 &matrix) override {
		Geometry::Transform(matrix, posA, posB, posC);
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
	      name(std::move(name)),
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