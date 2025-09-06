#include <cstdio>
#include <cstdlib>
#include <string>
#include <iostream>

#define SDL_MAIN_HANDLED
#include "SDL2/SDL.h"
#include "glad/glad.h"

#include "GameMain.hpp"
#include "Engine/util/shaderc.hpp"
#include "math/math.hpp"
#include "filesystem/filesystem.hpp"

using namespace NMATH;

int main(int argc, char* argv[])
{
	std::string scenePath;
	for (int i = 1; i < argc; ++i) {
		std::string a = argv[i];
		const std::string key = "--scene=";
		if (a == "--scene" && i + 1 < argc) { scenePath = argv[++i]; continue; }
		if (a.compare(0, key.size(), key) == 0) { scenePath = a.substr(key.size()); continue; }
	}

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
		std::cerr << "SDL init failed: " << SDL_GetError() << std::endl;
		return -1;
	}

	SDL_Window* window = SDL_CreateWindow("GENGINE Game",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
	if (!window) {
		std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
		SDL_Quit();
		return -1;
	}

	// Request a modern OpenGL context (3.3 core)
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	SDL_GLContext glContext = SDL_GL_CreateContext(window);
	if (!glContext) {
		std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << std::endl;
		SDL_DestroyWindow(window);
		SDL_Quit();
		return -1;
	}

	if (!gladLoadGL()) {
		std::cerr << "gladLoadGL failed" << std::endl;
		SDL_GL_DeleteContext(glContext);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return -1;
	}

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	Shaderc shaderCompiler;
	GLuint program = shaderCompiler.loadShader("shaders/vertex.glsl", "shaders/fragment.glsl");
	if (program == 0) {
		std::cerr << "Failed to load shader program" << std::endl;
		SDL_GL_DeleteContext(glContext);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return -1;
	}

	// Create and initialize the game
	GameMain game;
	if (!scenePath.empty()) {
		if (fs::exists(scenePath)) {
			game.scene->loadScene(scenePath);
		} else {
			char* base = SDL_GetBasePath();
			if (base) {
				std::string candidate = std::string(base) + scenePath;
				if (fs::exists(candidate)) game.scene->loadScene(candidate);
				SDL_free(base);
			}
		}
	}
	game.Start();

	bool running = true;
	SDL_Event event;
	Uint64 NOW = SDL_GetTicks();
	Uint64 LAST = NOW;
	float deltaTime = 0.016f;

	while (running) {
		LAST = NOW;
		NOW = SDL_GetTicks();
		deltaTime = (NOW - LAST) / 1000.0f;

		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) running = false;
			if (event.type == SDL_KEYDOWN) {
				if (event.key.keysym.sym == SDLK_ESCAPE) running = false;
			}

			if (event.type == SDL_MOUSEMOTION && game.player->cameraController) {
				game.player->cameraController->onMouseMotion((int)event.motion.xrel, (int)event.motion.yrel);
			}
		}

		game.Update(deltaTime);

        int w, h;
        SDL_GetWindowSize(window, &w, &h);

		Mat4 view = lookAt(game.player->mainCamera.position,
			game.player->mainCamera.position + game.player->mainCamera.forward(),
			Vec3d(0.0f, 1.0f, 0.0f));
		Mat4 projection = game.player->mainCamera.getProjectionMatrix((float)w / (float)h);

        // Render
        glViewport(0, 0, w, h);
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(program);
        glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, GL_FALSE, view.value_ptr());
        glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, projection.value_ptr());

        game.scene->render(program, view, projection);

		SDL_GL_SwapWindow(window);
	}

	SDL_GL_DeleteContext(glContext);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}