#include "Engine/editor.hpp"
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#endif

Editor::Editor(SDL_Window* w, GameMain* g, float& width)
    : window(w), game(g), editorWidth(width), viewportTexture(0), viewportTexW(0), viewportTexH(0),
      viewportHovered(false), viewportMouseU(0.0f), viewportMouseV(0.0f), viewportMouseDown(false),
      currentProjectPath(""), selectedFile(""), objectCount(0), renaming(false),
      showBuildWindow(false), sceneFiles(), sceneSel(), buildMessage(), invokeCMakeBuild(true), selectedFolder(),
	showPreferences(false), showShadowView(false), shadowPreviewSize(128)
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
    
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y+15), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, viewport->Size.y-15));
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGuiWindowFlags dockspace_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                       ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

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
        if (ImGui::Button("Play")) {
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
    }
    ImGui::EndMainMenuBar();

    // Preferences window
    if (showPreferences) {
        ImGui::SetNextWindowSize(ImVec2(520, 220), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Preferences", &showPreferences)) {
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
                    // note: this assumes current working dir is the configured build dir, or cmake was run from here
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
                                fs::copy(foundExe, outDir / foundExe.filename(), fs::copy_options_overwrite_existing);
                                buildMessage += " — built exe copied: " + (outDir / foundExe.filename()).string();
                            } catch (std::exception& e) {
                                buildMessage += " — exe found but copy failed: ";
                                buildMessage += e.what();
                            }
                        } else {
                            buildMessage += " — built but exe not found automatically; check your build output.";
                        }
                    } else {
                        buildMessage += " — cmake build failed (see console output).";
                    }
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
                system(cmd.c_str());
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
                        sum += depthData[sy * w + sx];
                        ++cnt;
                    }
                }
                float val = (cnt > 0) ? (sum / (float)cnt) : 1.0f;
                preview[py * pw + px] = val;
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
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            pos[0]=obj->position.x; pos[1]=obj->position.y; pos[2]=obj->position.z;
            rot[0]=obj->rotation.x; rot[1]=obj->rotation.y; rot[2]=obj->rotation.z;
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

            obj->position = Vec3d(pos[0],pos[1],pos[2]);
            obj->rotation = Vec3d(rot[0],rot[1],rot[2]);
        }

        if (ImGui::CollapsingHeader("Texture", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::InputText("Path", texPath, IM_ARRAYSIZE(texPath));
            if (ImGui::Button("Apply")) {
                if (fs::exists((std::string)texPath)) obj->texture(texPath);
            }
            if (obj->textureID) ImGui::Image((void*)(intptr_t)obj->textureID, ImVec2(128,128));

            bool col = obj->collisionEnabled;
            if (ImGui::Checkbox("Collision", &col)) {
                obj->collisionEnabled = col;
                if (game && game->scene) game->scene->recreatePhysicsBody(obj);
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

    for (size_t i = 0; i < game->scene->lights.size(); ++i) {
        Light& l = game->scene->lights[i];
        ImGui::PushID((int)(1000 + i));
        bool selected = (game->scene->selectedLightIndex == (int)i);
        if (ImGui::Selectable(std::string("Light_" + std::to_string(i)).c_str(), selected)) {
            game->scene->selectedLightIndex = (int)i;
            game->scene->selectedObject = nullptr;
        }
        ImGui::PopID();
    }

    for (size_t i = 0; i < game->scene->objects.size(); i++) {
        Object* obj = game->scene->objects[i];
        ImGui::PushID((int)i);
        bool selected = (game->scene->selectedObject == obj);

        if (selected && renaming) {
            strcpy(nameBuffer, obj->name.c_str());
            if (ImGui::InputText("##rename", nameBuffer, IM_ARRAYSIZE(nameBuffer),
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
                obj->name = std::string(nameBuffer);
                renaming = false;
            }
        } else {
            if (ImGui::Selectable(obj->name.c_str(), selected)) {
                game->scene->selectedObject = obj;
                game->scene->selectedLightIndex = -1; // clear any light selection
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                renaming = true;
        }

        ImGui::PopID();
    }

    if (ImGui::BeginPopupContextWindow("SceneContextMenu", ImGuiPopupFlags_MouseButtonRight)) {
        if (ImGui::BeginMenu("New Object")) {
            if (ImGui::MenuItem("Cube")) game->scene->addObject("Cube", "Cube_" + std::to_string(objectCount++));
            if (ImGui::MenuItem("Cylinder")) game->scene->addObject("Cylinder", "Cylinder_" + std::to_string(objectCount++));
            if (ImGui::MenuItem("Sphere")) game->scene->addObject("Sphere", "Sphere_" + std::to_string(objectCount++));
            if (ImGui::MenuItem("Plane")) game->scene->addObject("Plane", "Plane_" + std::to_string(objectCount++));
            if (ImGui::MenuItem("Pyramid")) game->scene->addObject("Pyramid", "Pyramid_" + std::to_string(objectCount++));
            if (ImGui::MenuItem("Cone")) game->scene->addObject("Cone", "Cone_" + std::to_string(objectCount++));
            if(ImGui::BeginMenu("Light")) {
                if(ImGui::MenuItem("Point Light")) {
                    Light dirLight(LightType::Point, Vec3d(0,0,0), 1.0f);
                }
                if(ImGui::MenuItem("Directional Light")) {
                    Light dirLight(LightType::Directional, Vec3d(0,0,0), 1.0f);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if ((game->scene->selectedObject || game->scene->selectedLightIndex != -1) && ImGui::MenuItem("Delete Object")) {
            if(game->scene->selectedObject) {
                game->scene->removeObject(game->scene->selectedObject);
                game->scene->selectedObject = nullptr;
            } else {
                game->scene->removeLight(game->scene->selectedLightIndex);
                game->scene->selectedLightIndex = -1;
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
        }
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
            }
        }
        f.close();
    } catch (...) {}
}