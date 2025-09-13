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
static std::string externalProjectsFile = "projects.ini";

std::vector<fs::path> loadExternalProjects() {
    std::vector<fs::path> result;
    std::ifstream f(externalProjectsFile);
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && fs::exists(fs::path(line) / "project.gproject"))
            result.push_back(fs::path(line));
    }
    return result;
}

void saveExternalProjects(const std::vector<fs::path>& projects) {
    std::ofstream f(externalProjectsFile, std::ios::trunc);
    for (const auto& p : projects)
        f << p.string() << "\n";
}

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

static std::string get_executable_dir() {
#if defined(_WIN32)
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, buf, MAX_PATH);
    if (len == 0) return std::string();
    std::string s(buf, buf + len);
#else
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len <= 0) return std::string();
    std::string s(buf, buf + len);
#endif
    size_t pos = s.find_last_of("/\\");
    if (pos == std::string::npos) return std::string(".");
    return s.substr(0, pos);
}

bool createProject(const std::string& name, const std::string& location) {
    fs::path projPath = fs::path(location) / name;
    if (fs::exists(projPath)) return false;

    // Create project base and subfolders
    if (!fs::create_directory(projPath)) {
        std::cerr << "[createProject] FAILED to create project folder: " << projPath.string() << "\n";
        return false;
    }
    std::ofstream f(projPath / "project.gproject");
    f << "name=" << name << "\n";
    f.close();
    fs::create_directories(projPath / "scenes");
    fs::create_directories(projPath / "textures");

    // locate shaders source
    fs::path shadersSource = "shaders"; // first try: relative cwd
    std::cerr << "[createProject] cwd: " << fs::current_path() << "\n";
    std::cerr << "[createProject] exists(\"shaders\")? " << (fs::exists(shadersSource) ? "yes" : "no") << "\n";

    if (!fs::exists(shadersSource)) {
        std::string exeDir = get_executable_dir();
        if (!exeDir.empty()) {
            fs::path candidate = fs::path(exeDir) / "shaders";
            std::cerr << "[createProject] trying exe-dir candidate: " << candidate.string() << "\n";
            if (fs::exists(candidate)) shadersSource = candidate;
        }
    }

    // try one level up from exe
    if (!fs::exists(shadersSource)) {
        std::string exeDir = get_executable_dir();
        if (!exeDir.empty()) {
            fs::path candidate = fs::path(exeDir) / ".." / "shaders";
            std::string candS = candidate.string();
            std::cerr << "[createProject] trying exe-dir ../ candidate: " << candS << "\n";
            if (fs::exists(candidate)) shadersSource = candidate;
        }
    }

    fs::path shadersDest = projPath / "shaders";

    if (!fs::exists(shadersSource)) {
        std::cerr << "[createProject] NO shaders source found. Tried: 'shaders' and exe-relative locations. Skipping copy.\n";
        return true; // project created, but no shaders copied
    }

    // Ensure destination exists
    if (!fs::create_directories(shadersDest)) {
        std::cerr << "[createProject] FAILED to create shaders destination: " << shadersDest.string() << "\n";
    }

    // Try high level copy + overwrite
    bool ok = fs::copy(shadersSource, shadersDest,
                       fs::copy_options_recursive | fs::copy_options_overwrite_existing);
    if (ok) {
        std::cerr << "[createProject] shaders copied OK from " << shadersSource.string() << " -> " << shadersDest.string() << "\n";
        return true;
    }

    // Fallback: manual iterate & copy
    std::cerr << "[createProject] fs::copy failed, falling back to manual copy.\n";
    fs::directory_iterator dit(shadersSource);
    for (auto it = dit.begin(); it != dit.end(); it++) {
        fs::path child = it->getpath();
        fs::path destChild = shadersDest / child.filename();
        if (it->is_directory()) {
            fs::create_directories(destChild);
            if (!fs::copy(child, destChild, fs::copy_options_recursive | fs::copy_options_overwrite_existing)) {
                std::cerr << "[createProject] failed to copy directory: " << child.string() << "\n";
            }
        } else {
            if (!fs::copy_file(child, destChild, fs::copy_options_overwrite_existing)) {
                std::cerr << "[createProject] failed to copy file: " << child.string() << " -> " << destChild.string() << "\n";
            } else {
                std::cerr << "[createProject] copied file: " << child.string() << " -> " << destChild.string() << "\n";
            }
        }
    }

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

    int projectToRemove = -1;
    bool showRemoveConfirm = false;

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

        auto projects = listProjects();
        auto externalProjects = loadExternalProjects();
        projects.insert(projects.end(), externalProjects.begin(), externalProjects.end());

        if (ImGui::Button("Add existing project")) {
            const char* folder = tinyfd_selectFolderDialog("Open project folder", NULL);
            if (folder) {
                fs::path projPath(folder);
                if (fs::exists(projPath / "project.gproject")) {
                    errorMsg.clear();
                    bool alreadyListed = false;
                    for (const auto& p : projects)
                        if (p == projPath) alreadyListed = true;
                    if (!alreadyListed) {
                        auto externalProjects = loadExternalProjects();
                        externalProjects.push_back(projPath);
                        saveExternalProjects(externalProjects);
                    }
                }
                else {
                    errorMsg = "Selected folder is not a valid project.";
                }
            }
        }

        if (selectedProject >= 0 && selectedProject < (int)projects.size()) {
            ImGui::SameLine();
            if (ImGui::Button("Open in Editor")) {
                launchEditor(projects[selectedProject]);
            }
        }

        ImGui::Text("Projects:", projectsRoot.c_str());
        for (size_t i = 0; i < projects.size(); ++i) {
            std::string label = projects[i].filename().string();
            if (ImGui::Selectable(label.c_str(), selectedProject == (int)i)) {
                selectedProject = (int)i;
            }

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove from list")) {
                    projectToRemove = (int)i;
                    showRemoveConfirm = true;
                }
                ImGui::EndPopup();
            }
        }

        if (showRemoveConfirm && projectToRemove >= 0 && projectToRemove < (int)projects.size()) {
            ImGui::OpenPopup("Remove Project?");
        }
        if (ImGui::BeginPopupModal("Remove Project?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("This will only remove the project from the list.\nThe project folder will not be deleted.\n\nContinue?");
            ImGui::Separator();

            if (ImGui::Button("Yes", ImVec2(120, 0))) {
                auto externalProjects = loadExternalProjects();
                fs::path toRemove = projects[projectToRemove];
                auto it = std::find(externalProjects.begin(), externalProjects.end(), toRemove);
                if (it != externalProjects.end()) {
                    externalProjects.erase(it);
                    saveExternalProjects(externalProjects);
                }
                showRemoveConfirm = false;
                projectToRemove = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("No", ImVec2(120, 0))) {
                showRemoveConfirm = false;
                projectToRemove = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
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