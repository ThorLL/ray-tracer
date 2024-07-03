#include <chrono>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <numbers>
#include <span>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "../include/Mesh.h"
#include "../include/json.hpp"
#include "../include/Material.h"

struct MyTransform {
	glm::vec3 translation{};
	glm::vec3 rotation{};
	glm::vec3 scale{};

	// Cached matrix
	mutable glm::mat4 matrix{};
	mutable bool dirty{};

	std::shared_ptr<MyTransform> parent;

	glm::mat4 GetRotationMatrix() const {
		auto matrix = glm::identity<glm::mat4>();
		matrix = rotate(matrix, rotation.y, glm::vec3(0, 1, 0));
		matrix = rotate(matrix, rotation.x, glm::vec3(1, 0, 0));
		matrix = rotate(matrix, rotation.z, glm::vec3(0, 0, 1));
		return matrix;
	}
};

struct MyCamera {
	MyCamera() : viewMatrix(1), projMatrix(1){}
	MyCamera(const glm::mat4 &view, const glm::mat4 &proj) :
		viewMatrix(view),
		projMatrix(proj),
		transform{
			ExtractTranslation(),
			ExtractRotation(),
			ExtractScale(),
			glm::mat4(1),
			false
		}
	{}
	glm::mat4 viewMatrix, projMatrix;
	MyTransform transform;
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

// program info/pramas
int screenWidth = 1920;
int screenHeight = 1080;
float aspectRatio = static_cast<float>(screenWidth) / static_cast<float>(screenHeight);
unsigned int frameCount = 0;

int numberOfRays = 2;
int numberOfbounches = 16;

// camera params
bool cameraEnabled = false;
MyCamera camera;
glm::vec2 lastMousePosition;

// uniform locations
GLint raysLocation;
GLint bounchesLocation;
GLint invProjMatrixLocation;
GLint frameCountLocation;
GLint sourceTextureLocation;
GLuint screenTexture;

GLint screenTexturePtr;
// scene data
std::vector<Sphere> spheres{};
std::vector<Triangle> triangles{};
std::vector<Mesh> meshes{};
std::vector<std::string> meshNames{};
std::vector<Material> materials{};
std::vector<std::string> materialNames{};

// Buffers
GLuint VertexBufferObject;
GLuint VertexArrayObject;
GLuint sphereBuffer;
GLuint TrianglesBuffer;
GLuint MeshBuffer;
GLuint MaterialBuffer;
GLuint screenFramebuffer;
// shader programs
GLuint shaderProgram;
GLuint copyShaderProgram;


GLFWwindow* window = nullptr;

void FrameBufferResized(GLFWwindow* window, int width, int height)
{
	glViewport(0,0, width, height);
	screenWidth = width;
	screenHeight = height;
	aspectRatio = static_cast<float>(width) / static_cast<float>(height);
}

void LoadShader(const GLuint shader, const std::vector<const char*> &paths)
{
	std::vector<std::stringstream> stringStreams(paths.size());
	std::vector<std::string> sourceCodeStrings(paths.size());
	std::vector<const char*> sourceCode(paths.size());
	for (int i = 0; i < paths.size(); ++i)
	{
		std::ifstream file(paths[i]);
		assert(file.is_open());
		stringStreams[i] << file.rdbuf() << '\0';
		sourceCodeStrings[i] = stringStreams[i].str();
		sourceCode[i] = sourceCodeStrings[i].c_str();
	}
	glShaderSource(shader, static_cast<int>(sourceCode.size()), sourceCode.data(), nullptr);
	glCompileShader(shader);
	GLint success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	assert(success);
}

void LoadMaterials(const char* filePath) {
	std::ifstream f(filePath);
	nlohmann::json data = nlohmann::json::parse(f);

	for (auto material : data)
	{
		materialNames.push_back(material["name"].get<std::string>());
		materials.push_back(
			{
				{
					material["albedo"][0].get<float>(),
					material["albedo"][1].get<float>(),
					material["albedo"][2].get<float>()
				},
				{
					material["emissionColor"][0].get<float>(),
					material["emissionColor"][1].get<float>(),
					material["emissionColor"][2].get<float>()
				},
				material["emissionStrength"].get<float>(),
				material["roughness"].get<float>(),
				material["metallic"].get<float>(),
				material["ior"].get<float>()
			}
		);
	}
}

void Update(const float deltaTime) {
	// Update camera controller
	static bool enablePressed = false;
	bool _enablePressed = glfwGetKey(window, GLFW_KEY_SPACE);
	if (_enablePressed && !enablePressed) {
		cameraEnabled = !cameraEnabled;
		glfwSetInputMode(window, GLFW_CURSOR, cameraEnabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
	}
	enablePressed = _enablePressed;

	if (cameraEnabled)
	{
		double x, y;
		glfwGetCursorPos(window, &x, &y);

		glm::vec2 mousePosition{
			static_cast<float>(x) / static_cast<float>(screenWidth) * 2.0f - 1.0f,
			static_cast<float>(y) / static_cast<float>(-screenHeight) * 2.0f + 1.0f
		};
		// Update translation
		glm::vec2 inputTranslation(0.0f);

		if (glfwGetKey(window, GLFW_KEY_A))
			inputTranslation.x = -1.0f;
		else if (glfwGetKey(window, GLFW_KEY_D))
			inputTranslation.x = 1.0f;

		if (glfwGetKey(window, GLFW_KEY_W))
			inputTranslation.y = -1.0f;
		else if (glfwGetKey(window, GLFW_KEY_S))
			inputTranslation.y = 1.0f;

		inputTranslation *= 2 * deltaTime;
		if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT))
			inputTranslation *= 2.0f;

		glm::vec3 right, up, forward;
		camera.ExtractVectors(right, up, forward);
		camera.transform.translation += inputTranslation.x * right;
		camera.transform.translation += inputTranslation.y * forward;

		// Update rotation
		const glm::vec2 deltaMousePosition = mousePosition - lastMousePosition;
		lastMousePosition = mousePosition;

		const glm::vec3 inputRotation(deltaMousePosition.y, -deltaMousePosition.x, 0.0f);

		camera.transform.rotation += inputRotation * 2.0f;

		const glm::mat4 matrix = transpose(camera.transform.GetRotationMatrix());
		camera.viewMatrix = translate(matrix, -camera.transform.translation);

		frameCount = 0;
	}
	std::vector _spheres(spheres);
	for (auto sphere: _spheres) sphere.Transform(camera.viewMatrix);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, sphereBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, _spheres.size() * sizeof(Sphere), _spheres.data(), GL_STATIC_READ);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, sphereBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	std::vector<TriangleData> _triangles;
	for (auto triangle: triangles) _triangles.push_back(triangle.Transform(camera.viewMatrix).GetData());

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, TrianglesBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, _triangles.size() * sizeof(TriangleData), _triangles.data(), GL_STATIC_READ);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, TrianglesBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, MeshBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, meshes.size() * sizeof(Mesh), meshes.data(), GL_STATIC_READ);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, MeshBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	std::vector<MaterialData> _materials;
	for (auto material: materials) _materials.push_back(material.GetData());

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, MaterialBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, _materials.size() * sizeof(MaterialData), _materials.data(), GL_STATIC_READ);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, MaterialBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void initialise() {
	// Init GLFW
	glfwInit();
	// Create window
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	window = glfwCreateWindow(1920, 1080, "Ray Tracer", nullptr, nullptr);
	if (!window) {
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		exit(-1);
	}
	glfwMakeContextCurrent(window);

	gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress));
	glfwSetFramebufferSizeCallback(window, FrameBufferResized);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	// gen buffers
	glGenBuffers(1, &VertexBufferObject);
	glGenVertexArrays(1, &VertexArrayObject);
	glGenBuffers(1, &sphereBuffer);
	glGenBuffers(1, &TrianglesBuffer);
	glGenBuffers(1, &MeshBuffer);
	glGenBuffers(1, &MaterialBuffer);

	glBindBuffer(GL_ARRAY_BUFFER, VertexBufferObject);
	glBindVertexArray(VertexArrayObject);

	const std::vector<glm::vec3> fullscreenVertices {
					{-1.0f, -1.0f, 0.0f},
					{3.0f, -1.0f, 0.0f},
					{-1.0f, 3.0f, 0.0f}
	};

	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * fullscreenVertices.size(), fullscreenVertices.data(), GL_STATIC_DRAW);

	constexpr GLuint location = 0;
	glVertexAttribPointer(location, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	glEnableVertexAttribArray(location);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	glEnable(GL_FRAMEBUFFER_SRGB);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	glfwSwapInterval(1);

	// Init ImGUI
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 430 core");

	// Init camera
	camera = MyCamera{
		lookAt(glm::vec3(0.0f, 2.75f, 5.0f),glm::vec3(0.0f,3.0f,1.0f),glm::vec3(0.0f, 1.0f, 0.0f)),
		glm::perspective(1.5f, aspectRatio, 0.1f, 500.0f)
	};
}

void loadResources() {
	LoadMaterials("resources/materials/materials.json");

	// Creater shader
	shaderProgram = glCreateProgram();
	const GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	LoadShader(vertexShader,{
		"resources/shaders/version430.glsl",
		"resources/shaders/renderer/fullscreen.vert"
	});

	LoadShader(fragmentShader, {
		"resources/shaders/version430.glsl",
		"resources/shaders/random.glsl",
		"resources/shaders/raytracing.frag"
	});

	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);

	// get uniforms from shader
	raysLocation = glGetUniformLocation(shaderProgram, "NumRaysPerPixel");
	bounchesLocation = glGetUniformLocation(shaderProgram, "RayCapacity");
	invProjMatrixLocation = glGetUniformLocation(shaderProgram, "InvProjMatrix");
	frameCountLocation = glGetUniformLocation(shaderProgram, "FrameCount");

	// only delete fragment shader as we'll reuse the vertex shader later
	glDeleteShader(fragmentShader);

	// Init Framebuffer
	glGenTextures(1, &screenTexture);
	glGenFramebuffers(1, &screenFramebuffer);
	glBindTexture(GL_TEXTURE_2D, screenTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, screenWidth, screenHeight, 0, GL_RGBA, GL_FLOAT, std::span<float>().data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	// Scene framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, screenFramebuffer);
	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, screenTexture, 0);
	constexpr GLenum attachments[] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, attachments);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Create copy shader
	copyShaderProgram = glCreateProgram();
	fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	LoadShader(fragmentShader, {
		"resources/shaders/version430.glsl",
		"resources/shaders/renderer/copy.frag"
	});

	// reusing vertex shader
	glAttachShader(copyShaderProgram, vertexShader);
	glAttachShader(copyShaderProgram, fragmentShader);
	glLinkProgram(copyShaderProgram);

	sourceTextureLocation = glGetUniformLocation(copyShaderProgram, "SourceTexture");

	glDeleteShader(fragmentShader);
	glDeleteShader(vertexShader);

	// Load meshes
	const char* meshPaths[] = {
		"resources/models/WhiteRoom.obj"
	};
	for (const auto path: meshPaths) {
		for (auto &[name, mesh] : loadMesh(path, &triangles)) {
			meshNames.emplace_back(name);
			meshes.push_back(mesh);
		}
	}
}

void RenderGUI() {
	// Render the debug user interface
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	bool changed = false;
	ImGui::Begin("Ray tracing");
	changed |= ImGui::DragInt("Rays per Pixel", &numberOfRays, 1, 0);
	changed |= ImGui::DragInt("Bounces", &numberOfbounches, 1, 0);
	ImGui::End();
	ImGui::Begin("Materials");
	for (int i = 0; i < materials.size(); i++) {
		if(ImGui::TreeNodeEx((materialNames[i] + std::to_string(i)).c_str(), ImGuiTreeNodeFlags_OpenOnDoubleClick)) {
			auto &[albedo, emissionColor, strength, roughness, metallic, ior] = materials[i];
			changed |= ImGui::ColorEdit3("Color", &albedo[0]);
			changed |= ImGui::ColorEdit3("Emission Color", &emissionColor[0]);
			changed |= ImGui::DragFloat("Emission Strength", &strength, 0.05f, 0, 100);
			changed |= ImGui::DragFloat("Roughness", &roughness, 0.05f, 0, 1);
			changed |= ImGui::DragFloat("Metallic", &metallic, 0.05f, 0, 1);
			changed |= ImGui::DragFloat("IOR", &ior, 0.05f, 0);
			ImGui::TreePop();
		}
	}
	ImGui::End();
	ImGui::Begin("Spheres");
	for (int i = 0; i < spheres.size(); i++) {
		if(ImGui::TreeNodeEx(("Sphere" + std::to_string(i+1)).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			changed |= ImGui::DragFloat3("Center", &spheres[i].center[0], .1f);
			changed |= ImGui::DragFloat("Radius", &spheres[i].radius, .1f, 0);
			changed |= ImGui::DragInt("Material index", &spheres[i].materialIndex,  1, 0, static_cast<int>(materials.size()) -1);
			ImGui::TreePop();
		}
	}
	ImGui::End();
	ImGui::Begin("Meshes");
	for (int i = 0; i < meshes.size(); i++) {
		if(ImGui::TreeNodeEx(meshNames[i].c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
			changed |= ImGui::Checkbox("Hide", &meshes[i].hide);
			changed |= ImGui::DragInt("Material index", &meshes[i].materialIndex, 1, 0, static_cast<int>(materials.size()) -1);
			ImGui::TreePop();
		}
	}
	ImGui::End();

	if (changed)
		frameCount = 0;

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

int main() {
	initialise();
	loadResources();
	// Start Program
	const auto startTime = std::chrono::steady_clock::now();
	float currentTime = 0;
	// Main loop
	while (window != nullptr && !glfwWindowShouldClose(window))
	{
		// set current time relative to start time
		std::chrono::duration<float> duration = std::chrono::steady_clock::now() - startTime;

		// Update
		if (glfwGetKey(window, GLFW_KEY_ESCAPE)) {
			glfwSetWindowShouldClose(window, GL_TRUE);
			return 0;
		}

		Update(duration.count() - currentTime);
		currentTime = duration.count();

		// Render
		GLbitfield mask = 0;

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		mask |= GL_COLOR_BUFFER_BIT;

		glClearDepth(1.0f);
		mask |= GL_DEPTH_BUFFER_BIT;

		glClear(mask);

		glBindFramebuffer(GL_FRAMEBUFFER, screenFramebuffer);
		glUseProgram(shaderProgram);

		glUniform1uiv(frameCountLocation, 1, &++frameCount);
		glUniformMatrix4fv(invProjMatrixLocation, 1, false, &inverse(camera.projMatrix)[0][0]);
		glUniform1iv(raysLocation, 1, &numberOfRays);
		glUniform1iv(bounchesLocation, 1, &numberOfbounches);

		// Set depth test
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
		glStencilFunc(GL_NEVER, 0, UINT_MAX);
		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glBindVertexArray(VertexArrayObject);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glUseProgram(copyShaderProgram);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, screenTexture);
		glUniform1iv(sourceTextureLocation, 1, &screenTexturePtr);

		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
		glStencilFunc(GL_NEVER, 0, UINT_MAX);
		glDisable(GL_BLEND);

		glBindVertexArray(VertexArrayObject);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		RenderGUI();

		// Swap buffers and poll events at the end of the frame
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
