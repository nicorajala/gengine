#ifndef SHAPEGEN_H
#define SHAPEGEN_H

#include <vector>
#include "glad/glad.h"
#include <GL/glext.h>

#include <stb_image.h>

#include <iostream>

#include "math/math.hpp"

using namespace NMATH;

struct Vertex {
    Vec3d pos;
    Vec3d color;
    Vec3d normal;
    Vec2d texCoord;

    Vertex() = default;
    Vertex(const Vec3d& pos, const Vec3d& col, const Vec3d& norm, const Vec2d& uv)
        : pos(pos), color(col), normal(norm), texCoord(uv) {
    }
};

class ShapeGenerator {

public:
    static void createCube(float size, std::vector<Vertex>& outVertices, std::vector<unsigned int>& outIndices);
    static void createCylinder(const Vec3d& start, const Vec3d& end, float radius, int segments, std::vector<Vertex>& outVertices, std::vector<unsigned int>& outIndices);
    static void createPlane(float width, float height, std::vector<Vertex>& outVertices, std::vector<unsigned int>& outIndices);
    static void createPyramid(float size, float height, std::vector<Vertex>& outVertices, std::vector<unsigned int>& outIndices);
    static void createSphere(float radius, int segments, int rings, std::vector<Vertex>& outVertices, std::vector<unsigned int>& outIndices);
    static void createCone(const Vec3d& baseCenter, const Vec3d& tip, float baseRadius, int segments, std::vector<Vertex>& outVertices, std::vector<unsigned int>& outIndices);

    unsigned int loadTexture(const char* path);
};

#endif