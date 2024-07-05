//Inputs
in vec2 TexCoord;

//Outputs
out vec4 FragColor;

// --- Structs ---
struct Ray
{
	vec3 origin;
	vec3 direction;
	float ior;
};

struct Material
{
	vec3 albedo; float albedo_pad;
	vec3 emissionColor;  float emissionColor_pad;
	float strength;
	float roughness;
	float metallic;
	float ior;
};

struct HitInfo
{
	bool didHit;
	float dst;
	vec3 hitPoint;
	vec3 normal;
	int materialIndex;
};

struct Sphere
{
	vec3 center; float center_pad;
	float radius;
	int materialIndex;
};

struct Triangle
{
	vec3 posA;
	vec3 posB;
	vec3 posC;
	vec3 normalA;
	vec3 normalB;
	vec3 normalC;
};

struct MeshInfo
{
	int firstTriangleIndex;
	int nTriangle;
	int materialIndex;
	bool visible;
};

// --- Uniforms ---
// Camera uniforms
uniform mat4 InvProjMatrix;
// Raytracing uniforms
uniform int NumRaysPerPixel;
uniform int RayCapacity;

// Shader Storage Buffer Objects (ssbo)
layout(std430, binding = 1) buffer SphereBuffer {
	Sphere spheres[];
};

layout(std430, binding = 2) buffer TrianglesBuffer {
	Triangle triangles[];
};

layout(std430, binding = 3) buffer MeshInfoBuffer {
	MeshInfo meshInfos[];
};

layout(std430, binding = 4) buffer MaterialBuffer {
	Material materials[];
};

vec3 GetImplicitNormal(vec2 normal)
{
	float z = sqrt(1.0f - normal.x * normal.x - normal.y * normal.y);
	return vec3(normal, z);
}

// --- Ray Intersection Functions ---
// Calculate the intersection of a ray with a sphere
HitInfo RaySphereIntersection(Ray ray, Sphere sphere)
{
	HitInfo hitInfo;
	hitInfo.didHit = false;

	vec3 offsetRayOrigin = ray.origin - sphere.center;
	// From the equation: sqrLength(rayOrigin + rayDir * dst) = radius^2
	// Solving for dst results in a quadratic equation with coefficients:
	float a = dot(ray.direction, ray.direction); // a = 1 (assuming unit vector)
	float b = 2 * dot(offsetRayOrigin, ray.direction);
	float c = dot(offsetRayOrigin, offsetRayOrigin) - sphere.radius * sphere.radius;
	// Quadratic discriminant
	float discriminant = b * b - 4 * a * c;

	// No solution when d < 0 (ray misses sphere)
	if (discriminant >= 0) {
		// Distance to nearest intersection point (from quadratic formula)
		float dst = (-b - sqrt(discriminant)) / (2 * a);

		// Ignore intersections that occur behind the ray
		if (dst >= 0) {
			hitInfo.didHit = true;
			hitInfo.dst = dst;
			hitInfo.hitPoint = ray.origin + ray.direction * dst;
			hitInfo.normal = normalize(hitInfo.hitPoint - sphere.center);
		}
	}
	return hitInfo;
}

// Calculate the intersection of a ray with a triangle using Möller–Trumbore algorithm
HitInfo RayTriangleIntersection(Ray ray, Triangle tri)
{
	vec3 edgeAB = tri.posB - tri.posA;
	vec3 edgeAC = tri.posC - tri.posA;
	vec3 normalVector = cross(edgeAB, edgeAC);
	vec3 ao = ray.origin - tri.posA;
	vec3 dao = cross(ao, ray.direction);

	float determinant = -dot(ray.direction, normalVector);
	float invDet = 1 / determinant;

	// Calculate dst to triangle & barycentric coordinates of intersection point
	float dst = dot(ao, normalVector) * invDet;
	float u = dot(edgeAC, dao) * invDet;
	float v = -dot(edgeAB, dao) * invDet;
	float w = 1 - u - v;

	// Initialize hit info
	HitInfo hitInfo;
	hitInfo.didHit = determinant >= 1E-6 && dst >= 0 && u >= 0 && v >= 0 && w >= 0;
	hitInfo.hitPoint = ray.origin + ray.direction * dst;
	hitInfo.normal = normalize(tri.normalA * w + tri.normalB * u + tri.normalC * v);
	hitInfo.dst = dst;
	return hitInfo;
}

// Find the first point that the given ray collides with, and return hit info
HitInfo CollisionDetection(Ray ray)
{
	HitInfo closestHit;
	closestHit.didHit = false;
	closestHit.dst = 1.0f/0.0f; // 'closest' hit is infinitely far away

	// check against spheres
	for(int sphereIndex = 0; sphereIndex < spheres.length(); sphereIndex++){
		Sphere sphere = spheres[sphereIndex];
		HitInfo hitInfo = RaySphereIntersection(ray, sphere);

		if (hitInfo.didHit && hitInfo.dst < closestHit.dst)
		{
			closestHit = hitInfo;
			closestHit.materialIndex = int(sphere.materialIndex);
		}
	}

	//check against meshes
	for (int meshIndex = 0; meshIndex < meshInfos.length(); meshIndex ++)
	{
		MeshInfo meshInfo = meshInfos[meshIndex];
		if (!meshInfo.visible)
			continue;

		for (uint i = 0u; i < meshInfo.nTriangle; i ++) {
			uint triIndex = meshInfo.firstTriangleIndex + i;
			Triangle tri = triangles[triIndex];
			HitInfo hitInfo = RayTriangleIntersection(ray, tri);

			if (hitInfo.didHit && hitInfo.dst < closestHit.dst)
			{
				closestHit = hitInfo;
				closestHit.materialIndex = meshInfo.materialIndex;
			}
		}

	}
	return closestHit;
}


vec3 CastRay(Ray ray)
{
	vec3 incomingLight = vec3(0);
	vec3 rayColor = vec3(1);
	for (int bounceIndex = 0; bounceIndex < RayCapacity; ++bounceIndex)
	{
		HitInfo hitinfo = CollisionDetection(ray);
		if (!hitinfo.didHit){
			break;
		}

		// move ray to new position
		ray.origin = hitinfo.hitPoint;
		ray.origin += 0.00001f * ray.direction;

		// extract material
		Material material = materials[hitinfo.materialIndex];

		// lighting
		vec3 emittedLight = material.emissionColor * material.strength;
		incomingLight += emittedLight * rayColor;

		vec3 normal = hitinfo.normal;
		vec3 F0 = mix(vec3(0.04f), material.albedo, material.metallic);
		vec3 fresnel = F0 + (vec3(1.0f) - F0) * pow(1.0f - max(dot(-ray.direction, normal), 0), 5.0f);
		bool isTransparent = material.ior != 0.0f;
		bool isExit = ray.ior != 1.0f;

		// determine next ray's bounce type (refract, diffuse or specular)
		bool isSpecularBounce = material.metallic > RandomValue();

		// specular and diffusing directions
		vec3 diffuseDir= normalize(normal + GetRandomDirection());

		// cast specular light
		if (isSpecularBounce){
			vec3 specularDir = reflect(ray.direction, normal);
			ray.direction = normalize(mix(specularDir, diffuseDir, material.roughness * material.roughness));
			rayColor *= fresnel;
		}
		else
		{
			// --- diffused light ---
			// determine refaction
			float ior = mix(1.0f, material.ior, isTransparent && !isExit);
			vec3 refractedDir = refract(ray.direction, normal, ray.ior / ior);

			ray.direction = isTransparent ? refractedDir : diffuseDir;
			if (!isExit){
				rayColor *= mix(material.albedo, vec3(0), material.metallic);
			}
			rayColor *= (1.0f - fresnel);
			ray.ior = ior;
		}
		// Random early exit if ray colour is nearly 0 (can't contribute much to final result)
		float p = max(rayColor.r, max(rayColor.g, rayColor.b));
		if (RandomValue() >= p) {
			break;
		}
		rayColor *= 1.0f / p;
	}
	return incomingLight;
}

void main()
{
	InitRandomSeed();

	// Start from transformed position
	vec4 viewPos = InvProjMatrix * vec4(TexCoord.xy * 2.0f - 1.0f, 0.0f, 1.0f);

	Ray ray;
	vec3 totalIncomingLight = vec3(0);
	float Focus = 100.0f;
	for (int rayIndex = 0; rayIndex < NumRaysPerPixel; rayIndex++)
	{
		ray.ior = 1.0f;

		// Calculate ray origin and direction
		vec3 defocusJitter = GetRandomDirection() * (1 - 0.01f * Focus);
		ray.origin = viewPos.xyz + vec3(1,0,0) * defocusJitter.x + vec3(0,1,0) * defocusJitter.y;
		ray.direction = normalize(ray.origin);

		// Cast Ray
		totalIncomingLight += CastRay(ray);
	}

	// Raytrace the scene
	vec3 color = totalIncomingLight / NumRaysPerPixel;
	float alpha = 1.0f / float(FrameCount);
	FragColor = vec4(color, alpha);
}