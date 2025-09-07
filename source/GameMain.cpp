#include "GameMain.hpp"
#include <cstdio>
#include "Engine/objects/shapegen.hpp"

GameMain::GameMain() {
	scene = new SceneManager();

	Light dirLight(LightType::Directional, Vec3d(1,1,1), 1.0f);
	dirLight.direction = Vec3d(-0.2f,-1.0f,-0.3f);
	scene->addLight(dirLight);

	cube1 = scene->addObject("Cube", "Cube_1");
	cube1->position = Vec3d(-5.f, 0.f, 0.f);
	cube1->texture("textures/peppa.png");

	// Two ways of adding objects to a scene from c++
	sphere1 = scene->addObject("Sphere", "Spheree");
	sphere1->position = Vec3d(-5.f, 0.f, 5.f);
	sphere1->scale = Vec3d(1.f);
	sphere1->texture("textures/yoda.png");

	floor = scene->addObject("Plane", "Floor");
	floor->position = Vec3d(0.f, -1.f, 0.f);
	floor->scale = Vec3d(50.f);
	floor->texture("textures/yoda2.png");
	floor->physicsBody->isStatic = true;
	floor->physicsBody->height = 1;

	Object* cylinder = new Object();
	cylinder->initCylinder(0.5f, 2.0f, 16);
	cylinder->name = "cylinderi";
	cylinder->position = Vec3d(-2.f, 0.f, 2.f);
	cylinder->texture("textures/yoda.png");
	cylinder->physicsBody = scene->physics.createBody(cylinder, false, 1.0, 0.5, cylinder->position, Physics::COLLIDER_CYLINDER, 2.0);
	scene->objects.push_back(cylinder);

	Light pointLight(LightType::Point, Vec3d(1,1,1), 1.0f);
	pointLight.position = Vec3d(5,0,0);
	scene->addLight(pointLight);

	player = new Player();
}

GameMain::~GameMain()
{
	delete player;

	if (scene) {
		delete scene;
		scene = NULL;
	}
}

void GameMain::Start()
{
	// Log object count for debugging
	int objCount = (int)scene->objects.size();
	printf("[GameMain] Scene has %d objects\n", objCount);

	// Ensure scene reference on player
	player->scene = scene;

	Object* foundPlayer = nullptr;
	for (auto o : scene->objects) {
		if (o && o->type == "Player") {
			foundPlayer = o;
			break; // first Player object wins
		}
	}

	if (foundPlayer) {
		player->playerObject = foundPlayer;
		// ensure physics body exists for player object
		scene->recreatePhysicsBody(foundPlayer);
		std::cerr << "[GameMain] Attached player to object '" << foundPlayer->name << "'\n";

		// create a camera target child object for third-person camera control
		Object* camTarget = new Object();
		camTarget->name = foundPlayer->name + std::string("_CameraTarget");
		camTarget->type = "CameraTarget";
		// local offset behind and above player by default (local coordinates)
		camTarget->position = Vec3d(0.0f, 1.8f, -3.0f);
		camTarget->scale = Vec3d(1.0f);
		// add camTarget to scene AFTER we've finished iterating objects to avoid invalidating iterators
		scene->objects.push_back(camTarget);
		scene->setParent(camTarget, foundPlayer);

		// tell Player about it
		player->cameraTargetObject = camTarget;
	}

	// Recreate floor body now that scale was set in constructor
	if (floor) {
		scene->recreatePhysicsBody(floor);
		if (floor->physicsBody) floor->physicsBody->isStatic = true;
	}

	// Now initialize the player (will create camera or object body)
	player->Start();
}

void GameMain::Update(float dt)
{
	player->Update(dt);

	if (scene) scene->update(dt);

	static float debugAccum = 0.0f;
	debugAccum += dt;
	if (debugAccum >= 1.0f) {
		debugAccum = 0.0f;
		int bi = 0;
		for (auto b : scene->physics.bodies_) {
			std::string ownerName = (b->owner ? b->owner->name : std::string("<none>"));
			Vec3d pos = b->owner ? b->owner->position : b->position;
		}
	}

	float aspect = 16.0f / 9.0f; // compute from your viewport size
	Mat4 view = player->mainCamera.getViewMatrix();
	Mat4 proj = player->mainCamera.getProjectionMatrix(aspect);

	// get the program you want to render with and call scene render with camera matrices
	GLuint prog = scene->getActiveProgram();
	scene->render(prog, view, proj);
}