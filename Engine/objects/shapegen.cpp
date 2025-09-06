#include "Engine/objects/shapegen.hpp"

void ShapeGenerator::createCube(float size, std::vector<Vertex>& outVertices, std::vector<unsigned int>& outIndices) {
    float h = size / 2.0f;

    Vec3d facePositions[6][4] = {
        {{-h,-h,-h},{h,-h,-h},{h,h,-h},{-h,h,-h}}, // back
        {{-h,-h,h},{h,-h,h},{h,h,h},{-h,h,h}},     // front
        {{-h,-h,-h},{-h,h,-h},{-h,h,h},{-h,-h,h}}, // left
        {{h,-h,-h},{h,h,-h},{h,h,h},{h,-h,h}},     // right
        {{-h,h,-h},{h,h,-h},{h,h,h},{-h,h,h}},     // top
        {{-h,-h,-h},{h,-h,-h},{h,-h,h},{-h,-h,h}}  // bottom
    };

    Vec3d normals[6] = {
        {0,0,-1}, {0,0,1}, {-1,0,0}, {1,0,0}, {0,1,0}, {0,-1,0}
    };

    Vec2d uvs[4] = {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f}
    };

    Vec3d color = {1,1,1};

    for(int f=0; f<6; f++){
        unsigned int startIndex = outVertices.size();
        for(int v=0; v<4; v++){
            outVertices.push_back({facePositions[f][v], color, normals[f], uvs[v]});
        }
        // two triangles per face
        // Ensure winding matches the provided normal (so back-face culling works)
        Vec3d v0 = facePositions[f][0];
        Vec3d v1 = facePositions[f][1];
        Vec3d v2 = facePositions[f][2];
        Vec3d a = v1 - v0;
        Vec3d b = v2 - v0;
        Vec3d cross = a.cross(b);
        float d = cross.dot(normals[f]);
        if (d >= 0.0f) {
            outIndices.push_back(startIndex);
            outIndices.push_back(startIndex+1);
            outIndices.push_back(startIndex+2);

            outIndices.push_back(startIndex+2);
            outIndices.push_back(startIndex+3);
            outIndices.push_back(startIndex);
        } else {
            // reverse winding
            outIndices.push_back(startIndex);
            outIndices.push_back(startIndex+2);
            outIndices.push_back(startIndex+1);

            outIndices.push_back(startIndex);
            outIndices.push_back(startIndex+3);
            outIndices.push_back(startIndex+2);
        }
    }
}

void ShapeGenerator::createCylinder(const Vec3d& start, const Vec3d& end, float radius, int segments, std::vector<Vertex>& outVertices, std::vector<unsigned int>& outIndices) {
    if (segments < 3) segments = 3;

    Vec3d dir = (end - start).normalized();

    Vec3d up(0, 1, 0);
    if (fabs(dir.dot(up)) > 0.99f) up = Vec3d(1, 0, 0);

    Vec3d side = dir.cross(up).normalized();
    Vec3d up2 = side.cross(dir).normalized();

    // precompute ring points, radial normals and u coordinates
    std::vector<Vec3d> ringStart(segments);
    std::vector<Vec3d> ringEnd(segments);
    std::vector<Vec3d> radialNormals(segments);
    std::vector<float> ucoords(segments);

    for (int i = 0; i < segments; ++i) {
        float theta = 2.0f * PI * (float)i / (float)segments;
        float c = cosf(theta);
        float s = sinf(theta);
        Vec3d radial = side * c + up2 * s;
        radial = radial.normalized();
        radialNormals[i] = radial;
        ringStart[i] = start + radial * radius;
        ringEnd[i] = end + radial * radius;
        ucoords[i] = (float)i / (float)segments;
    }

    Vec3d color = {1,1,1};

    // SIDE: push shared ring vertices (start ring then end ring)
    unsigned int sideStartIndex = outVertices.size();
    for (int i = 0; i < segments; ++i) {
        // start ring vertex
        outVertices.push_back(Vertex( ringStart[i], color, radialNormals[i], Vec2d(ucoords[i], 0.0f) ));
    }
    for (int i = 0; i < segments; ++i) {
        // end ring vertex
        outVertices.push_back(Vertex( ringEnd[i], color, radialNormals[i], Vec2d(ucoords[i], 1.0f) ));
    }

    // side indices (two triangles per segment) with winding check
    for (int i = 0; i < segments; ++i) {
        int ni = (i + 1) % segments;

        unsigned int s_i   = sideStartIndex + i;
        unsigned int s_ni  = sideStartIndex + ni;
        unsigned int e_i   = sideStartIndex + segments + i;
        unsigned int e_ni  = sideStartIndex + segments + ni;

        // compute triangle normal using ring positions and compare with outward radial normal
        Vec3d v0 = ringStart[i];
        Vec3d v1 = ringStart[ni];
        Vec3d v2 = ringEnd[ni];
        Vec3d triNormal = (v1 - v0).cross(v2 - v0);
        float dotOut = triNormal.dot(radialNormals[i]);

        if (dotOut >= 0.0f) {
            // original winding (faces outward)
            outIndices.push_back(s_i);
            outIndices.push_back(s_ni);
            outIndices.push_back(e_ni);

            outIndices.push_back(s_i);
            outIndices.push_back(e_ni);
            outIndices.push_back(e_i);
        } else {
            // flip winding
            outIndices.push_back(s_i);
            outIndices.push_back(e_ni);
            outIndices.push_back(s_ni);

            outIndices.push_back(s_i);
            outIndices.push_back(e_i);
            outIndices.push_back(e_ni);
        }
    }

    // CAPS
    // Start cap (near start): normal points -dir
    Vec3d centerStart = start;
    Vec3d capNormalStart = dir * -1.0f;
    Vec2d centerUV = {0.5f, 0.5f};

    unsigned int capStartCenterIndex = outVertices.size();
    outVertices.push_back(Vertex(centerStart, color, capNormalStart, centerUV));

    // push ring vertices for start cap (with cap normal and cap UV)
    for (int i = 0; i < segments; ++i) {
        Vec3d r_i = ringStart[i] - start;
        Vec2d uv_i = Vec2d( 0.5f + 0.5f * (r_i.dot(side) / radius),  0.5f + 0.5f * (r_i.dot(up2) / radius) );
        outVertices.push_back(Vertex(ringStart[i], color, capNormalStart, uv_i));
    }

    // indices for start cap: ensure winding agrees with capNormalStart
    for (int i = 0; i < segments; ++i) {
        int ni = (i + 1) % segments;
        unsigned int ring_i_index = capStartCenterIndex + 1 + i;
        unsigned int ring_ni_index = capStartCenterIndex + 1 + ni;

        // compute triangle normal for (center, ring_ni, ring_i)
        Vec3d v0c = centerStart;
        Vec3d v1c = ringStart[ni];
        Vec3d v2c = ringStart[i];
        Vec3d triN = (v1c - v0c).cross(v2c - v0c);
        if (triN.dot(capNormalStart) >= 0.0f) {
            outIndices.push_back(capStartCenterIndex);
            outIndices.push_back(ring_ni_index);
            outIndices.push_back(ring_i_index);
        } else {
            // flip ring order
            outIndices.push_back(capStartCenterIndex);
            outIndices.push_back(ring_i_index);
            outIndices.push_back(ring_ni_index);
        }
    }

    // End cap (near end): normal points +dir
    Vec3d centerEnd = end;
    Vec3d capNormalEnd = dir;

    unsigned int capEndCenterIndex = outVertices.size();
    outVertices.push_back(Vertex(centerEnd, color, capNormalEnd, centerUV));

    // push ring vertices for end cap (with cap normal and cap UV)
    for (int i = 0; i < segments; ++i) {
        Vec3d r_i = ringEnd[i] - end;
        Vec2d uv_i = Vec2d( 0.5f + 0.5f * (r_i.dot(side) / radius),  0.5f + 0.5f * (r_i.dot(up2) / radius) );
        outVertices.push_back(Vertex(ringEnd[i], color, capNormalEnd, uv_i));
    }

    // indices for end cap: ensure winding agrees with capNormalEnd
    for (int i = 0; i < segments; ++i) {
        int ni = (i + 1) % segments;
        unsigned int ring_i_index = capEndCenterIndex + 1 + i;
        unsigned int ring_ni_index = capEndCenterIndex + 1 + ni;

        // compute triangle normal for (center, ring_i, ring_ni)
        Vec3d v0e = centerEnd;
        Vec3d v1e = ringEnd[i];
        Vec3d v2e = ringEnd[ni];
        Vec3d triNe = (v1e - v0e).cross(v2e - v0e);
        if (triNe.dot(capNormalEnd) >= 0.0f) {
            outIndices.push_back(capEndCenterIndex);
            outIndices.push_back(ring_i_index);
            outIndices.push_back(ring_ni_index);
        } else {
            // flip ring order
            outIndices.push_back(capEndCenterIndex);
            outIndices.push_back(ring_ni_index);
            outIndices.push_back(ring_i_index);
        }
    }
}

void ShapeGenerator::createPlane(float width, float height, std::vector<Vertex>& outVertices, std::vector<unsigned int>& outIndices) {
    float w = width / 2.0f;
    float h = height / 2.0f;

    Vec3d positions[4] = {
        {-w, 0, -h}, {w, 0, -h}, {w, 0, h}, {-w, 0, h}
    };

    Vec2d uvs[4] = {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f}
    };

    Vec3d normal = {0,1,0};
    Vec3d color = {1,1,1};

    unsigned int startIndex = outVertices.size();
    for(int i=0;i<4;i++)
        outVertices.push_back({positions[i], color, normal, uvs[i]});

    // Ensure winding matches the normal (so back-face culling works)
    Vec3d v0 = positions[0];
    Vec3d v1 = positions[1];
    Vec3d v2 = positions[2];
    Vec3d a = v1 - v0;
    Vec3d b = v2 - v0;
    Vec3d cross = a.cross(b);
    float d = cross.dot(normal);
    if (d >= 0.0f) {
        outIndices.push_back(startIndex);
        outIndices.push_back(startIndex+1);
        outIndices.push_back(startIndex+2);

        outIndices.push_back(startIndex+2);
        outIndices.push_back(startIndex+3);
        outIndices.push_back(startIndex);
    } else {
        outIndices.push_back(startIndex);
        outIndices.push_back(startIndex+2);
        outIndices.push_back(startIndex+1);

        outIndices.push_back(startIndex);
        outIndices.push_back(startIndex+3);
        outIndices.push_back(startIndex+2);
    }
}

void ShapeGenerator::createPyramid(float size, float height, std::vector<Vertex>& outVertices, std::vector<unsigned int>& outIndices) {
    float h = size / 2.0f;
    Vec3d top = {0,height/2.0f,0};

    Vec3d base[4] = {
        {-h,-height/2.0f,-h}, {h,-height/2.0f,-h}, {h,-height/2.0f,h}, {-h,-height/2.0f,h}
    };

    Vec2d baseUVs[4] = {
        {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}
    };

    Vec3d color = {1,1,1};

    // base
    unsigned int startIndex = outVertices.size();
    for(int i=0;i<4;i++)
        outVertices.push_back({base[i], color, {0,-1,0}, baseUVs[i]});

    outIndices.push_back(startIndex);
    outIndices.push_back(startIndex+1);
    outIndices.push_back(startIndex+2);
    outIndices.push_back(startIndex+2);
    outIndices.push_back(startIndex+3);
    outIndices.push_back(startIndex);

    // sides
    for(int i=0;i<4;i++){
        unsigned int idx = outVertices.size();
        Vec3d next = base[(i+1)%4];
        Vec2d uv0 = {0.0f, 0.0f};
        Vec2d uv1 = {1.0f, 0.0f};
        Vec2d uv2 = {0.5f, 1.0f};

        Vec3d edge1 = next - base[i];
        Vec3d edge2 = top - base[i];
        Vec3d normal = edge1.cross(edge2).normalized();

        outVertices.push_back({base[i], color, normal, uv0});
        outVertices.push_back({next,   color, normal, uv1});
        outVertices.push_back({top,    color, normal, uv2});

        outIndices.push_back(idx);
        outIndices.push_back(idx+1);
        outIndices.push_back(idx+2);
    }
}

void ShapeGenerator::createSphere(float radius, int segments, int rings, std::vector<Vertex>& outVertices, std::vector<unsigned int>& outIndices) {
    Vec3d color = {1,1,1};
    for(int y=0;y<=rings;y++){
        for(int x=0;x<=segments;x++){
            float xSegment = (float)x / segments;
            float ySegment = (float)y / rings;
            float xPos = radius * NMATH::cos(xSegment * 2.0f * PI) * NMATH::sin(ySegment * PI);
            float yPos = radius * NMATH::cos(ySegment * PI);
            float zPos = radius * NMATH::sin(xSegment * 2.0f * PI) * NMATH::sin(ySegment * PI);

            Vec3d pos = {xPos,yPos,zPos};
            Vec3d normal = pos.normalized();
            Vec2d texCoord = {xSegment, ySegment};

            outVertices.push_back({pos,color,normal,texCoord});
        }
    }

    for(int y=0;y<rings;y++){
        for(int x=0;x<segments;x++){
            int i0 = y * (segments+1) + x;
            int i1 = i0 + segments +1;
            int i2 = i1 +1;
            int i3 = i0 +1;

            outIndices.push_back(i0);
            outIndices.push_back(i1);
            outIndices.push_back(i2);

            outIndices.push_back(i0);
            outIndices.push_back(i2);
            outIndices.push_back(i3);
        }
    }
}

void ShapeGenerator::createCone(const Vec3d& baseCenter, const Vec3d& tip, float baseRadius, int segments, std::vector<Vertex>& outVertices, std::vector<unsigned int>& outIndices) {
    if (segments < 3) segments = 3;

    Vec3d dir = (tip - baseCenter).normalized();

    Vec3d up(0, 1, 0);
    if (absf(dir.dot(up)) > 0.99f) up = Vec3d(1, 0, 0);

    Vec3d side = dir.cross(up).normalized();
    Vec3d up2 = side.cross(dir).normalized();

    // precompute ring points and u coords
    std::vector<Vec3d> ring(segments);
    std::vector<float> ucoords(segments);
    for (int i = 0; i < segments; ++i) {
        float theta = 2.0f * PI * (float)i / (float)segments;
        float c = cosf(theta);
        float s = sinf(theta);
        Vec3d radial = side * c + up2 * s;
        radial = radial.normalized();
        ring[i] = baseCenter + radial * baseRadius;
        ucoords[i] = (float)i / (float)segments;
    }

    Vec3d color = {1,1,1};

    // SIDES (flat-shaded): each triangle gets its own vertices with the triangle normal
    for (int i = 0; i < segments; ++i) {
        int ni = (i + 1) % segments;

        // compute triangle normal (tip, ring[i], ring[ni])
        Vec3d v0 = tip;
        Vec3d v1 = ring[i];
        Vec3d v2 = ring[ni];
        Vec3d triN = (v1 - v0).cross(v2 - v0);
        Vec3d triNormal = triN.normalized();

        // choose winding so normal points outward (compare with radial direction)
        float outwardTest = triN.dot((ring[i] - baseCenter));
        unsigned int startIndex = outVertices.size();

        if (outwardTest >= 0.0f) {
            // tip, ring[i], ring[ni]
            outVertices.push_back(Vertex(v0, color, triNormal, Vec2d(0.5f, 1.0f)));
            outVertices.push_back(Vertex(v1, color, triNormal, Vec2d(ucoords[i], 0.0f)));
            outVertices.push_back(Vertex(v2, color, triNormal, Vec2d(ucoords[ni], 0.0f)));

            outIndices.push_back(startIndex);
            outIndices.push_back(startIndex+1);
            outIndices.push_back(startIndex+2);
        } else {
            // flipped winding
            outVertices.push_back(Vertex(v0, color, -triNormal, Vec2d(0.5f, 1.0f)));
            outVertices.push_back(Vertex(v2, color, -triNormal, Vec2d(ucoords[ni], 0.0f)));
            outVertices.push_back(Vertex(v1, color, -triNormal, Vec2d(ucoords[i], 0.0f)));

            outIndices.push_back(startIndex);
            outIndices.push_back(startIndex+1);
            outIndices.push_back(startIndex+2);
        }
    }

    // BASE CAP: fan triangles around baseCenter, normal should point opposite to dir
    Vec3d capNormal = dir * -1.0f;
    Vec2d centerUV = {0.5f, 0.5f};
    unsigned int capCenterIndex = outVertices.size();
    outVertices.push_back(Vertex(baseCenter, color, capNormal, centerUV));

    // push ring verts for cap with capNormal and cap UVs
    for (int i = 0; i < segments; ++i) {
        Vec3d r_i = ring[i] - baseCenter;
        Vec2d uv_i = Vec2d( 0.5f + 0.5f * (r_i.dot(side) / baseRadius),  0.5f + 0.5f * (r_i.dot(up2) / baseRadius) );
        outVertices.push_back(Vertex(ring[i], color, capNormal, uv_i));
    }

    // indices for cap: ensure winding matches capNormal
    for (int i = 0; i < segments; ++i) {
        int ni = (i + 1) % segments;
        unsigned int ring_i_index = capCenterIndex + 1 + i;
        unsigned int ring_ni_index = capCenterIndex + 1 + ni;

        // triangle (center, ring_ni, ring_i) should produce normal close to capNormal
        Vec3d v0c = baseCenter;
        Vec3d v1c = ring[ni];
        Vec3d v2c = ring[i];
        Vec3d triNc = (v1c - v0c).cross(v2c - v0c);

        if (triNc.dot(capNormal) >= 0.0f) {
            outIndices.push_back(capCenterIndex);
            outIndices.push_back(ring_ni_index);
            outIndices.push_back(ring_i_index);
        } else {
            // flip ring order
            outIndices.push_back(capCenterIndex);
            outIndices.push_back(ring_i_index);
            outIndices.push_back(ring_ni_index);
        }
    }
}

unsigned int loadTexture(const char* path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrChannels;
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
    if (data) {
        GLenum format = (nrChannels == 3) ? GL_RGB : GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format,
                     GL_UNSIGNED_BYTE, data);

        // filtering & wrapping
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        std::cerr << "Failed to load texture: " << path << std::endl;
    }
    stbi_image_free(data);
    return textureID;
}
