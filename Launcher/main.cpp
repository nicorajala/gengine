#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "glad/glad.h"
#include "filesystem/filesystem.hpp"
#include "tinyfiledialogs.h"
#include <vector>
#include <string>
#include <fstream>
#include <iostream>

static std::string projectsRoot = "Projects";
static std::string editorExe = "GENGINE.exe";

void ensureProjectsDir() {
    if (!fs::exists(projectsRoot)) fs::create_directory(projectsRoot);
}

std::vector<fs::path> listProjects() {
    std::vector<fs::path> result;
    ensureProjectsDir();
    for (auto& entry : fs::directory_iterator(projectsRoot)) {
        if (entry.is_directory() && fs::exists(entry.path_() / "project.gproject")) {
            result.push_back(entry.path_());
        }
    }
    return result;
}

bool createProject(const std::string& name, const std::string& location) {
    fs::path projPath = fs::path(location) / name;
    if (fs::exists(projPath)) return false;
    fs::create_directory(projPath);
    std::ofstream f(projPath / "project.gproject");
    f << "name=" << name << "\n";
    f.close();
    fs::create_directory(projPath / "scenes");
    fs::create_directory(projPath / "textures");
    return true;
}

void launchEditor(const fs::path& projectPath) {
#if defined(_WIN32)
    std::string cmd = "\"" + editorExe + "\" --project \"" + projectPath.string() + "\"";
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    std::vector<char> buf(cmd.begin(), cmd.end());
    buf.push_back('\0');
    if (CreateProcessA(NULL, buf.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
#else
    std::string cmd = "./" + editorExe + " --project \"" + projectPath.string() + "\" &";
    system(cmd.c_str());
#endif
}

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow("GENGINE Launcher",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          800, 600,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (!window) {
        SDL_Log("Could not create window: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    gladLoadGL();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 120");

    bool quit = false;
    SDL_Event e;
    char newProjName[128] = "";
    char selectedFolder[512] = "";
    std::string errorMsg;
    int selectedProject = -1;

    while (!quit) {
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT) quit = true;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        int winW, winH;
        SDL_GetWindowSize(window, &winW, &winH);
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)winW, (float)winH));

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar;

        ImGui::Begin("GENGINE Launcher", nullptr, flags);

        ImGui::Text("Projects in '%s':", projectsRoot.c_str());
        auto projects = listProjects();
        for (size_t i = 0; i < projects.size(); ++i) {
            std::string label = projects[i].filename().string();
            if (ImGui::Selectable(label.c_str(), selectedProject == (int)i)) {
                selectedProject = (int)i;
            }
        }

        // Calculate the height needed for the bottom controls
        float bottomControlsHeight = 30.0f;
        bottomControlsHeight += ImGui::GetFrameHeightWithSpacing();
        bottomControlsHeight += ImGui::GetFrameHeightWithSpacing();
        if (!errorMsg.empty())
            bottomControlsHeight += ImGui::GetTextLineHeightWithSpacing();

        float contentRegionAvail = ImGui::GetContentRegionAvail().y;
        if (contentRegionAvail > bottomControlsHeight)
            ImGui::Dummy(ImVec2(0, contentRegionAvail - bottomControlsHeight));

        // --- Bottom controls ---
        ImGui::Separator();
        ImGui::InputText("New Project Name", newProjName, sizeof(newProjName));
        ImGui::InputText("Location", selectedFolder, sizeof(selectedFolder), ImGuiInputTextFlags_ReadOnly);
        if (ImGui::Button("Choose Location")) {
            const char* folder = tinyfd_selectFolderDialog("Select Project Location", NULL);
            if (folder) strcpy(selectedFolder, folder);
        }
        ImGui::SameLine();
        if (ImGui::Button("Create New Project")) {
            if (strlen(newProjName) == 0 || strlen(selectedFolder) == 0) {
                errorMsg = "Project name and location required.";
            } else if (!createProject(newProjName, selectedFolder)) {
                errorMsg = "Project already exists or failed to create.";
            } else {
                errorMsg.clear();
                strcpy(newProjName, "");
                strcpy(selectedFolder, "");
            }
        }

        if (selectedProject >= 0 && selectedProject < (int)projects.size()) {
            ImGui::SameLine();
            if (ImGui::Button("Open in Editor")) {
                launchEditor(projects[selectedProject]);
            }
        }

        if (!errorMsg.empty()) {
            ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "%s", errorMsg.c_str());
        }

        ImGui::End();

        glViewport(0, 0, 800, 600);
        glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);
        SDL_Delay(16);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}