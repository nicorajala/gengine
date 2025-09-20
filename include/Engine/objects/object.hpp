#ifndef OBJECT_H
#define OBJECT_H

#include <vector>
#include "glad/glad.h"
#include "Engine/objects/shapegen.hpp"

#include "math/math.hpp"
using namespace NMATH;

namespace Physics { struct RigidBody; }

class Object;

class Object {
public:
	Vec3d position;
	Vec3d rotation;             // Euler angles
	std::string type;
	Vec3d scale;
	std::string name;

	unsigned int VAO, VBO, EBO;
	unsigned int textureID;
	std::string texturePath;

	bool collisionEnabled;
	bool isStatic;

	// Pointer to physics body (managed by Physics::World). May be nullptr.
	Physics::RigidBody* physicsBody;

	Object* parent;                      // parent object (nullptr if root)
	std::vector<Object*> children;       // child objects

	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;

	Object();
	virtual ~Object();

	void addChild(Object* child);
	void removeChild(Object* child);

	void detachFromParent();
	void removeAllChildren(bool recursive = false);
	void setParent(Object* neParent);
	bool isAncestorOf(const Object* possibleDescendant) const;
	bool isDescendantOf(const Object* possibleAncestor) const;
	Object* getRoot();
	void getAllDescendants(std::vector<Object*>& outDescendants) const;

	void initCube(float size);
	void initCylinder(float radius, float height, int segments);
	void initPlane(float width, float height);
	void initSphere(float radius, int segments, int rings);
	void initPyramid(float size, float height);
	void initCone(float baseRadius, float height, int segments);

	void setupMesh();
	Mat4 getModelMatrix() const; // returns world model matrix (composed with parents)

	void texture(const std::string& path);
	void draw() const;
	float boundingRadius() const;
};

#endif
