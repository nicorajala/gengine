#include "Engine/objects/object.hpp"
#include <algorithm> // std::max
#include <iostream>

Object::Object() {
    position = Vec3d(0.0f);
    rotation = Vec3d(0.0f);
    scale    = Vec3d(1.0f);
    name = "Unnamed object";
    VAO = VBO = EBO = 0;
    textureID = 0;

    collisionEnabled = true;
    isStatic = false;
    physicsBody = nullptr;

    // hierarchy
    parent = nullptr;
    children.clear();
}

Object::~Object() {
    detachFromParent();
    removeAllChildren(true);

    // Safely release GL resources if they were created.
    if (textureID != 0) {
        if (glIsTexture(textureID)) glDeleteTextures(1, &textureID);
        textureID = 0;
    }
    if (VAO != 0) {
        if (glIsVertexArray(VAO)) glDeleteVertexArrays(1, &VAO);
        VAO = 0;
    }
    if (VBO != 0) {
        if (glIsBuffer(VBO)) glDeleteBuffers(1, &VBO);
        VBO = 0;
    }
    if (EBO != 0) {
        if (glIsBuffer(EBO)) glDeleteBuffers(1, &EBO);
        EBO = 0;
    }
}

void Object::addChild(Object* child) {
    if (!child) return;
    // avoid duplicates
    for (auto c : children) if (c == child) return;
    children.push_back(child);
    child->parent = this;
}

void Object::removeChild(Object* child) {
    if (!child) return;
    for (auto it = children.begin(); it != children.end(); ++it) {
        if (*it == child) {
            children.erase(it);
            child->parent = nullptr;
            return;
        }
    }
}

void Object::detachFromParent() {
    if (parent) {
        parent->removeChild(this);
        parent = nullptr;
    }
}

void Object::removeAllChildren(bool recursive) {
    for (auto* c : children) {
        if (c) {
            c->parent = nullptr;
            if (recursive) {
                c->removeAllChildren(true);
                delete c;
            }
        }
    }
    children.clear();
}

void Object::setParent(Object* newParent) {
    if (newParent == this || isAncestorOf(newParent)) return;
    detachFromParent();
    if (newParent) newParent->addChild(this);
}

bool Object::isAncestorOf(const Object* possibleDescendant) const {
    if (!possibleDescendant) return false;
    const Object* p = possibleDescendant->parent;
    while (p) {
        if (p == this) return true;
        p = p->parent;
    }
    return false;
}

bool Object::isDescendantOf(const Object* possibleAncestor) const {
    if (!possibleAncestor) return false;
    const Object* p = parent;
    while (p) {
        if (p == possibleAncestor) return true;
        p = p->parent;
    }
    return false;
}

Object* Object::getRoot() {
    Object* root = this;
    while (root->parent) root = root->parent;
    return root;
}

void Object::getAllDescendants(std::vector<Object*>& out) const {
    for (auto* c : children) {
        if (c) {
            out.push_back(c);
            c->getAllDescendants(out);
        }
    }
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
    // Create GL objects if needed
    if (VAO == 0) glGenVertexArrays(1, &VAO);
    if (VBO == 0) glGenBuffers(1, &VBO);
    if (EBO == 0) glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    // Use .data() safely: it may return nullptr for empty vectors; pass size 0 in that case.
    const void* vtxData = vertices.empty() ? nullptr : vertices.data();
    GLsizeiptr vtxSize = static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex));
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vtxSize, vtxData, GL_STATIC_DRAW);

    const void* idxData = indices.empty() ? nullptr : indices.data();
    GLsizeiptr idxSize = static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idxSize, idxData, GL_STATIC_DRAW);

    // If there's no vertex data we still set attribute pointers to the expected layout
    // so draw calls that are guarded by indices.size() won't crash.
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));

    // Unbind VAO to avoid accidental state changes
    glBindVertexArray(0);
    // leave buffers bound state to driver's discretion
}

Mat4 Object::getModelMatrix() const {
    // local transform
    Mat4 model = Mat4::identity();
    model = translate(model, position);
    model = rotate(model, radians(rotation.x), {1,0,0});
    model = rotate(model, radians(rotation.y), {0,1,0});
    model = rotate(model, radians(rotation.z), {0,0,1});
    model = NMATH::scale(model, scale);

    // compose with parent world transform if present
    if (parent) {
        Mat4 parentWorld = parent->getModelMatrix();
        return parentWorld * model;
    }
    return model;
}


void Object::texture(const std::string& path) {
    if (path.empty()) return;

    texturePath = path;

    if (textureID != 0) {
        if (glIsTexture(textureID)) glDeleteTextures(1, &textureID);
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
        // Keep textureID as created but empty; caller can test textureID.
    }
    stbi_image_free(data);
}

void Object::draw() const {
    // Guard against invalid draw calls
    if (VAO == 0 || indices.empty()) return;
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
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
        float d = NMATH::sqrt(dx * dx + dy * dy + dz * dz);
        if (d > maxDist) maxDist = d;
    }
    if (maxDist <= 0.0f) {
        // fallback radius for empty meshes
        maxDist = 0.5f;
    }
    // account for non-uniform scaling by using the largest scale component's absolute value
    float s = max(max(absf(scale.x), absf(scale.y)), absf(scale.z));
    if (s <= 0.0f) s = 1.0f;
    return maxDist * s;
}