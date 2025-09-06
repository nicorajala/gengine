#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOGDI
#define NOOPENGL
#endif

#define STB_IMAGE_IMPLEMENTATION

#include "glad/glad.h"
#define SDL_MAIN_HANDLED
#include "SDL2/SDL.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include <iostream>
#include "math/math.hpp"

#include "Engine/util/shaderc.hpp"
#include "Engine/input.hpp"
#include "Engine/sceneManager.hpp"
#include "Engine/editor.hpp"
#include "Engine/objects/shapegen.hpp"

#include "GameMain.hpp"

using namespace NMATH;

// Global shader mode index used by the editor (0 = lit, 1 = unlit)
int glShaderType = 0;

int main(int argc, char* argv[]) {

	bool game_mode = false;
	std::string scenePath = "";
	for (int i = 1; i < argc; ++i) {
		std::string a = argv[i];
		if (a == "--game") {
			game_mode = true;
			continue;
		}
		// support --scene <path> and --scene=<path>
		if (a == "--scene" && i + 1 < argc) {
			scenePath = std::string(argv[++i]);
			continue;
		}
		const std::string key = "--scene=";
		if (a.size() > key.size() && a.compare(0, key.size(), key) == 0) {
			scenePath = a.substr(key.size());
			continue;
		}
	}

	printf("[Main] Args: game_mode=%d scenePath='%s'\n", game_mode ? 1 : 0, scenePath.c_str());

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
		std::cerr << "SDL could not initialize! " << SDL_GetError() << std::endl;
		return -1;
	}

	SDL_Window* window = nullptr;
	SDL_GLContext glContext = nullptr;

	int major = 3, minor = 3;
	bool success = false;

	while (!success && major >= 2) {
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

		window = SDL_CreateWindow("GENGINE", 100, 100,
											1024, 768,
											SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

		if (!window) {
			printf("Window creation failed: %s\n", SDL_GetError());
			major--; minor = 0;
			continue;
		}

		glContext = SDL_GL_CreateContext(window);
		if (!glContext) {
			std::cerr << "OpenGL context could not be created for version: " << major << "."<< minor << " Switching to older version" << std::endl;
			SDL_GL_DeleteContext(glContext);
			SDL_DestroyWindow(window);
			window = NULL;
			major--; minor = 0;
			continue;
		}

		SDL_GL_MakeCurrent(window, glContext);

		if (!gladLoadGL()) {
			std::cerr << "Failed to initialize GLAD\n";
			return -1;
		}

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		ImGui::StyleColorsDark();
		ImGui_ImplSDL2_InitForOpenGL(window, glContext);
		ImGui_ImplOpenGL3_Init("#version 120");
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		success = true;
	}

	if (!success) {
		printf("No compatible OpenGL context could be created.\n");
		SDL_Quit();
		return -1;
	}

	SDL_GL_MakeCurrent(window, glContext);

	glEnable(GL_DEPTH_TEST);
	//glEnable(GL_CULL_FACE);
	//glCullFace(GL_BACK);
	glFrontFace(GL_CCW);

	Shaderc ShaderCompiler;
	GLuint shaderProgram = ShaderCompiler.loadShader("shaders/vertex.glsl", "shaders/fragment.glsl");
	if (shaderProgram == 0) {
		std::cerr << "Failed to load/compile/link shaders. Exiting." << std::endl;
		SDL_Quit();
		return -1;
	}
	std::cerr << "[Main] shaderProgram id = " << shaderProgram << std::endl;

	EditorInput inputHandler(window);

	Uint64 NOW = SDL_GetTicks();
	Uint64 LAST = 0;
	float deltaTime = 0.016f;

	float editorWidth = 0.f;

	bool running = true;
	SDL_Event event;

	Editor* editor;

	bool prevViewportMouseDown = false;

	// --- Setup an FBO for rendering the viewport texture ---
	GLuint viewportFBO = 0;
	GLuint viewportTexture = 0;
	GLuint viewportDepthRBO = 0;
	int viewportW = 1024, viewportH = 768;

	glGenFramebuffers(1, &viewportFBO);
	glGenTextures(1, &viewportTexture);
	glBindTexture(GL_TEXTURE_2D, viewportTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, viewportW, viewportH, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glGenRenderbuffers(1, &viewportDepthRBO);
	glBindRenderbuffer(GL_RENDERBUFFER, viewportDepthRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, viewportW, viewportH);

	glBindFramebuffer(GL_FRAMEBUFFER, viewportFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, viewportTexture, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, viewportDepthRBO);

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		std::cerr << "Viewport FBO incomplete: " << status << std::endl;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	const GLubyte* glver = glGetString(GL_VERSION);
	SDL_SetWindowTitle(window, ("GENGINE - Editor (OpenGL " + std::string((const char*)glver) + ")").c_str());

	GameMain game;

	if(game_mode) {
		if (!scenePath.empty()) {
			FILE* f = fopen(scenePath.c_str(), "r");
			if (f) { fclose(f); game.scene->loadScene(scenePath); }
			else {
				// try relative to executable base path
				char* base = SDL_GetBasePath();
				if (base) {
					std::string candidate = std::string(base) + scenePath;
					if (fs::exists(candidate)) {
						printf("[Main] Found scene at base path: %s\n", candidate.c_str());
						game.scene->loadScene(candidate);
						SDL_free(base);
					} else {
						SDL_free(base);
						// try relative to repo root / working dir
						if (fs::exists(scenePath)) {
							printf("[Main] Found scene at cwd: %s\n", scenePath.c_str());
							game.scene->loadScene(scenePath);
						} else {
							printf("[Main] Scene file not found: %s\n", scenePath.c_str());
						}
					}
				} else {
					if (fs::exists(scenePath)) { game.scene->loadScene(scenePath); }
					else printf("[Main] Scene file not found: %s\n", scenePath.c_str());
				}
			}
		}
		game.Start();

		for (auto* o : game.scene->objects) {
			if (o) {
				o->setupMesh(); // idempotent: safe to call even if resources already created
			}
		}

		SDL_SetRelativeMouseMode(SDL_TRUE);
	} else {
		editor = new Editor(window, &game, editorWidth);
		editor->setViewportTexture(viewportTexture, viewportW, viewportH);
		game.scene->initGrid(50, 1.f);
		game.scene->initLightGizmo();
	}

	while (running) {
		LAST = NOW;
		NOW = SDL_GetTicks();
		deltaTime = (NOW - LAST) / 1000.0f;

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		while (SDL_PollEvent(&event)) {
			ImGui_ImplSDL2_ProcessEvent(&event);
			if (event.type == SDL_QUIT) running = false;
			inputHandler.handleEvent(event, window);

			if (game_mode && event.type == SDL_MOUSEMOTION && game.player->cameraController) {
				// event.motion.xrel / yrel contain the relative motion when relative mode is enabled.
				game.player->cameraController->onMouseMotion((int)event.motion.xrel, (int)event.motion.yrel);
			}
		}

		if(game_mode) {
			game.Update(deltaTime);
		}

		int windowWidth, windowHeight;
		SDL_GetWindowSize(window, &windowWidth, &windowHeight);

		// Update editor UI when in editor mode
		if (!game_mode) {
			editorWidth = 350.f;
			if (editor) editor->Update();
		}

		float availableWidth = windowWidth - editorWidth;
		float availableHeight = windowHeight;
		float aspect = 16.f/10.f;
		if (game_mode) {
			aspect = (float)windowWidth / (float)windowHeight;
		}
		float viewportWidth = availableWidth;
		float viewportHeight = availableWidth / aspect;
		if (viewportHeight > availableHeight) {
			viewportHeight = availableHeight;
			viewportWidth = viewportHeight * aspect;
		}
		int viewportX = 0;
		int viewportY = (availableHeight - viewportHeight) / 2;

		if (game_mode) {
			viewportX = 0;
			viewportY = 0;
			viewportWidth = (float)windowWidth;
			viewportHeight = (float)windowHeight;
		}

		glViewport(viewportX, viewportY, (int)viewportWidth, (int)viewportHeight);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		Mat4 view;
		Mat4 projection;
		if (!game_mode) {
			view = lookAt(inputHandler.cameraPos,
						  inputHandler.cameraPos + inputHandler.cameraFront,
						  inputHandler.cameraUp);
			projection = perspective(radians(45.0f),
				(float)viewportWidth / (float)viewportHeight,
				0.1f, 100.0f);
		} else {
			view = lookAt(game.player->mainCamera.position,
						  game.player->mainCamera.position + game.player->mainCamera.forward(),
						  Vec3d(0.0f, 1.0f, 0.0f));
			projection = game.player->mainCamera.getProjectionMatrix((float)viewportWidth / (float)viewportHeight);
		}

		if (!game_mode && editor) {
			inputHandler.processKeyboard(deltaTime);
			inputHandler.handleViewportInput(editor, game, view, projection,
											 viewportX, viewportY, (int)viewportWidth, (int)viewportHeight);
		}

		// Only update editor viewport texture when in editor mode and size changed
		SDL_GetWindowSize(window, &windowWidth, &windowHeight);
		if (!game_mode && (windowWidth != viewportW || windowHeight != viewportH)) {
			viewportW = windowWidth;
			viewportH = windowHeight;
			glBindTexture(GL_TEXTURE_2D, viewportTexture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, viewportW, viewportH, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
			glBindTexture(GL_TEXTURE_2D, 0);

			if (viewportDepthRBO != 0) {
				glBindRenderbuffer(GL_RENDERBUFFER, viewportDepthRBO);
				glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, viewportW, viewportH);
				glBindRenderbuffer(GL_RENDERBUFFER, 0);
			}

			if (editor) {
				editor->setViewportTexture(viewportTexture, viewportW, viewportH);
			}
		}

		// If running the standalone game, render directly to the default framebuffer
		if (game_mode) {
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glViewport(0, 0, viewportW, viewportH);
		} else {
			glBindFramebuffer(GL_FRAMEBUFFER, viewportFBO);
			glViewport(0, 0, viewportW, viewportH);
		}
		glClearColor(0.1f, 0.0f, 0.2f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glUseProgram(shaderProgram);
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, view.value_ptr());
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, projection.value_ptr());
		
		game.scene->render(shaderProgram, view, projection);

		if(!game_mode) {
			GLuint activeForEditor = game.scene->getActiveProgram();
			if (activeForEditor == 0) activeForEditor = shaderProgram;
			game.scene->drawGrid(activeForEditor, view, projection);
			glDisable(GL_DEPTH_TEST);
			game.scene->drawGizmo(activeForEditor, view, projection);
			glEnable(GL_DEPTH_TEST);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		SDL_GL_SwapWindow(window);
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_Quit();
	return 0;
}
