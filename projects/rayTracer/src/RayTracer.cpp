#include <chrono>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <span>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "../include/Mesh.h"
#include "../include/json.hpp"
#include "../include/Camera.h"
#include "../include/SSBO.h"


// program info/pramas
int screenWidth = 1920;
int screenHeight = 1080;
float aspectRatio = static_cast<float>(screenWidth) / static_cast<float>(screenHeight);
unsigned int frameCount = 0;

int numberOfRays = 1;
int numberOfbounches = 8;

// camera params
bool cameraEnabled = false;
Camera camera;
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
std::vector<std::shared_ptr<Sphere>> spheres{};
std::vector<std::shared_ptr<Triangle>> triangles{};
std::vector<std::shared_ptr<Mesh>> meshes{};
std::vector<std::shared_ptr<Material>> materials{};

// Buffers
GLuint VertexBufferObject;
GLuint VertexArrayObject;
GLuint screenFramebuffer;
// shader programs
GLuint shaderProgram;
GLuint copyShaderProgram;

std::optional<SSBO> SphereSSBO;
std::optional<SSBO> TriangleSSBO;
std::optional<SSBO> MeshSSBO;
std::optional<SSBO> MaterialSSBO;

constexpr unsigned short SPHERES = 1;
constexpr unsigned short TRIANGLES = 2;
constexpr unsigned short MESHES = 4;
constexpr unsigned short MATERIALS = 8;
constexpr unsigned short SYSTEM = 16;
constexpr unsigned short CAMERA = 32 + TRIANGLES + SPHERES;

std::vector<unsigned short> ChangesBuffer{};

std::vector<float> frameTimes{};

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
	int idx = 0;
	for (auto material : data)
	{
		materials.push_back(std::make_shared<Material>(Material{
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
			material["ior"].get<float>(),
			material["name"].get<std::string>(),
			idx++
		}));
	}
}

void Update(const float deltaTime) {
	// Update camera controller
	static bool enablePressed = false;
	bool _enablePressed = glfwGetKey(window, GLFW_KEY_SPACE);
	if (_enablePressed && !enablePressed) {
		cameraEnabled = !cameraEnabled;
		glfwSetInputMode(window, GLFW_CURSOR, !cameraEnabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
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
		ChangesBuffer.push_back(CAMERA);
	}
}

void initialise() {
	// Init GLFW
	glfwInit();
	// Create window
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
	window = glfwCreateWindow(screenWidth, screenHeight, "Ray Tracer", nullptr, nullptr);
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

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->AddFontFromFileTTF("resources/myFont.ttf", 18.0f);

	// Init ImGUI
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 430 core");

	// gen buffers
	glGenBuffers(1, &VertexBufferObject);
	glGenVertexArrays(1, &VertexArrayObject);
	SphereSSBO.emplace(1);
	TriangleSSBO.emplace(2);
	MeshSSBO.emplace(3);
	MaterialSSBO.emplace(4);

	glBindBuffer(GL_ARRAY_BUFFER, VertexBufferObject);

	const std::vector<glm::vec3> fullscreenVertices {
					{-1.0f, -1.0f, 0.0f},
					{3.0f, -1.0f, 0.0f},
					{-1.0f, 3.0f, 0.0f}
	};

	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * fullscreenVertices.size(), fullscreenVertices.data(), GL_STATIC_DRAW);
	glBindVertexArray(VertexArrayObject);
	constexpr GLuint location = 0;
	glVertexAttribPointer(location, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	glEnableVertexAttribArray(location);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	glEnable(GL_FRAMEBUFFER_SRGB);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	glfwSwapInterval(0);



	// Init camera
	camera = Camera{
		lookAt(glm::vec3(0.0f, 1.1f, 2.5f),glm::vec3(0.0f,1.0f,-1.0f),glm::vec3(0.0f, 1.0f, 0.0f)),
		glm::perspective(1.3f, aspectRatio, 0.1f, 500.0f)
	};
}

void loadResources() {
	LoadMaterials("resources/materials/materials.json");

	// Creater shader
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	LoadShader(vertexShader,{
		"resources/shaders/version430.glsl",
		"resources/shaders/renderer/fullscreen.vert"
	});

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	LoadShader(fragmentShader, {
		"resources/shaders/version430.glsl",
		"resources/shaders/random.glsl",
		"resources/shaders/raytracing.frag"
	});

	shaderProgram = glCreateProgram();
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
	glBindTexture(GL_TEXTURE_2D, screenTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, screenWidth, screenHeight, 0, GL_RGBA, GL_FLOAT, std::span<float>().data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	// Scene framebuffer
	glGenFramebuffers(1, &screenFramebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, screenFramebuffer);
	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, screenTexture, 0);
	constexpr GLenum attachments[] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, attachments);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Create copy shader
	fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	LoadShader(fragmentShader, {
		"resources/shaders/version430.glsl",
		"resources/shaders/renderer/copy.frag"
	});

	// reusing vertex shader
	copyShaderProgram = glCreateProgram();
	glAttachShader(copyShaderProgram, vertexShader);
	glAttachShader(copyShaderProgram, fragmentShader);
	glLinkProgram(copyShaderProgram);

	sourceTextureLocation = glGetUniformLocation(copyShaderProgram, "SourceTexture");

	glDeleteShader(fragmentShader);
	glDeleteShader(vertexShader);

	// Load meshes
	const char* meshPaths[] = {
		"resources/models/CornellBox.obj"
	};
	for (const auto path: meshPaths)
		for (const auto& mesh : loadMesh(path, &triangles))
			meshes.push_back(mesh);

	constexpr int indices[]{5, 4, 0, 3, 2, 4, 1, 7, 8};
	for (int i = 0; i < meshes.size(); ++i) {
		meshes[i]->materialIndex = indices[i];
	}

	spheres.push_back(std::make_shared<Sphere>(glm::vec3(0.5f, 1.0f, -0.2f), 0.4f, 6));
}

bool MaterialDropDown(int &materialIndex) {
	std::vector<const char*> materialNames;
	for (const auto &material : materials)
		materialNames.push_back(material->name.c_str());

	if (ImGui::Combo("Material", &materialIndex, materialNames.data(), static_cast<int>(materialNames.size())))
		return true;
	return false;
}

void ShowMatricies() {
	ImGui::Begin("Matricies");
	std::vector sorted(frameTimes);
	const float currentTime = frameTimes[frameTimes.size()-1];
	std::ranges::sort(sorted);
	std::ranges::reverse(sorted);

	auto idx1 = static_cast<size_t>(sorted.size() * 0.01);
	float ms99 = 0.0f;
	float ms1 = 0.0f;

	for (int i = 0; i < sorted.size(); i++)
		if (i < idx1) ms1 += sorted[i];
		else ms99 += sorted[i];
	ms99 /= std::max(static_cast<int>(sorted.size() - idx1), 1);
	ms1 /= std::max(static_cast<int>(idx1), 1);
	std::ostringstream current, p99, p1;
	current << "fps: " << static_cast<int>(1.0f / currentTime) << "(" << std::fixed << std::setprecision(2) << currentTime * 1000.0f << "ms)";
	p99     << "99 : " << static_cast<int>(1.0f / ms99)        << "(" << std::fixed << std::setprecision(2) << ms99 * 1000.0f << "ms)";
	p1      << "1  : " << static_cast<int>(1.0f / ms1)         << "(" << std::fixed << std::setprecision(2) << ms1 * 1000.0f << "ms)";

	ImGui::Text(current.str().c_str());
	ImGui::Text(p99.str().c_str());
	ImGui::Text(p1.str().c_str());
	ImGui::End();
}

void RenderGUI() {
	// Render the debug user interface
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	bool systemhanges = false;
	bool materialChanges = false;
	bool sphereChanges = false;
	bool meshChanges = false;

	ShowMatricies();

	ImGui::Begin("Ray tracing");
	systemhanges |= ImGui::DragInt("Rays per Pixel", &numberOfRays, 1, 0);
	systemhanges |= ImGui::DragInt("Bounces", &numberOfbounches, 1, 0);
	ImGui::End();
	ImGui::Begin("Materials");
	for (const auto &material : materials) {
		if(ImGui::TreeNodeEx(material->name.c_str(), ImGuiTreeNodeFlags_OpenOnDoubleClick)) {
			materialChanges |= ImGui::ColorEdit3("Color", &material->albedo[0]);
			materialChanges |= ImGui::ColorEdit3("Emission Color", &material->emissionColor[0]);
			materialChanges |= ImGui::DragFloat("Emission Strength", &material->strength, 0.05f, 0, 100);
			materialChanges |= ImGui::DragFloat("Roughness", &material->roughness, 0.05f, 0, 1);
			materialChanges |= ImGui::DragFloat("Metallic", &material->metallic, 0.05f, 0, 1);
			materialChanges |= ImGui::DragFloat("IOR", &material->ior, 0.05f, 0);
			ImGui::TreePop();
		}
	}
	ImGui::End();
	ImGui::Begin("Spheres");
	for (const auto &sphere : spheres) {
		if(ImGui::TreeNodeEx("Sphere", ImGuiTreeNodeFlags_DefaultOpen))
		{
			sphereChanges |= ImGui::DragFloat3("Center", &sphere->center[0], .1f);
			sphereChanges |= ImGui::DragFloat("Radius", &sphere->radius, .1f, 0);
			sphereChanges |= MaterialDropDown(sphere->materialIndex);
			ImGui::TreePop();
		}
	}
	ImGui::End();
	ImGui::Begin("Meshes");
	for (const auto &mesh : meshes) {
		if (ImGui::TreeNodeEx(mesh->name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
			meshChanges |= ImGui::Checkbox("Hide", &mesh->visible);
			meshChanges |= MaterialDropDown(mesh->materialIndex);
			ImGui::TreePop();
		}
	}
	ImGui::End();

	if (systemhanges) ChangesBuffer.push_back(SYSTEM);
	if (materialChanges) ChangesBuffer.push_back(MATERIALS);
	if (sphereChanges) ChangesBuffer.push_back(SPHERES);
	if (meshChanges) ChangesBuffer.push_back(MESHES);

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void HandleChanges() {
	if(ChangesBuffer.empty()) return;
	const unsigned short change = std::accumulate(std::begin(ChangesBuffer), std::end(ChangesBuffer), 0);

	if (change & SPHERES) {
		std::vector<std::shared_ptr<ShaderStruct>> _spheres;
		for (const auto &sphere: spheres)
			_spheres.push_back(std::make_shared<Sphere>(sphere->Transform(camera.viewMatrix)));
		SphereSSBO->BufferData(_spheres);
	}
	if (change & TRIANGLES) {
		std::vector<std::shared_ptr<ShaderStruct>> _triangles;
		for (const auto &triangle: triangles)
			_triangles.push_back(std::make_shared<Triangle>(triangle->Transform(camera.viewMatrix)));
		TriangleSSBO->BufferData(_triangles);
	}
	if (change & MESHES) {
		std::vector<std::shared_ptr<ShaderStruct>> _meshes;
		for (const auto &mesh: meshes)_meshes.push_back(mesh);
		MeshSSBO->BufferData(_meshes);
	}
	if (change & MATERIALS) {
		std::vector<std::shared_ptr<ShaderStruct>> _materials;
		for (const auto &material: materials)_materials.push_back(material);
		MaterialSSBO->BufferData(_materials);
	}
	if (change & SYSTEM) {
		glUniform1iv(raysLocation, 1, &numberOfRays);
		glUniform1iv(bounchesLocation, 1, &numberOfbounches);
	}
	if (change & CAMERA) {
		glUniformMatrix4fv(invProjMatrixLocation, 1, false, &inverse(camera.projMatrix)[0][0]);
	}
	ChangesBuffer.clear();
	frameCount = 0;
}

int main() {
	initialise();
	loadResources();
	// Start Program
	const auto startTime = std::chrono::steady_clock::now();
	float currentTime = 0;

	// max unsigned short == update all values
	ChangesBuffer.push_back(std::numeric_limits<unsigned short>::max());

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
		const float delta = duration.count() - currentTime;
		frameTimes.push_back(delta);

		// Check if the list size exceeds the limit
		if (frameTimes.size() > 3000) {
			// Remove the first element
			frameTimes.erase(frameTimes.begin());
		}

		Update(delta);
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

		HandleChanges();

		glUniform1uiv(frameCountLocation, 1, &++frameCount);

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
