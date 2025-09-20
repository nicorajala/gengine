cmake_minimum_required(VERSION 3.15)

project(GENGINE)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

include_directories(include include/nsmlib/ source shaders)

file(GLOB ENGINE_SOURCES "Engine/*.cpp" "Engine/*/*.cpp" "source/*.cpp" "Include/glad/*.c" )

# editor uses full backends, player only needs core imgui implementation
set(IMGUI_CORE_SRC
    ${CMAKE_SOURCE_DIR}/Include/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/Include/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/Include/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/Include/imgui/imgui_widgets.cpp

    ${CMAKE_SOURCE_DIR}/Include/imgui/imgui_demo.cpp
)

file(GLOB IMGUI_BACKENDS
    "Include/imgui/backends/imgui_impl_sdl2.cpp"
    "Include/imgui/backends/imgui_impl_opengl3.cpp"
)

add_executable(GENGINE ${ENGINE_SOURCES} ${IMGUI_CORE_SRC} ${IMGUI_BACKENDS})

add_definitions(-DIMGUI_IMPL_OPENGL_LOADER_GLAD)
add_definitions(-DIMGUI_ENABLE_DOCKING)

file(COPY shaders DESTINATION ${CMAKE_BINARY_DIR})
file(COPY textures DESTINATION ${CMAKE_BINARY_DIR})

target_compile_definitions(GENGINE PRIVATE GLM_ENABLE_EXPERIMENTAL)

target_include_directories(GENGINE PRIVATE include source shaders include/imgui include/nsmlib include/imgui/backends)
target_link_libraries(GENGINE PRIVATE SDL2::SDL2 OpenGL::GL)

# Lightweight player runtime (no editor UI)
file(GLOB_RECURSE PLAYER_SOURCES
    "Engine/*.cpp"
    "Engine/*/*.cpp"
    "source/*.cpp"
    "Include/glad/*.c"
)

list(FILTER PLAYER_SOURCES EXCLUDE REGEX "Editor\\.cpp$")
list(FILTER PLAYER_SOURCES EXCLUDE REGEX "imgui_.*\\.cpp$")
list(FILTER PLAYER_SOURCES EXCLUDE REGEX "Engine/main\\.cpp$")

list(APPEND PLAYER_SOURCES ${IMGUI_CORE_SRC})
list(APPEND PLAYER_SOURCES "${CMAKE_SOURCE_DIR}/Player/stb_image_impl.cpp")
list(APPEND PLAYER_SOURCES "${CMAKE_SOURCE_DIR}/Player/main.cpp")
list(APPEND PLAYER_SOURCES "${CMAKE_SOURCE_DIR}/Player/globals.cpp")

add_executable(GENGINE_PLAYER ${PLAYER_SOURCES})

target_compile_definitions(GENGINE_PLAYER PRIVATE GLM_ENABLE_EXPERIMENTAL)
target_include_directories(GENGINE_PLAYER PRIVATE include source shaders include/nsmlib include/imgui)
target_link_libraries(GENGINE_PLAYER PRIVATE SDL2::SDL2 OpenGL::GL)

add_custom_command(TARGET GENGINE_PLAYER POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:GENGINE_PLAYER>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:GENGINE_PLAYER>/shaders
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:GENGINE_PLAYER>/textures
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/textures $<TARGET_FILE_DIR:GENGINE_PLAYER>/textures
    COMMENT "Packaging shaders and textures alongside GENGINE_PLAYER"
)

add_executable(GENGINE_LAUNCHER Launcher/main.cpp include/glad/glad.c include/tinyfiledialogs/tinyfiledialogs.c ${IMGUI_CORE_SRC} ${IMGUI_BACKENDS})
target_include_directories(GENGINE_LAUNCHER PRIVATE include source shaders include/nsmlib include/imgui include/tinyfiledialogs/)
target_link_libraries(GENGINE_LAUNCHER PRIVATE SDL2::SDL2 OpenGL::GL)