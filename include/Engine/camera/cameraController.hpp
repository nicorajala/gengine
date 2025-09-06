#ifndef CAMERA_CONTROLLER_HPP
#define CAMERA_CONTROLLER_HPP

#include "Engine/camera/Camera.hpp"
#include "SDL2/SDL.h"
#include "Engine/physics/physics.hpp" // new: camera controller can own/drive a RigidBody

struct CameraController {
	virtual ~CameraController() {}

	virtual void onMouseMotion(int dx, int dy) = 0;
	virtual void onKeyboard(const Uint8* keyState, float dt) = 0;
	virtual void update(Camera& cam, float dt) = 0;

	// Optional: attach a physics body to be used by the controller.
	// Default implementation does nothing (not all controllers support physics bodies).
	virtual void setPhysicsBody(Physics::RigidBody* body) { (void)body; }
};

class FPSCameraController : public CameraController {
public:
    float moveSpeed;
    float mouseSensitivity;
    bool invertY;

    FPSCameraController()
        : moveSpeed(5.0f), mouseSensitivity(0.1f), invertY(false), physicsBody(nullptr) {
    }

    virtual void onMouseMotion(int dx, int dy) override {
        lastDx += dx;
        lastDy += dy;
    }

    virtual void onKeyboard(const Uint8* keyState, float dt) override {
        pendingKeys = keyState;
        pendingDt = dt;
    }

    virtual void update(Camera& cam, float dt) override {
        // apply rotation from mouse deltas
        if (lastDx != 0 || lastDy != 0) {
            cam.yaw += lastDx * mouseSensitivity;
            cam.pitch += (invertY ? 1 : -1) * lastDy * mouseSensitivity;
            if (cam.pitch > 89.9f) cam.pitch = 89.9f;
            if (cam.pitch < -89.9f) cam.pitch = -89.9f;
            lastDx = lastDy = 0;
        }

        // movement requested by keys (world-space displacement)
        Vec3d forward = cam.forward();
        Vec3d right = forward.cross(Vec3d(0.0f, 1.0f, 0.0f)).normalized();

        if (pendingKeys) {
            float s = moveSpeed * (pendingDt > 0.0f ? pendingDt : dt);

            Vec3d moveDelta(0.0f);
            if (pendingKeys[SDL_SCANCODE_W]) moveDelta = moveDelta + forward * s;
            if (pendingKeys[SDL_SCANCODE_S]) moveDelta = moveDelta - forward * s;
            if (pendingKeys[SDL_SCANCODE_A]) moveDelta = moveDelta - right * s;
            if (pendingKeys[SDL_SCANCODE_D]) moveDelta = moveDelta + right * s;
            if (pendingKeys[SDL_SCANCODE_Q]) moveDelta = moveDelta + Vec3d(0, 1, 0) * s;
            if (pendingKeys[SDL_SCANCODE_E]) moveDelta = moveDelta - Vec3d(0, 1, 0) * s;

            if (physicsBody) {
                // drive horizontal velocity on attached physics body (preserve vertical velocity)
                float invDt = (pendingDt > 1e-6f) ? (1.0f / pendingDt) : (1.0f / dt);
                // desired horizontal velocity = moveDelta / dt
                Vec3d desiredVel = physicsBody->vel;
                desiredVel.x = moveDelta.x * invDt;
                desiredVel.z = moveDelta.z * invDt;

                // clamp horizontal speed
                float horizSpeed = std::sqrt(desiredVel.x * desiredVel.x + desiredVel.z * desiredVel.z);
                if (horizSpeed > moveSpeed) {
                    float scale = moveSpeed / horizSpeed;
                    desiredVel.x *= scale;
                    desiredVel.z *= scale;
                }
                physicsBody->vel.x = desiredVel.x;
                physicsBody->vel.z = desiredVel.z;
            } else {
                cam.position = cam.position + moveDelta;
            }
        }
    }

    virtual void setPhysicsBody(Physics::RigidBody* body) override {
        physicsBody = body;
    }

private:
    int lastDx = 0, lastDy = 0;
    const Uint8* pendingKeys = nullptr;
    float pendingDt = 0.0f;

    // Attached mesh-less camera body (optional). Controller drives this body instead of cam.position.
    Physics::RigidBody* physicsBody;
};


inline CameraController* createDefaultFPSController() {
	return new FPSCameraController();
}

#endif