#include "Engine/util/shaderc.hpp"
#include <SDL2/SDL.h>
#include <vector>
#include <cstring>
#include <iostream>
#include <sstream>
#include <fstream>

std::string Shaderc::loadShaderSource(const char* filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "ERROR::SHADER::FILE_NOT_FOUND: " << filepath << std::endl;
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

static std::string getGLStringSafe(GLenum name) {
    // Return empty if no GL context
    if (SDL_GL_GetCurrentContext() == nullptr) return std::string();
    const GLubyte* str = glGetString(name);
    return str ? std::string((const char*)str) : std::string();
}

GLuint Shaderc::loadShader(const char* vertexPath, const char* fragmentPath) {
    // Prevent calling GL functions if no GL context exists (e.g. running with DX11 renderer)
    if (SDL_GL_GetCurrentContext() == nullptr) {
        std::cerr << "[Shaderc] No OpenGL context available; skipping shader load for " << vertexPath << " / " << fragmentPath << std::endl;
        return 0;
    }

    std::string vCode = loadShaderSource(vertexPath);
    std::string fCode = loadShaderSource(fragmentPath);

    std::cerr << "[Shaderc] loadShader start: vlen=" << vCode.size() << " flen=" << fCode.size() << std::endl;

    const std::string glver = getGLStringSafe(GL_VERSION);
    const std::string glslver = getGLStringSafe(GL_SHADING_LANGUAGE_VERSION);
    std::cerr << "GL Version: " << (glver.empty() ? "(no context)" : glver)
              << " | GLSL Version: " << (glslver.empty() ? "(no context)" : glslver) << std::endl;


    if (vCode.empty()) {
        std::cerr << "ERROR::SHADER::VERTEX::SOURCE_EMPTY: " << vertexPath << std::endl;
        return 0;
    }
    if (fCode.empty()) {
        std::cerr << "ERROR::SHADER::FRAGMENT::SOURCE_EMPTY: " << fragmentPath << std::endl;
        return 0;
    }
    const char* vShaderCode = vCode.c_str();
    const char* fShaderCode = fCode.c_str();

    auto getInfoLog = [](GLuint obj, bool isProgram) -> std::string {
        GLint length = 0;
        if (isProgram) glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &length);
        else glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &length);
        if (length <= 1) return std::string();
        std::vector<char> buf(length);
        if (isProgram) glGetProgramInfoLog(obj, length, nullptr, buf.data());
        else glGetShaderInfoLog(obj, length, nullptr, buf.data());
        return std::string(buf.data(), buf.data() + length);
    };

    // Compile vertex shader
    std::cerr << "[Shaderc] compiling vertex shader" << std::endl;
    GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
    if (vertex == 0) { std::cerr << "[Shaderc] glCreateShader returned 0 for vertex\n"; return 0; }
    glShaderSource(vertex, 1, &vShaderCode, nullptr);
    glCompileShader(vertex);

    GLint success = GL_FALSE;
    glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
    if (!success) {
        std::string infoLog = getInfoLog(vertex, false);
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
        std::cerr << "--- Vertex shader source (" << vertexPath << ") ---\n" << vCode << std::endl;
        GLenum err = glGetError();
        std::cerr << "glGetError after vertex compile: " << err << std::endl;
        glDeleteShader(vertex);
        return 0;
    }
    else {
        std::cerr << "[Shaderc] vertex compiled successfully (id=" << vertex << ")" << std::endl;
    }

    // Compile fragment shader
    std::cerr << "[Shaderc] compiling fragment shader" << std::endl;
    GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
    if (fragment == 0) { std::cerr << "[Shaderc] glCreateShader returned 0 for fragment\n"; glDeleteShader(vertex); return 0; }
    glShaderSource(fragment, 1, &fShaderCode, nullptr);
    glCompileShader(fragment);

    glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
    if (!success) {
        std::string infoLog = getInfoLog(fragment, false);
        std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
        std::cerr << "--- Fragment shader source (" << fragmentPath << ") ---\n" << fCode << std::endl;
        GLenum err = glGetError();
        std::cerr << "glGetError after fragment compile: " << err << std::endl;
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return 0;
    }
    else {
        std::cerr << "[Shaderc] fragment compiled successfully (id=" << fragment << ")" << std::endl;
    }

    GLuint program = glCreateProgram();
    if (program == 0) { std::cerr << "[Shaderc] glCreateProgram returned 0\n"; glDeleteShader(vertex); glDeleteShader(fragment); return 0; }
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);

    glBindAttribLocation(program, 0, "aPos");
    glBindAttribLocation(program, 1, "aColor");
    glBindAttribLocation(program, 2, "aNormal");
    glBindAttribLocation(program, 3, "aTexCoord");

    std::cerr << "[Shaderc] linking program (id=" << program << ")" << std::endl;
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        std::string infoLog = getInfoLog(program, true);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        GLenum err = glGetError();
        std::cerr << "glGetError after program link: " << err << std::endl;
        glDeleteProgram(program);
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return 0;
    }
    std::cerr << "[Shaderc] program linked successfully (id=" << program << ")" << std::endl;

    // Cleanup
    glDetachShader(program, vertex);
    glDetachShader(program, fragment);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    return program;
}