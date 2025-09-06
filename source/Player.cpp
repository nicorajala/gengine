#include "Player.hpp"
#include <SDL2/SDL.h>
#include <string>
#include <iostream>

void Player::Start()
{
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
	} else if (scene) {
		// camera-only body: cylinder collider by default for better stepping.
		// bodyRadius, bodyHeight come from Player instance defaults and can be tuned.
		Physics::RigidBody* cb = scene->physics.createBody(nullptr, false, 1.0, bodyRadius, mainCamera.position, Physics::COLLIDER_CYLINDER, bodyHeight);
		cameraBody = cb;
		if (cameraBody) cameraBody->vel = Vec3d(0.0);

		// If controller supports attaching a body, attach it so controller drives the body's horizontal velocity.
		if (cameraController) {
			cameraController->setPhysicsBody(cameraBody);
		}
	}
}

void Player::Update(float dt)
{
	// choose the active rigid body (owned or camera-only)
	Physics::RigidBody* rb = nullptr;
	if (playerObject) rb = playerObject->physicsBody;
	else rb = cameraBody;

	// ensure cameraController exists
	if (!cameraController) return;

	// Input handling (mouse/keyboard)
	int dx = 0, dy = 0;
	cameraController->onMouseMotion(dx, dy);
	const Uint8* keys = SDL_GetKeyboardState(NULL);
	cameraController->onKeyboard(keys, dt);

	// capture camera pos before controller moves it
	Vec3d prevCamPos = mainCamera.position;
	cameraController->update(mainCamera, dt);
	Vec3d intended = mainCamera.position - prevCamPos;

	// If we have a physics body, controller already wrote target horizontal velocity into it.
	// here we only handle jump + sync camera position from body.
	if (rb) {
		if (keys[SDL_SCANCODE_SPACE]) {
			if (scene && scene->physics.isOnGround(rb, 0.05)) {
				rb->vel.y = jumpSpeed;
			}
		}

		// Sync camera to body position (body is authoritative; physics step updates it)
		Vec3d bodyPos = rb->owner ? rb->owner->position : rb->position;
		mainCamera.position = bodyPos;
		if (rb->owner) playerObject->position = bodyPos;
	} else {
		// no physics: keep previous behavior
		if (keys[SDL_SCANCODE_SPACE]) mainCamera.position.y += jumpSpeed * dt;
		if (playerObject) playerObject->position = mainCamera.position;
	}

	// keep orientation in object if present
	if (playerObject) playerObject->rotation.y = mainCamera.yaw;
}
