#include "Player.hpp"
#include <SDL2/SDL.h>
#include <string>
#include <iostream>

Player::~Player() {

	scene = nullptr;
	playerObject = nullptr;
	cameraTargetObject = nullptr;
	cameraController = nullptr;
}

void Player::OnStart() {
	
}

void Player::OnUpdate(float dt) {
	const Uint8* keys = SDL_GetKeyboardState(NULL);

	if (rb) {
		if (keys[SDL_SCANCODE_SPACE]) {
			if (scene && scene->physics.isOnGround(rb, 0.05)) {
				rb->vel.y = jumpSpeed;
			}
		}
	}

	// keep orientation in object if present
	if (playerObject) playerObject->rotation.y = mainCamera.yaw;

	// If camera target object is set, override camera position to match target object's world position.
	if (cameraTargetObject) {
		Mat4 camModel = cameraTargetObject->getModelMatrix();
		Vec3d camWorldPos = camModel.transformPoint(Vec3d(0.0f, 0.0f, 0.0f));
		mainCamera.position = camWorldPos;
	}
}
