#ifndef GAME_HPP
#define GAME_HPP

#include <SDL2/SDL.h>

#include <Engine/sceneManager.hpp>
#include "Engine/camera/camera.hpp"
#include "Engine/camera/cameraController.hpp"
#include "Engine/physics/physics.hpp"

#include "math/math.hpp"
using namespace NMATH;

class Game {

public:
    virtual void Start() {}
    virtual void Update(float dt) {}
};

#endif

#ifndef PLAYER_BASE_HPP
#define PLAYER_BASE_HPP

class PlayerBase {
public:

    void Start() {
		mainCamera.position = Vec3d(0.f, 3.f, 0.f);
		mainCamera.yaw = -90.f;
		mainCamera.nearP = 0.25f;
		cameraController = createDefaultFPSController();

		// create a physics body (simple sphere) for the player object OR create a camera-only body.
		if (playerObject && scene) {
			double r = playerObject->boundingRadius();
			if (r <= 0.01) r = 0.5;
			Physics::RigidBody* rb = scene->physics.createBody(playerObject, false, 1.0, r);
			playerObject->physicsBody = rb;
			if (rb) rb->vel = Vec3d(0.0);
			// Attach physics body to controller if possible
			if (cameraController) {
				cameraController->setPhysicsBody(rb);
			}
		} else {
			rb = nullptr;
			if (cameraController) {
				cameraController->setPhysicsBody(nullptr);
			}
		}

		OnStart();
    }

    void Update(float dt) {
		// ensure cameraController exists
		if (!cameraController) return;

		// choose the active rigid body (owned or camera-only)
		if (playerObject) rb = playerObject->physicsBody;

		// Input handling (mouse/keyboard)
		int dx = 0, dy = 0;
		SDL_GetRelativeMouseState(&dx, &dy);

		cameraController->onMouseMotion(dx, dy);

		const Uint8* keys = SDL_GetKeyboardState(NULL);
		keys = SDL_GetKeyboardState(NULL);
		cameraController->onKeyboard(keys, dt);

        Vec3d prevCamPos = mainCamera.position;
        cameraController->update(mainCamera, dt);
        Vec3d intended = mainCamera.position - prevCamPos;

        if (rb) {
            Vec3d bodyPos = rb->owner ? rb->owner->position : rb->position;
            mainCamera.position = bodyPos;
            if (rb->owner) playerObject->position = bodyPos;
        }

		OnUpdate(dt);
    }

	virtual void OnStart() {}
	virtual void OnUpdate(float) {}

    Object* playerObject = nullptr;
    Physics::RigidBody* rb = nullptr;

    Camera mainCamera;
    CameraController* cameraController = nullptr;

    SceneManager* scene;

    // Movement tuning
    float moveSpeed = 5.0f;   // meters/sec
    float jumpSpeed = 5.0f;   // upward impulse velocity

    // Camera-body tuning (used for mesh-less camera rigidbody)
    double bodyRadius = 0.3;
    double bodyHeight = 1.8; // used for cylinder collider
    int bodyColliderType = Physics::COLLIDER_CYLINDER;
};

#endif