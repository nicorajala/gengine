#include "GameMain.hpp"
#include <cstdio>
#include "Engine/objects/shapegen.hpp"

GameMain::GameMain() {
	scene = new SceneManager();

	Light dirLight(LightType::Directional, Vec3d(1,1,1), 1.0f);
	dirLight.direction = Vec3d(-0.2f,-1.0f,-0.3f);
	scene->addLight(dirLight);

	cube1 = scene->addObject("Player", "Cube_1");
	cube1->position = Vec3d(-5.f, 0.f, 0.f);
	cube1->texture("textures/peppa.png");

	// Two ways of adding objects to a scene from c++
	sphere1 = scene->addObject("Sphere", "Player");
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
			std::cerr << "[Physics] body[" << bi++ << "] owner='" << ownerName
				<< "' static=" << (b->isStatic ? "yes" : "no")
				<< " mass=" << b->mass
				<< " collider=" << (b->colliderType == Physics::COLLIDER_CYLINDER ? "CYL" : "SPH")
				<< " rad=" << b->radius << " h=" << b->height
				<< " pos=(" << pos.x << "," << pos.y << "," << pos.z << ")\n";
		}
	}

	float aspect = 16.0f / 9.0f; // compute from your viewport size
	Mat4 view = player->mainCamera.getViewMatrix();
	Mat4 proj = player->mainCamera.getProjectionMatrix(aspect);

	// get the program you want to render with and call scene render with camera matrices
	GLuint prog = scene->getActiveProgram();
	scene->render(prog, view, proj);
}