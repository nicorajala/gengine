#ifndef SCENEMANAGER_HPP
#define SCENEMANAGER_HPP

#include "Engine/objects/object.hpp"
#include "Engine/lighting/light.hpp"
#include "Engine/lighting/shadow.hpp"
#include "Engine/gizmos/transformTool.hpp"
#include "Engine/physics/physics.hpp" // ensure physics types visible

#include "glad/glad.h"
#include "nlohmann/json.hpp"

#include <cfloat>
#include <string>
#include <vector>

#include "math/math.hpp"

#include <fstream>

using json = nlohmann::json;
using namespace NMATH;

struct GizmoAxis {
	Vec3d start;
	Vec3d end;
	Vec3d color;
	Vec3d dir;

	// Default constructor so SceneManager can be default-constructed
	GizmoAxis()
		: start(Vec3d(0.0)), end(Vec3d(0.0)), color(Vec3d(0.0)), dir(Vec3d(0.0)) {}

	GizmoAxis(const Vec3d& s, const Vec3d& e, const Vec3d& c, const Vec3d& d)
		: start(s), end(e), color(c), dir(d) {}

	// State captured at pick time to make dragging stable
	float initialProj = 0.0f;
	Vec3d initialObjPos = Vec3d(0.0f);
};

class SceneManager {
public:
	SceneManager();
	std::vector<Object*> objects;
	std::vector<Light> lights;
	std::vector<Shadow*> lightShadows;

	GLuint shadowDepthProgram = 0;

	static const int MAX_DIR_SHADOWS = 4;

	Object* selectedObject;
	
	GizmoAxis grabbedAxis;
	bool axisGrabbed;
	int grabbedAxisIndex; // -1 if none, 0=X, 1=Y, 2=Z
	bool objectDrag;
	Vec3d dragPlaneNormal;
	Vec3d dragInitialPoint;
	Vec3d dragInitialObjPos;
	float axisGrabDistance;

	// Width in pixels for gizmo axis lines
	float gizmoLineWidth;

	GLuint axisVAO, axisVBO;
	GLuint lightVAO, lightVBO;
	int selectedLightIndex;

	int objCounter;

	void clearScene();

	void initLightGizmo();
	void drawGizmo(GLuint shaderProgram, const Mat4& view, const Mat4& projection);
	bool pickGizmoAxis(const Vec3d& rayOrigin, const Vec3d& rayDir, GizmoAxis& outAxis);
	bool pickLight(const Vec3d& rayOrigin, const Vec3d& rayDir, int& outIndex, float radius = 0.5f);
	void dragSelectedObject(const Vec3d& rayOrigin, const Vec3d& rayDir);

	Object* addObject(const std::string& type, const std::string& name);
	void removeObject(Object* objPtr);
	void removeLight(int index);
	void addLight(const Light& light);

	void setParent(Object* child, Object* parent); // set or clear parent; keeps children vectors consistent

	void update(float deltaTime);
	void render(GLuint shaderProgram, const Mat4& view, const Mat4& projection);
	Object* pickObject(const Vec3d& rayOrigin, const Vec3d& rayDir);

	GLuint getActiveProgram() const { return lastActiveProgram; }

	void initGrid(int gridSize = 20, float spacing = 1.0f);
	void drawGrid(GLuint shaderProgram, const Mat4& view, const Mat4& projection);

	void saveScene(const std::string& path);
	void loadScene(const std::string& path);

	// Debug: draw wireframe representation of collision shapes when true
	bool drawColliders = false;
	bool drawPlayerModel = true;

	// Ensure a physics body matches the current object (recreate with current scale/type/position).
	// Call after you change position/scale/type manually.
	void recreatePhysicsBody(Object* obj);

	// owned physics world
	Physics::World physics;

	enum class SkyboxMode {
		None,
		Color,
		Cubemap,
		Equirectangular
	};

	SkyboxMode getSkyboxMode() const { return skyboxMode;  }
	Vec3d getSkyboxColor() const { return skyboxColor; }
	GLuint getSkyboxCubemapID() const { return skyboxCubemapID; }
	const std::vector<std::string>& getSkyboxCubemapPaths() const { return skyboxCubemapPaths; }

	void setSkyboxColor(const Vec3d& color);
	bool setSkyboxCubemap(const std::vector<std::string>& facePaths);
	bool setSkyboxCubemapFrom3x2(const std::string& path);
	bool setSkyboxEquirectangular(const std::string& path);
	void clearSkybox();
	void setSkyboxMode(SkyboxMode mode);

private:
	GLuint gridVAO = 0, gridVBO = 0;
	int gridVertexCount = 0;
	GLuint lastActiveProgram = 0;

	SkyboxMode skyboxMode = SkyboxMode::None;
	Vec3d skyboxColor = Vec3d(0.5f, 0.7f, 1.0f);
	GLuint skyboxCubemapID = 0;
	GLuint skyboxEquirectID = 0;
	std::vector<std::string> skyboxCubemapPaths;

	GLuint skyboxVAO = 0, skyboxVBO = 0;

	// TODO: move to nsmlib?
	GLuint loadTextureFromFile(const std::string& path);
	GLuint loadCubemap(const std::vector<std::string>& faces);
};

#endif
