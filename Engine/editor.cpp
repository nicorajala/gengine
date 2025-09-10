#include "Engine/editor.hpp"
#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

Editor::Editor(SDL_Window* w, GameMain* g, float& width)
	: window(w), game(g), editorWidth(width), viewportTexture(0), viewportTexW(0), viewportTexH(0),
	  viewportHovered(false), viewportMouseU(0.0f), viewportMouseV(0.0f), viewportMouseDown(false),
	  currentProjectPath(""), selectedFile(""), objectCount(0), renaming(false),
	  showBuildWindow(false), sceneFiles(), sceneSel(), buildMessage(), invokeCMakeBuild(true), selectedFolder(),
	showPreferences(false), showShadowView(false), shadowPreviewSize(128), showAbout(false)
{
	// initialize arrays
	pos[0]=pos[1]=pos[2]=0.0f;
	rot[0]=rot[1]=rot[2]=0.0f;
	texPath[0] = '\0';
	nameBuffer[0] = '\0';

	glShaderType = 0;

	externalEditorPath[0] = '\0';
	LoadPreferences();
}

Editor::~Editor() {
	if (!shadowPreviewTex.empty()) {
		glDeleteTextures((GLsizei)shadowPreviewTex.size(), shadowPreviewTex.data());
		shadowPreviewTex.clear();
	}
}

static inline std::string toLower(const std::string& s) {
	std::string r = s;
	std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c){ return std::tolower(c); });
	return r;
}

void Editor::ensurePreviewTextures(size_t count) {
	if (shadowPreviewTex.size() == count) return;

	// free old
	if (!shadowPreviewTex.empty()) {
		glDeleteTextures((GLsizei)shadowPreviewTex.size(), shadowPreviewTex.data());
		shadowPreviewTex.clear();
	}

	if (count == 0) return;

	shadowPreviewTex.resize(count, 0);
	glGenTextures((GLsizei)count, shadowPreviewTex.data());
	for (size_t i = 0; i < count; ++i) {
		glBindTexture(GL_TEXTURE_2D, shadowPreviewTex[i]);
		// allocate a small preview texture (we'll upload float red channel data)
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, shadowPreviewSize, shadowPreviewSize, 0, GL_RED, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
}

void Editor::Update() {
	int wW, wH; SDL_GetWindowSize(window, &wW, &wH);

	ImGuiIO& io = ImGui::GetIO();
	if (playMode) {
		if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
			mouseLocked = false;
			SDL_ShowCursor(SDL_ENABLE);
			SDL_SetRelativeMouseMode(SDL_FALSE);
		}
		if (viewportHovered && ImGui::IsMouseClicked(0) && !mouseLocked) {
			mouseLocked = true;
			SDL_ShowCursor(SDL_DISABLE);
			SDL_SetRelativeMouseMode(SDL_TRUE);
		}
		if (mouseLocked) {
			SDL_ShowCursor(SDL_DISABLE);
			SDL_SetRelativeMouseMode(SDL_TRUE);
		}
	}
	else {
		mouseLocked = false;
		SDL_ShowCursor(SDL_ENABLE);
		SDL_SetRelativeMouseMode(SDL_FALSE);
	}
	
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y+15), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, viewport->Size.y-15));
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::SetNextWindowBgAlpha(0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGuiWindowFlags dockspace_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
										ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
										ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
										ImGuiDockNodeFlags_NoWindowMenuButton;

	ImGui::Begin("DockSpaceParent", nullptr, dockspace_flags);
	ImGui::PopStyleVar(2);

	ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

	MainMenu();
	SceneGUI();
	EditorGUI();
	ProjectGUI();

	ImGui::End();

	drawShadowView();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("Viewport", NULL, ImGuiWindowFlags_NoCollapse);
	ImGui::PopStyleVar();

	ImGui::BeginChild("SceneToolbar", ImVec2(0, 28), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	{
		if (ImGui::Button("Play (process)")) {
			// create and save temporary scene path (use native separators)
			fs::path tmpScenePath = currentProjectPath.empty()
										? fs::path(fs::current_path()) / "tmp_play_scene.gscene"
										: fs::path(currentProjectPath) / "tmp_play_scene.gscene";
			tmpScenePath = fs::path(tmpScenePath);
			game->scene->saveScene(tmpScenePath.string());
			std::cerr << "[Editor] Saved play-scene to '" << tmpScenePath.string() << "'\n";

			// search for a runnable player executable in a few sensible places
			std::vector<fs::path> lookPaths = {
				fs::path(fs::current_path()) / "GENGINE",
				fs::path(fs::current_path()) / "GENGINE.exe",
			};

			// also try base path (where the editor exe lives) and common build output folders
			char* base = SDL_GetBasePath();
			if (base) {
				fs::path basePath(base);
				lookPaths.push_back(basePath / "GENGINE.exe");
				SDL_free(base);
			}

			lookPaths.push_back(fs::path(fs::current_path()) / "build" / "GENGINE.exe");
			lookPaths.push_back(fs::path(fs::current_path()) / "build" / "x64-Debug" / "GENGINE.exe");
			lookPaths.push_back(fs::path(fs::current_path()) / "build" / "x64-Release" / "GENGINE.exe");

			fs::path foundExe;
			for (auto &p : lookPaths) {
				if (!p.string().empty() && fs::exists(p) && fs::is_regular_file(p)) { foundExe = p; break; }
			}

			// last resort: try to find any file in cwd containing "GENGINE" or "GENGINE_PLAYER"
			if (foundExe.string().empty()) {
				for (auto &entry : fs::directory_iterator(fs::current_path())) {
					if (!entry.is_regular_file()) continue;
					std::string name = entry.path_().filename().string();
					if (name.find("GENGINE") != std::string::npos || name.find("GENGINE_PLAYER") != std::string::npos) {
						foundExe = entry.path_();
						break;
					}
				}
			}

			if (foundExe.string().empty()) {
				std::cerr << "[Editor] Play failed: no player executable found. Set the player path or build the player.\n";
#if defined(_WIN32)
				MessageBoxA(NULL, "No player executable found. Build the player or set the path and try again.", "Play Error", MB_OK | MB_ICONERROR);
#endif
			} else {
				// build a safe command line and launch
				std::string exePath = fs::path(foundExe).string();
				std::string scenePath = tmpScenePath.string();
				std::cerr << "[Editor] Launching: " << exePath << " --game --scene " << scenePath << std::endl;

#if defined(_WIN32)
				std::string cmdLine = "\"" + exePath + "\" --game --scene \"" + scenePath + "\"";
				STARTUPINFOA si;
				PROCESS_INFORMATION pi;
				ZeroMemory(&si, sizeof(si));
				si.cb = sizeof(si);
				ZeroMemory(&pi, sizeof(pi));

				// CreateProcess modifies the command line buffer, so give it a writable one
				std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
				cmdBuf.push_back('\0');

				BOOL ok = CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
				if (ok) {
					// detach handles so the editor doesn't wait on the child
					CloseHandle(pi.hThread);
					CloseHandle(pi.hProcess);
					std::cerr << "[Editor] Launch succeeded\n";
				} else {
					DWORD err = GetLastError();
					std::cerr << "[Editor] CreateProcess failed, error=" << err << std::endl;
					MessageBoxA(NULL, "Failed to launch player executable. See console for details.", "Launch Error", MB_OK | MB_ICONERROR);
				}
#else
				std::string cmd = std::string("\"") + exePath + "\" --game --scene \"" + scenePath + "\"";
				std::cerr << "[Editor] Launching (shell): " << cmd << std::endl;
				int r = std::system(cmd.c_str());
				std::cerr << "[Editor] launch returned " << r << std::endl;
				(void)r;
#endif
			}
		}

		ImGui::SameLine();

		if (!playMode) {
			if (ImGui::Button("Play")) {
				playMode = true;

				playModeBackupScenePath = (currentProjectPath.empty() ? "tmp_editor_scene.gscene" : (currentProjectPath + "/tmp_editor_scene.gscene"));
				if (game && game->scene) {
					game->scene->saveScene(playModeBackupScenePath);
					game->Start();
					if (game->player) game->player->Start();
				}
			}
			ImGui::SameLine();
		}
		else {
			if (ImGui::Button("Stop")) {
				playMode = false;

				//if (game && game->scene && !playModeBackupScenePath.empty()) {
				//	if (game->player) {
				//		game->player->playerObject = nullptr;
				//		game->player->cameraTargetObject = nullptr;
				//	}
				//	game->scene->selectedObject = nullptr;

				//	game->scene->loadScene(playModeBackupScenePath);
				//}
				// Doesn't work yet

				//fs::remove(playModeBackupScenePath.c_str());
				if (game) {
					game->Start();
				}
			}
			ImGui::SameLine();
		}

		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine();

		ImGui::SameLine();
		if (ImGui::Button("lit")) {
			glShaderType = 0;
			std::cerr << "[Editor] Shader mode switched to: LIT" << std::endl;
		}

		ImGui::SameLine(); 
		if (ImGui::Button("unlit")) {
			glShaderType = 1;
			std::cerr << "[Editor] Shader mode switched to: UNLIT" << std::endl;
		}

		ImGui::SameLine();
		if (ImGui::Button("wireframe")) {
			glShaderType = 2;
			std::cerr << "[Editor] Shader mode switched to: WIREFRAME" << std::endl;
		}

		ImGui::SameLine(); 
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine();

		if (game && game->scene) {
			ImGui::SameLine();
			ImGui::Checkbox("Colliders", &game->scene->drawColliders);
		}

		ImGui::SameLine();
		if (ImGui::Button("realtime")) {
			isRealtime = !isRealtime;

			std::cerr << "[Editor] Realtime mode: " << (isRealtime ? "ON" : "OFF") << std::endl;
		}

		if (isRealtime) {
			LAST = NOW;
			NOW = SDL_GetTicks();
			deltaTime = (NOW - LAST) / 1000.0f;

			game->Update(deltaTime);
		}
	}
	ImGui::EndChild();

	Viewport::DrawViewport(viewportTexture, viewportTexW, viewportTexH,
		viewportHovered, viewportMouseU, viewportMouseV, viewportMouseDown);

	ImGui::End();
}

void Editor::setViewportTexture(GLuint tex, int w, int h) {
	viewportTexture = tex;
	viewportTexW = w;
	viewportTexH = h;
}

void Editor::MainMenu() {
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("New Project")) {
				const char* newProjName = "MyProject";
				fs::create_directory((std::string)newProjName);
				std::ofstream projFile(newProjName + std::string("/project.gproject"));
				projFile << "name=" << newProjName << "\n";
				projFile.close();
				currentProjectPath = newProjName;
			}
			if (ImGui::MenuItem("Load Project")) {
				std::string path = "MyProject";
				if (fs::exists(path + "/project.gproject")) currentProjectPath = path;
			}
			if (ImGui::MenuItem("Save Scene")) SaveScene();
			if (ImGui::MenuItem("Load Scene")) LoadScene();

			// Build entry
			if (ImGui::MenuItem("Build")) {
				showBuildWindow = true;
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit")) {
			if (ImGui::MenuItem("Preferences")) {
				showPreferences = true;
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View")) {
			ImGui::MenuItem("Shadows", NULL, &showShadowView);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Help")) {
			ImGui::MenuItem(("About"), NULL, &showAbout);

			ImGui::EndMenu();
		}
	}
	ImGui::EndMainMenuBar();

	if (showAbout) {
		ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_Always);
		ImGui::Begin("About SOULCRUSHER Editor", &showAbout);

		ImGui::Text("SOULCRUSHER Editor");
		ImGui::Separator();
		ImGui::Text("Version 1.0.0");
		ImGui::End();
	}

	// Preferences window
	if (showPreferences) {
		ImGui::SetNextWindowSize(ImVec2(520, 220), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Preferences", &showPreferences)) {
			ImGui::Text("Graphics API:");
			const char* apis[] = { "OpenGL", "Direct3D11" };
			int apiSel = preferredGraphicsAPI; // editor already has this field
			if (ImGui::Combo("Graphics API", &apiSel, apis, IM_ARRAYSIZE(apis))) {
				// user changed selection
				preferredGraphicsAPI = apiSel;
				// save to cfg so next run uses new API
				SavePreferences();
				// ask user to restart
				ImGui::SameLine();
				if (ImGui::Button("Restart now to apply")) {
					// flush any saved prefs then restart
					SavePreferences();
					// call restart helper (declared extern)
					extern bool restartApplication();
					restartApplication();
				}
				ImGui::SameLine();
				ImGui::TextDisabled("(requires restart)");
			}

			ImGui::Separator();

			ImGui::TextWrapped("Configure external editor used to open source files.");
			ImGui::InputText("External Editor Path", externalEditorPath, IM_ARRAYSIZE(externalEditorPath));
			ImGui::TextDisabled("Examples: C:\\\\Program Files\\\\VSCode\\\\Code.exe  or /usr/bin/code");
			if (ImGui::Button("Save")) {
				SavePreferences();
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear")) {
				externalEditorPath[0] = '\0';
				SavePreferences();
			}
			ImGui::End();
		}
	}

	if (showBuildWindow) {
		ImGui::SetNextWindowSize(ImVec2(520, 420), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Build Game", &showBuildWindow)) {
			if (currentProjectPath.empty() || !fs::exists(currentProjectPath)) {
				ImGui::TextWrapped("No project loaded. Set or create a project to select scenes for build.");
				if (ImGui::Button("Close")) { showBuildWindow = false; }
				ImGui::End();
				return;
			}

			// Add current scene button (saves current scene into project/scenes)
			if (ImGui::Button("Add Current Scene to Project")) {
				fs::path scenesDir = fs::path(currentProjectPath) / "scenes";
				fs::create_directories(scenesDir);
				// choose a name
				std::string base = "scene";
				int i = 0;
				std::string out;
				do {
					out = (scenesDir / (base + "_" + std::to_string(i) + ".gscene")).string();
					++i;
				} while (fs::exists(out));
				game->scene->saveScene(out);
				buildMessage = "Saved current scene into: " + out;
			}
			ImGui::SameLine();
			if (ImGui::Button("Refresh Scenes")) {
				// intentionally falls through to refresh list below
			}

			ImGui::Separator();

			// gather .gscene files under project/scenes (non-recursive)
			sceneFiles.clear();
			fs::path scenesDirPath = fs::path(currentProjectPath) / "scenes";
			if (fs::exists(scenesDirPath)) {
				for (auto &entry : fs::directory_iterator(scenesDirPath)) {
					if (!entry.is_regular_file()) continue;
					std::string s = entry.path_().string();
					if (s.size() >= 7 && s.substr(s.size() - 7) == ".gscene") {
						sceneFiles.push_back(s);
					}
				}
			}
			// also allow top-level .gscene in project root
			if (fs::exists(currentProjectPath)) {
				for (auto &entry : fs::directory_iterator(currentProjectPath)) {
					if (!entry.is_regular_file()) continue;
					std::string s = entry.path_().string();
					if (s.size() >= 7 && s.substr(s.size() - 7) == ".gscene") {
						// avoid duplicates
						bool dup = false;
						for (size_t k = 0; k < sceneFiles.size(); ++k) if (sceneFiles[k] == s) { dup = true; break; }
						if (!dup) sceneFiles.push_back(s);
					}
				}
			}

			// sync selection vector
			if (sceneSel.size() != sceneFiles.size()) {
				sceneSel.assign(sceneFiles.size(), 0);
			}

			ImGui::Text("Scenes in project:");
			ImGui::BeginChild("ScenesList", ImVec2(0, 220), true);
			for (size_t i = 0; i < sceneFiles.size(); ++i) {
				ImGui::PushID((int)i);
				ImGui::Checkbox(sceneFiles[i].c_str(), (bool*) & sceneSel[i]);
				ImGui::PopID();
			}
			ImGui::EndChild();

			ImGui::Checkbox("Invoke CMake build for player target (cmake --build . --target GENGINE_PLAYER)", &invokeCMakeBuild);

			ImGui::Separator();
			if (ImGui::Button("Build")) {
				// prepare output build folder
				time_t now = time(NULL);
				char tsbuf[32]; snprintf(tsbuf, sizeof(tsbuf), "%lld", (long long)now);
				fs::path outDir = fs::path(currentProjectPath) / "Builds" / ("Build_" + std::string(tsbuf));
				fs::create_directories(outDir);

				// copy selected scenes
				fs::path scenesOut = outDir / "scenes";
				fs::create_directories(scenesOut);
				int copied = 0;
				for (size_t i = 0; i < sceneFiles.size(); ++i) {
					if (!sceneSel[i]) continue;
					fs::path src = sceneFiles[i];
					fs::path dst = scenesOut / src.filename();
					try {
						fs::copy_file(src, dst, fs::copy_options::copy_options_overwrite_existing);
						++copied;
					} catch (std::exception& e) {
						std::cerr << "Failed to copy scene ";
					}
				}

				// copy shaders and textures (project-level or engine-level)
				try {
					fs::path shadersSrc = fs::path("shaders");
					fs::path texturesSrc = fs::path("textures");
					if (fs::exists(shadersSrc)) fs::copy(shadersSrc, outDir / "shaders", fs::copy_options_recursive | fs::copy_options_overwrite_existing);
					if (fs::exists(texturesSrc)) fs::copy(texturesSrc, outDir / "textures", fs::copy_options_recursive | fs::copy_options_overwrite_existing);
				} catch (std::exception& e) {
					std::cerr << "Asset copy error: " << e.what() << std::endl;
				}

				buildMessage = "Packaged " + std::to_string(copied) + " scenes to " + outDir.string();

				// optionally invoke cmake build for the player target
				if (invokeCMakeBuild) {
					// run build in background thread so UI doesn't hang
					buildMessage += " — starting cmake build...";
					fs::path capturedOutDir = outDir;
					this->buildMessage = buildMessage;
					std::thread([this, capturedOutDir]() {
						std::string buildCmd = "cmake --build . --target GENGINE_PLAYER --config Release";
						int r = system(buildCmd.c_str());
						if (r == 0) {
							// try to find the executable in several likely locations and copy it beside the package
							std::vector<fs::path> lookPaths = {
								fs::path(fs::current_path()) / "GENGINE_PLAYER",
								fs::path(fs::current_path()) / "GENGINE_PLAYER.exe",
								fs::path(fs::current_path()) / "GENGINE_PLAYER.exe",
								fs::path(fs::current_path()) / "Release" / "GENGINE_PLAYER.exe",
								fs::path(fs::current_path()) / "Release" / "GENGINE_PLAYER",
								fs::path(fs::current_path()) / "bin" / "Release" / "GENGINE_PLAYER.exe",
								fs::path(fs::current_path()) / "bin" / "Release" / "GENGINE_PLAYER"
							};
							fs::path foundExe;
							for (auto &p : lookPaths) {
								if (fs::exists(p)) { foundExe = p; break; }
							}
							if (foundExe.string().empty()) {
								// try scanning current directory for files starting with GENGINE_PLAYER
								for (auto &entry : fs::directory_iterator(fs::current_path())) {
									std::string name = entry.path_().filename().string();
									if (name.find("GENGINE_PLAYER") != std::string::npos) {
										foundExe = entry.path_();
										break;
									}
								}
							}
							if (!foundExe.string().empty()) {
								try {
									fs::copy(foundExe, capturedOutDir / foundExe.filename(), fs::copy_options_overwrite_existing);
									this->buildMessage += " — built exe copied: " + (capturedOutDir / foundExe.filename()).string();
								} catch (std::exception& e) {
									this->buildMessage += " — exe found but copy failed: ";
									this->buildMessage += e.what();
								}
							} else {
								this->buildMessage += " — built but exe not found automatically; check your build output.";
							}
						} else {
							this->buildMessage += " — cmake build failed (see console output).";
						}
					}).detach();
				}
			}

			ImGui::Separator();
			ImGui::TextWrapped("%s", buildMessage.c_str());

			if (ImGui::Button("Close")) {
				showBuildWindow = false;
			}
			ImGui::SameLine();
			if (ImGui::Button("Open Builds Folder")) {
				fs::path buildsRoot = fs::path(currentProjectPath) / "Builds";
				if (!fs::exists(buildsRoot)) fs::create_directories(buildsRoot);
#if defined(_WIN32)
				std::string cmd = "explorer \"" + buildsRoot.string() + "\"";
#elif defined(__APPLE__)
				std::string cmd = "open \"" + buildsRoot.string() + "\"";
#else
				std::string cmd = "xdg-open \"" + buildsRoot.string() + "\"";
#endif
				// open asynchronously to avoid blocking the UI (xdg-open/open/start usually return quickly but keep consistent)
				std::thread([cmd]() {
					int r = system(cmd.c_str());
					(void)r;
				}).detach();
			}

			ImGui::End();
		}
	}
}

void Editor::drawShadowView() {
	if (!showShadowView) return;

	ImGui::SetNextWindowSize(ImVec2(420, 300), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Shadows", &showShadowView)) {
		ImGui::End();
		return;
	}

	SceneManager* scene = (game ? game->scene : nullptr);
	if (!scene) {
		ImGui::Text("No scene available");
		ImGui::End();
		return;
	}

	size_t shadowCount = scene->lightShadows.size();
	ensurePreviewTextures(shadowCount);

	// For each directional light's shadow, read depth texture and upload into preview texture
	for (size_t i = 0; i < shadowCount; ++i) {
		Shadow* sh = scene->lightShadows[i];
		if (!sh) continue;

		// read depth image from shadow texture (may be large) -- read into CPU then upload resized float red texture.
		// This is a debug path — it's fine for interactive use but not optimized.
		GLint oldTex = 0; glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTex);
		glBindTexture(GL_TEXTURE_2D, sh->getDepthTexture());

		// read full depth image
		int w = sh->SHADOW_SIZE;
		int h = sh->SHADOW_SIZE;
		std::vector<float> depthData;
		depthData.resize((size_t)w * (size_t)h);
		// Make sure correct alignment
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, depthData.data());

		// Downsample CPU -> preview size (simple box average)
		int pw = shadowPreviewSize;
		int ph = shadowPreviewSize;
		std::vector<float> preview;
		preview.resize((size_t)pw * (size_t)ph);
		// naive downsample
		for (int py = 0; py < ph; ++py) {
			for (int px = 0; px < pw; ++px) {
				// map preview pixel to source region
				float sx0f = (float)px * (float)w / (float)pw;
				float sy0f = (float)py * (float)h / (float)ph;
				float sx1f = (float)(px + 1) * (float)w / (float)pw;
				float sy1f = (float)(py + 1) * (float)h / (float)ph;
				int sx0 = (int)floor(sx0f);
				int sy0 = (int)floor(sy0f);
				int sx1 = (int)min((float)w, ceil(sx1f));
				int sy1 = (int)min((float)h, ceil(sy1f));
				float sum = 0.0f;
				int cnt = 0;
				for (int sy = sy0; sy < sy1; ++sy) {
					for (int sx = sx0; sx < sx1; ++sx) {
						sum += depthData[static_cast<std::vector<float, std::allocator<float>>::size_type>(sy) * w + sx];
						++cnt;
					}
				}
				float val = (cnt > 0) ? (sum / (float)cnt) : 1.0f;
				preview[static_cast<std::vector<float, std::allocator<float>>::size_type>(py) * pw + px] = val;
			}
		}

		// upload preview into preview texture (R32F)
		glBindTexture(GL_TEXTURE_2D, shadowPreviewTex[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, pw, ph, 0, GL_RED, GL_FLOAT, preview.data());
		glBindTexture(GL_TEXTURE_2D, oldTex);

		// display preview
		ImGui::Text("Light %zu (type=%s)  tex=%u", i,
			(scene->lights[i].type == LightType::Directional) ? "Directional" : "Point",
			sh->getDepthTexture());
		ImGui::Image((void*)(intptr_t)shadowPreviewTex[i], ImVec2(256, 256));
		ImGui::Separator();
	}

	ImGui::End();
}

void Editor::EditorGUI() {
	int wW, wH; SDL_GetWindowSize(window, &wW, &wH);
	ImGui::Begin("Editor", nullptr, ImGuiWindowFlags_NoCollapse);

	Object* obj = game->scene->selectedObject;
	if (obj) {
		if (obj->type != "World") {
			if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
				pos[0] = obj->position.x; pos[1] = obj->position.y; pos[2] = obj->position.z;
				rot[0] = obj->rotation.x; rot[1] = obj->rotation.y; rot[2] = obj->rotation.z;
				scale[0] = obj->scale.x; scale[1] = obj->scale.y; scale[2] = obj->scale.z;

				ImGui::InputFloat3("Position", pos, "%.2f");
				ImGui::InputFloat3("Rotation", rot, "%.2f");
				if (ImGui::InputFloat3("Scale", scale, "%.2f")) {
					// user edited scale -> apply and recreate collider to match new scale
					obj->scale = Vec3d(scale[0], scale[1], scale[2]);
					if (game && game->scene) {
						game->scene->recreatePhysicsBody(obj);
					}
				}

				obj->position = Vec3d(pos[0], pos[1], pos[2]);
				obj->rotation = Vec3d(rot[0], rot[1], rot[2]);
			}

			if (ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen)) {
				if (game->scene->selectedObject && game->scene->selectedObject->physicsBody) {
					Physics::RigidBody* rb = game->scene->selectedObject->physicsBody;
					ImGui::Separator();
					ImGui::Text("Selected Object Physics");
					float mass = (float)rb->mass;
					if (ImGui::DragFloat("Mass", &mass, 0.01f, 0.01f, 1000.0f)) {
						rb->mass = max(0.01, (double)mass);
					}
					float fr = (float)rb->friction;
					if (ImGui::SliderFloat("Friction (Obj)", &fr, 0.0f, 1.0f)) {
						rb->friction = fr;
					}
					float re = (float)rb->restitution;
					if (ImGui::SliderFloat("Restitution (Obj)", &re, 0.0f, 1.0f)) {
						rb->restitution = re;
					}
					bool stat = rb->isStatic;
					if (ImGui::Checkbox("Static", &stat)) {
						rb->isStatic = stat;
					}
					bool en = rb->enabled;
					if (ImGui::Checkbox("Enabled", &en)) {
						rb->enabled = en;
					}
				}
			}

			if (ImGui::CollapsingHeader("Texture", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::InputText("Path", texPath, IM_ARRAYSIZE(texPath));
				if (ImGui::Button("Apply")) {
					if (fs::exists((std::string)texPath)) obj->texture(texPath);
				}
				if (obj->textureID) ImGui::Image((void*)(intptr_t)obj->textureID, ImVec2(128, 128));
			}
		}

		if (game && game->player && obj->type != "World") {
			ImGui::Separator();
			ImGui::Text("Player Binding:");
			if (game->player->playerObject == obj) {
				ImGui::TextDisabled("This object is assigned as the Player.");
				if (ImGui::Button("Unassign Player")) {
					// detach player from object (editor only)
					game->player->playerObject = nullptr;
					// optionally remove any editor-created physics body
				}
				// quick teleport runtime camera to object (useful while testing)
				if (ImGui::Button("Teleport Camera to Object")) {
					game->player->mainCamera.position = obj->position;
					if (obj->physicsBody) {
						if (obj->physicsBody->owner) obj->physicsBody->owner->position = obj->position;
						obj->physicsBody->position = obj->position;
					}
				}
			}
			else {
				if (ImGui::Button("Assign Selected as Player")) {
					game->player->playerObject = obj;
					// ensure object has a physics body in editor so Player script can use it if needed
					if (game->scene) game->scene->recreatePhysicsBody(obj);
					// keep camera in sync immediately
					game->player->mainCamera.position = obj->position;
				}
			}
			ImGui::Separator();
		}

		if (obj->type == "World") {
			if (ImGui::CollapsingHeader("Sky Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
				SceneManager* scene = game->scene;
				if (!scene) { ImGui::Text("No scene available"); }
				else {
					// Mode selector
					SceneManager::SkyboxMode currentMode = scene->getSkyboxMode();
					int mode = static_cast<int>(currentMode);
					const char* modes[] = { "None", "Color", "Cubemap" };
					if (ImGui::Combo("Skybox Mode", &mode, modes, IM_ARRAYSIZE(modes))) {
						if (mode == 0 && currentMode != SceneManager::SkyboxMode::None) {
							scene->clearSkybox();
						} else if (mode == 1 && currentMode != SceneManager::SkyboxMode::Color) {
							scene->setSkyboxColor(scene->getSkyboxColor());
						} else if (mode == 2 && currentMode != SceneManager::SkyboxMode::Cubemap) {
							scene->setSkyboxMode(SceneManager::SkyboxMode::Cubemap);
						}
					}

					// Color picker (only meaningful if mode == Color)
					if (mode == 1) {
						Vec3d c = scene->getSkyboxColor();
						float colArr[3] = { c.x, c.y, c.z };
						if (ImGui::ColorEdit3("Sky Color", colArr)) {
							scene->setSkyboxColor(Vec3d(colArr[0], colArr[1], colArr[2]));
						}
					}

					// Cubemap path input (only if mode == Cubemap)
					if (mode == 2) {
						static char cubemapPaths[6][256] = { 0 };
						static const char* faceNames[6] = {
							"Right (+X)", "Left (-X)", "Top (+Y)", "Bottom (-Y)", "Front (+Z)", "Back (-Z)"
						};

						// Prefill with current cubemap paths if available
						const std::vector<std::string>& currentPaths = scene->getSkyboxCubemapPaths();
						for (int i = 0; i < 6; ++i) {
							if (!currentPaths.empty() && i < (int)currentPaths.size() && cubemapPaths[i][0] == '\0') {
								strncpy(cubemapPaths[i], currentPaths[i].c_str(), sizeof(cubemapPaths[i]) - 1);
								cubemapPaths[i][sizeof(cubemapPaths[i]) - 1] = '\0';
							}
						}

						for (int i = 0; i < 6; ++i) {
							ImGui::InputText(faceNames[i], cubemapPaths[i], sizeof(cubemapPaths[i]));
						}
						if (ImGui::Button("Apply Cubemap")) {
							std::vector<std::string> faces;
							for (int i = 0; i < 6; ++i) faces.push_back(cubemapPaths[i]);
							if (scene->setSkyboxCubemap(faces)) {
								ImGui::TextColored(ImVec4(0, 1, 0, 1), "Cubemap loaded!");
							}
							else {
								ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to load cubemap.");
							}
						}
						if (scene->getSkyboxCubemapID()) {
							ImGui::Text("Cubemap loaded.");
						}
					}
				}
			}
			if (ImGui::CollapsingHeader("World Physics", ImGuiTreeNodeFlags_DefaultOpen)) {
				Physics::World& world = game->scene->physics;
				static bool physicsEnabled = world.enabled;
				if (ImGui::Checkbox("Enable Physics", &physicsEnabled)) {
					world.setEnabled(physicsEnabled);
				}
				Vec3d g = world.gravity;
				if (ImGui::DragFloat3("Gravity", &g.x, 0.1f, -100.0f, 100.0f)) {
					world.setGravity(g);
				}
				float rest = (float)world.restitution;
				if (ImGui::SliderFloat("Restitution", &rest, 0.0f, 1.0f)) {
					world.setRestitution(rest);
				}
				float corr = (float)world.positionalCorrection;
				if (ImGui::SliderFloat("Positional Correction", &corr, 0.0f, 1.0f)) {
					world.setPositionalCorrection(corr);
				}
				float fric = (float)world.friction;
				if (ImGui::SliderFloat("Friction", &fric, 0.0f, 1.0f)) {
					world.setFriction(fric);
				}
			}
			if (ImGui::CollapsingHeader("Player properties", ImGuiTreeNodeFlags_DefaultOpen)) {
				if (ImGui::Checkbox("Render player model", &game->scene->drawPlayerModel)) {
					// toggle
				}
			}
		}
	} 
	if (game->scene->selectedLightIndex >= 0) {
		Light& l = game->scene->lights[game->scene->selectedLightIndex];
		float pos[3] = { l.position.x, l.position.y, l.position.z };
		float col[3] = { l.color.x, l.color.y, l.color.z };
		float dir[3] = { l.direction.x, l.direction.y, l.direction.z };
		float intensity = l.intensity;
		if (ImGui::CollapsingHeader("Light Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (l.type == LightType::Directional) {
				ImGui::InputFloat3("Direction", dir);
			} else {
				ImGui::InputFloat3("Position", pos);
			}
			ImGui::ColorEdit3("Color", col);
			ImGui::InputFloat("Intensity", &intensity);
			l.position = Vec3d(pos[0], pos[1], pos[2]);
			l.color = Vec3d(col[0], col[1], col[2]);
			l.intensity = intensity;
			l.direction = Vec3d(dir[0], dir[1], dir[2]);
		}
	}

	editorWidth = ImGui::GetWindowWidth();
	ImGui::End();
}

void Editor::SceneGUI() {
	ImGui::Begin("Scene");

	SceneManager* scene = (game ? game->scene : nullptr);
	if (!scene) {
		ImGui::Text("No scene available");
		ImGui::End();
		return;
	}

	std::unordered_map<Object*, int> indexMap;
	indexMap.reserve(scene->objects.size());
	for (size_t i = 0; i < scene->objects.size(); ++i) indexMap[scene->objects[i]] = (int)i;

	// helper to detect ancestor relationship (avoid cycles)
	std::function<bool(Object*, Object*)> isAncestor = [&](Object* node, Object* possibleAncestor) -> bool {
		if (!node || !possibleAncestor) return false;
		Object* p = node->parent;
		while (p) {
			if (p == possibleAncestor) return true;
			p = p->parent;
		}
		return false;
	};

	std::function<void(Object*)> drawNode;
	drawNode = [&](Object* obj) {
		if (!obj) return;
		int idx = indexMap[obj];

		ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
		bool isSelected = (scene->selectedObject == obj);
		if (obj->children.empty()) nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

		// show as disabled if World sentinel with no mesh
		bool worldMarker = (obj->type == "World");

		// Tree node label: use Selectable so clicking selects
		char labelBuf[256];
		snprintf(labelBuf, sizeof(labelBuf), "%s##obj_%d", obj->name.c_str(), idx);

		if (nodeFlags & ImGuiTreeNodeFlags_NoTreePushOnOpen) {
			// leaf: draw selectable inline
			if (worldMarker) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
			if (ImGui::Selectable(labelBuf, isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
				scene->selectedObject = obj;
				scene->selectedLightIndex = -1;
			}
			if (worldMarker) ImGui::PopStyleColor();

			// Drag source
			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
				ImGui::SetDragDropPayload("SCENE_OBJECT", &idx, sizeof(int));
				ImGui::Text("%s", obj->name.c_str());
				ImGui::EndDragDropSource();
			}

			// Accept drop (reparent)
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_OBJECT")) {
					int srcIdx = *(const int*)payload->Data;
					if (srcIdx >= 0 && srcIdx < (int)scene->objects.size()) {
						Object* srcObj = scene->objects[srcIdx];
						// prevent making parent a descendant or itself
						if (srcObj != obj && !isAncestor(obj, srcObj)) {
							scene->setParent(srcObj, obj);
						}
					}
				}
				ImGui::EndDragDropTarget();
			}
		}
		else {
			// node with children: use TreeNodeEx so we can still have selectable behavior
			bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)idx, nodeFlags, "%s", obj->name.c_str());

			// Make the name selectable as well (click on name selects)
			if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
				scene->selectedObject = obj;
				scene->selectedLightIndex = -1;
			}

			// Drag source for the whole node
			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
				ImGui::SetDragDropPayload("SCENE_OBJECT", &idx, sizeof(int));
				ImGui::Text("%s", obj->name.c_str());
				ImGui::EndDragDropSource();
			}

			// Accept drop to reparent onto this node
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_OBJECT")) {
					int srcIdx = *(const int*)payload->Data;
					if (srcIdx >= 0 && srcIdx < (int)scene->objects.size()) {
						Object* srcObj = scene->objects[srcIdx];
						if (srcObj != obj && !isAncestor(obj, srcObj)) {
							scene->setParent(srcObj, obj);
						}
					}
				}
				ImGui::EndDragDropTarget();
			}

			// Context menu per node (rename / unparent / delete)
			if (ImGui::BeginPopupContextItem()) {
				if (ImGui::MenuItem("Rename")) {
					renaming = true;
					scene->selectedObject = obj;
					strncpy(nameBuffer, obj->name.c_str(), sizeof(nameBuffer) - 1);
					nameBuffer[sizeof(nameBuffer) - 1] = '\0';
				}
				if (obj->parent) {
					if (ImGui::MenuItem("Unparent")) {
						scene->setParent(obj, nullptr);
					}
				}
				if (ImGui::MenuItem("Delete")) {
					// safe deletion later: mark selection and delete after closing menu
					if (scene->selectedObject == obj) scene->selectedObject = nullptr;
					scene->removeObject(obj);
					ImGui::EndPopup();
					// children repositioned by removeObject; nothing more to do here
					ImGui::TreePop();
					return;
				}
				ImGui::EndPopup();
			}

			if (nodeOpen) {
				// draw children
				for (auto child : obj->children) drawNode(child);
				ImGui::TreePop();
			}
		}
	};

	// Build top-level roots (parent == nullptr)
	for (auto o : scene->objects) {
		if (!o) continue;
		if (o->parent == nullptr) {
			// skip internal World sentinel if you prefer (still selectable)
			drawNode(o);
		}
	}

	if (renaming && scene->selectedObject) {
		ImGui::Separator();
		ImGui::Text("Rename:");
		ImGui::InputText("##rename", nameBuffer, IM_ARRAYSIZE(nameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::SameLine();
		if (ImGui::Button("Apply") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
			scene->selectedObject->name = std::string(nameBuffer);
			renaming = false;
		}
		if (ImGui::Button("Cancel")) {
			renaming = false;
		}
	}

	// New Object menu (keeps existing functionality) with Player entry
	if (ImGui::BeginPopupContextWindow("SceneContextMenu", ImGuiPopupFlags_MouseButtonRight)) {
		if (ImGui::BeginMenu("New Object")) {
			if (ImGui::MenuItem("Cube")) scene->addObject("Cube", "Cube_" + std::to_string(objectCount++));
			if (ImGui::MenuItem("Cylinder")) scene->addObject("Cylinder", "Cylinder_" + std::to_string(objectCount++));
			if (ImGui::MenuItem("Sphere")) scene->addObject("Sphere", "Sphere_" + std::to_string(objectCount++));
			if (ImGui::MenuItem("Plane")) scene->addObject("Plane", "Plane_" + std::to_string(objectCount++));
			if (ImGui::MenuItem("Pyramid")) scene->addObject("Pyramid", "Pyramid_" + std::to_string(objectCount++));
			if (ImGui::MenuItem("Cone")) scene->addObject("Cone", "Cone_" + std::to_string(objectCount++));
			if (ImGui::MenuItem("Player")) {
				Object* p = scene->addObject("Player", "Player_" + std::to_string(objectCount++));
				// create camera target child for new player
				Object* camTarget = new Object();
				camTarget->name = p->name + "_CameraTarget";
				camTarget->type = "CameraTarget";
				camTarget->position = Vec3d(0.0f, 1.8f, -3.0f);
				scene->objects.push_back(camTarget);
				scene->setParent(camTarget, p);
				// if editor is running, wire the runtime player if available
				if (game && game->player) {
					game->player->playerObject = p;
					game->player->cameraTargetObject = camTarget;
				}
			}
			if (ImGui::BeginMenu("Light")) {
				if (ImGui::MenuItem("Point Light")) {
					Light L(LightType::Point, Vec3d(1, 1, 1), 1.0f);
					scene->addLight(L);
				}
				if (ImGui::MenuItem("Directional Light")) {
					Light L(LightType::Directional, Vec3d(1, 1, 1), 1.0f);
					scene->addLight(L);
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		// Delete selected
		if ((scene->selectedObject || scene->selectedLightIndex != -1) && ImGui::MenuItem("Delete Selected")) {
			if (scene->selectedObject) {
				scene->removeObject(scene->selectedObject);
				scene->selectedObject = nullptr;
			}
			else {
				scene->removeLight(scene->selectedLightIndex);
				scene->selectedLightIndex = -1;
			}
		}
		ImGui::EndPopup();
	}

	ImGui::End();
}

void Editor::ProjectGUI() {
	ImGui::Begin("Project", nullptr, ImGuiWindowFlags_NoCollapse);

	static float treeWidth = 250.0f; // initial width
	float minWidth = 120.0f;
	float maxWidth = ImGui::GetContentRegionAvail().x - 100.0f;

	ImGui::BeginChild("TreePanel", ImVec2(treeWidth, 0), true);

	if (ImGui::Button("New Folder")) {
		if (!selectedFolder.empty()) {
			std::string newFolder = selectedFolder + "/NewFolder";
			fs::create_directory(newFolder);
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("New C++ Class")) {
		if (!selectedFolder.empty()) {
			std::string className = "NewClass";
			std::ofstream header(selectedFolder + "/" + className + ".hpp");
			header << "#pragma once\n\nclass " << className << " {};\n";
			header.close();

			std::ofstream source(selectedFolder + "/" + className + ".cpp");
			source << "#include \"" << className << ".hpp\"\n";
			source.close();
		}
	}

	// draw tree: show directories and files (files as leaf Selectable)
	std::function<void(const fs::path&)> drawTree = [&](const fs::path& dir) {
		for (auto& entry : fs::directory_iterator(dir)) {
			// show directories as nodes
			if (entry.is_directory()) {
				std::string name = entry.getpath().filename().string();
				ImGuiTreeNodeFlags node_flags = ((selectedFolder == entry.getpath().string()) ? ImGuiTreeNodeFlags_Selected : 0)
											   | ImGuiTreeNodeFlags_OpenOnArrow;
				bool node_open = ImGui::TreeNodeEx(name.c_str(), node_flags);
				if (ImGui::IsItemClicked()) selectedFolder = entry.getpath().string();
				if (node_open) {
					drawTree(entry.getpath());
					ImGui::TreePop();
				}

				if (ImGui::BeginPopupContextItem()) {
					if (ImGui::MenuItem("New Folder")) {
						fs::create_directory(entry.getpath() / "NewFolder");
					}
					if (ImGui::MenuItem("New C++ Class")) {
						std::string className = "NewClass";
						std::ofstream header(entry.getpath() / (className + ".hpp"));
						header << "#pragma once\n\nclass " << className << " {};\n";
						header.close();
						std::ofstream source(entry.getpath() / (className + ".cpp"));
						source << "#include \"" << className << ".hpp\"\n";
						source.close();
					}
					ImGui::EndPopup();
				}
			} else {
				// file - show selectable leaf (double-click to open/load)
				std::string fname = entry.getpath().filename().string();
				bool sel = (selectedFile == entry.getpath().string());
				if (ImGui::Selectable(fname.c_str(), sel)) {
					selectedFile = entry.getpath().string();
				}
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
					// handle double-click open
					std::string ext = toLower(entry.getpath().string());
					if (ext == ".gscene") {
						game->scene->loadScene(entry.getpath().string());
					} else {
						// open in external editor if configured, otherwise use OS default
						std::string fpath = entry.getpath().string();
						std::string cmd;
						if (externalEditorPath[0] != '\0') {
							cmd = std::string("\"") + std::string(externalEditorPath) + "\" \"" + fpath + "\"";
						} else {
#if defined(_WIN32)
							cmd = "start \"\" \"" + fpath + "\"";
#elif defined(__APPLE__)
							cmd = "open \"" + fpath + "\"";
#else
							cmd = "xdg-open \"" + fpath + "\"";
#endif
						}
						int r = system(cmd.c_str());
						(void)r;
					}
				}
			}
		};
	};

	if (!currentProjectPath.empty() && fs::exists(currentProjectPath))
		drawTree(currentProjectPath);

	ImGui::EndChild();

	ImGui::SameLine();
	ImGui::PushID("Splitter");
	ImGui::InvisibleButton("##splitter", ImVec2(5, -1));
	if (ImGui::IsItemActive()) {
		treeWidth += ImGui::GetIO().MouseDelta.x;
		if (treeWidth < minWidth) treeWidth = minWidth;
		if (treeWidth > maxWidth) treeWidth = maxWidth;
	}
	ImGui::PopID();
	ImGui::SameLine();

	ImGui::BeginChild("ListPanel", ImVec2(0, 0), true);

	if (!selectedFolder.empty() && fs::exists(selectedFolder)) {
		for (auto& entry : fs::directory_iterator(selectedFolder)) {
			std::string name = entry.getpath().filename().string();
			bool isDir = entry.is_directory();

			if (isDir) ImGui::TextDisabled("[Folder] %s", name.c_str());
			else {
				bool sel = (selectedFile == entry.getpath().string());
				if (ImGui::Selectable(name.c_str(), sel)) {
					selectedFile = entry.getpath().string();
				}
				// double-click behavior in list panel as well
				if (!isDir && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
					std::string ext = toLower(entry.getpath().string());
					if (ext == ".gscene") {
						game->scene->loadScene(entry.getpath().string());
					} else {
						std::string fpath = entry.getpath().string();
						std::string cmd;
						if (externalEditorPath[0] != '\0') {
							cmd = std::string("\"") + std::string(externalEditorPath) + "\" \"" + fpath + "\"";
						} else {
#if defined(_WIN32)
							cmd = "start \"\" \"" + fpath + "\"";
#elif defined(__APPLE__)
							cmd = "open \"" + fpath + "\"";
#else
							cmd = "xdg-open \"" + fpath + "\"";
#endif
						}
						int r = system(cmd.c_str());
						(void)r;
					}
				}
			}

			if (ImGui::BeginDragDropSource()) {
				ImGui::SetDragDropPayload("DND_FILE", entry.getpath().string().c_str(),
										   entry.getpath().string().size() + 1);
				ImGui::Text("%s", name.c_str());
				ImGui::EndDragDropSource();
			}

			if (isDir && ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_FILE")) {
					const char* srcPath = (const char*)payload->Data;
					fs::path dest = entry.getpath() / fs::path(srcPath).filename();
					try { fs::rename(srcPath, dest); }
					catch (std::exception& e) { std::cerr << "Error moving file: " << e.what() << std::endl; }
				}
				ImGui::EndDragDropTarget();
			}

			if (!isDir && ImGui::BeginPopupContextItem()) {
				if (ImGui::MenuItem("Delete File")) fs::remove(entry.getpath());
				ImGui::EndPopup();
			}
		}
	} else {
		ImGui::Text("Select a folder to view its contents...");
	}

	ImGui::EndChild();
	ImGui::End();
}

void Editor::SaveScene() { 
	game->scene->saveScene(currentProjectPath+"/scene.gscene");
}
void Editor::LoadScene() {
	game->scene->loadScene(currentProjectPath+"/scene.gscene");
}

// Preferences persistence (very small key=value file in cwd)
void Editor::SavePreferences() {
	try {
		std::ofstream f("editor_prefs.cfg", std::ios::out | std::ios::trunc);
		if (!f.is_open()) return;
		f << "external_editor=" << externalEditorPath << "\n";

		std::string api = (preferredGraphicsAPI == 1) ? "vulkan" : "opengl";
		f << "graphics_api=" << api << "\n";
		f.close();
	} catch (...) {}
}

void Editor::LoadPreferences() {
	try {
		std::ifstream f("editor_prefs.cfg");
		if (!f.is_open()) return;
		std::string line;
		while (std::getline(f, line)) {
			if (line.rfind("external_editor=", 0) == 0) {
				std::string val = line.substr(strlen("external_editor="));
				size_t n = min((size_t)val.size(), sizeof(externalEditorPath)-1);
				memcpy(externalEditorPath, val.c_str(), n);
				externalEditorPath[n] = '\0';
			} else if (line.rfind("graphics_api=", 0) == 0) {
				std::string val = line.substr(strlen("graphics_api="));
				if (val == "vulkan") preferredGraphicsAPI = 1;
				else preferredGraphicsAPI = 0;
			}
		}
		f.close();
	} catch (...) {}
}