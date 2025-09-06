#include "Engine/objects/object.hpp"

Object::Object() {
    position = Vec3d(0.0f);
    rotation = Vec3d(0.0f);
    scale    = Vec3d(1.0f);
    name = "Unnamed object";
    VAO = VBO = EBO = 0;
    textureID = 0;

    collisionEnabled = true;
    physicsBody = nullptr;
}

void Object::initCube(float size) {
    ShapeGenerator::createCube(size, vertices, indices);
    type="Cube";
    setupMesh();
}

void Object::initCylinder(float radius, float height, int segments) {
    Vec3d start(0.0f, -height/2.0f, 0.0f);
    Vec3d end(0.0f, height/2.0f, 0.0f);
    ShapeGenerator::createCylinder(start, end, radius, segments, vertices, indices);
    type="Cylinder";
    setupMesh();
}

void Object::initPlane(float width, float height) {
    ShapeGenerator::createPlane(width, height, vertices, indices);
    type="Plane";
    setupMesh();
}

void Object::initSphere(float radius, int segments, int rings) {
    ShapeGenerator::createSphere(radius, segments, rings, vertices, indices);
    type="Sphere";
    setupMesh();
}

void Object::initPyramid(float size, float height) {
    ShapeGenerator::createPyramid(size, height, vertices, indices);
    type="Pyramid";
    setupMesh();
}

void Object::initCone(float baseRadius, float height, int segments) {
    Vec3d baseCenter(0.0f, -height/2.0f, 0.0f);
    Vec3d tip(0.0f, height/2.0f, 0.0f);
    ShapeGenerator::createCone(baseCenter, tip, baseRadius, segments, vertices, indices);
    type="Cone";
    setupMesh();
}

void Object::setupMesh() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
     
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

    // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    // Color
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
    glEnableVertexAttribArray(1);
    // Normal
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);
    // TexCoord
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));
    glEnableVertexAttribArray(3);
}

Mat4 Object::getModelMatrix() const {
    Mat4 model = Mat4::identity();
    model = translate(model, position);
    model = rotate(model, radians(rotation.x), {1,0,0});
    model = rotate(model, radians(rotation.y), {0,1,0});
    model = rotate(model, radians(rotation.z), {0,0,1});
    model = NMATH::scale(model, scale);
    return model;
}


void Object::texture(const std::string& path) {
    if (path.empty()) return;

    texturePath = path;

    if (textureID != 0) {
        glDeleteTextures(1, &textureID);
        textureID = 0;
    }

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    unsigned char *data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);
    if (data) {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        std::cerr << "[Object] Loaded texture '" << path << "' -> id=" << textureID << " (" << width << "x" << height << ", ch=" << nrChannels << ")" << std::endl;
    } else {
        std::cerr << "Failed to load texture: " << path.c_str() << std::endl;
        std::cerr << "stbi_failure_reason: " << stbi_failure_reason() << std::endl;
        return;
    }
    stbi_image_free(data);
}

void Object::draw() const {
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
}

float Object::boundingRadius() const {
    // Compute a bounding radius from mesh vertex positions (in object local space),
    // and then account for object scale.
    float maxDist = 0.0f;
    for (size_t i = 0; i < vertices.size(); ++i) {
        const Vertex& v = vertices[i];
        // assume Vertex has a 'position' Vec3d member
        float dx = v.pos.x;
        float dy = v.pos.y;
        float dz = v.pos.z;
        float d = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (d > maxDist) maxDist = d;
    }
    if (maxDist <= 0.0f) {
        // fallback radius for empty meshes
        maxDist = 0.5f;
    }
    // account for non-uniform scaling by using the largest scale component's absolute value
    float s = std::max(std::max(absf(scale.x), absf(scale.y)), absf(scale.z));
    if (s <= 0.0f) s = 1.0f;
    return maxDist * s;
}