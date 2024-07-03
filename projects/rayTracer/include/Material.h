#pragma once
#include <glm/vec3.hpp>

struct MaterialData {
	glm::vec4 albedo{}, emissionColor{};
	float strength{}, roughness{}, metallic{}, ior{};
};

struct Material
{
	glm::vec3 albedo{}, emissionColor{};
	float strength{}, roughness{}, metallic{}, ior{};

	MaterialData GetData(){
		return {
			{albedo, 0.0f},
			{emissionColor, 0.0f},
			strength,roughness, metallic, ior
		};
	}
};
