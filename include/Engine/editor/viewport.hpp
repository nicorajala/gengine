#ifndef VIEWPORT_HPP
#define VIEWPORT_HPP

#include "glad/glad.h"
#include <imgui/imgui.h>
#include "imgui/imgui_internal.h"

#include "math/math.hpp"

#include "filesystem/filesystem.hpp"
using namespace NMATH;

namespace Viewport {
	void DrawViewport(GLuint viewportTexture, int viewportTexW, int viewportTexH, 
					bool& viewportHovered, float& viewportMouseU, float& viewportMouseV, bool& viewportMouseDown);
};

#endif
