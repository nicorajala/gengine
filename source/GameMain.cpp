#include "GameMain.hpp"
#include <cstdio>
#include "Engine/objects/shapegen.hpp"

GameMain::GameMain() {
	scene = new SceneManager();

	Light dirLight(LightType::Directional, Vec3d(1,1,1), 1.0f);
	dirLight.direction = Vec3d(-0.2f,-1.0f,-0.3f);
	scene->addLight(dirLight);

	// Different ways of adding objects to a scene from c++ (can and should be created in Start(), this is just for demo)
	// Any objects created in the constructor must be deleted in the destructor
	// Also any objects created here appear in the editor automatically. The ones created in Start() do not.

	//cube1 = scene->addObject("Cube", "Test cube");	// This is the simplest way of adding an object (remember to keep a pointer in the header)

	//sphere1 = scene->addObject("Sphere", "Spheree");	// After creation you can modify different properties
	//sphere1->position = Vec3d(-5.f, 0.f, 5.f);		// Move it somewhere
	//sphere1->scale = Vec3d(1.f);						// Scale it
	//sphere1->texture("textures/texture-image.png");	// Apply a texture etc.


	// You can also create an object manually. This way is just what scene->addObject does internally.
	//Object* cylinder = new Object;					// Create the object pointer. Should be done in the header file
	//cylinder->initCylinder(radius, height, segments);	// You need to initialize its mesh
														// By doing the initialization manually you can customize the meshes
														// parameters on creation which gives you more flexibility than scene->addObject
	//cylinder->name = "cylinder";						// After that you can set its properties just like any other object
	//cylinder->position = Vec3d(-2.f, 0.f, 2.f);
	//cylinder->texture("textures/texture-image.png");
	//cylinder->physicsBody = scene->physics.createBody(cylinder,
	// false, 1.0, 0.5, cylinder->position, Physics::COLLIDER_CYLINDER, 2.0);	// If you want physics on the object
																				// you need to create the physics body manually
																				// using the scene's physics
																				// Read more about the parameters in the github wiki (When I make one).
	//scene->objects.push_back(cylinder);				// Finally you need to add it to the scene's object list manually

	player = new Player();
}

GameMain::~GameMain()
{
	delete player;
	player = nullptr;

	if (scene) {
		delete scene;
		scene = NULL;
	}

	// Ensure you delete any objects created in the constructor

	// if(sphere1) { 
	//		delete sphere1;
	//		sphere1 = nullptr;
	// }
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
		camTarget->position = Vec3d(0.0f, 1.f, 0.f);
		camTarget->scale = Vec3d(1.0f);
		// add camTarget to scene AFTER we've finished iterating objects to avoid invalidating iterators
		scene->objects.push_back(camTarget);
		scene->setParent(camTarget, foundPlayer);

		// tell Player about it
		player->cameraTargetObject = camTarget;
	}
}

void GameMain::Update(float dt)
{
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