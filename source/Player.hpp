#ifndef PLAYER_HPP
#define PLAYER_HPP

#include "Engine/sceneManager.hpp"
#include "Engine/camera/camera.hpp"
#include "Engine/camera/cameraController.hpp"
#include "Engine/physics/physics.hpp"

#include "math/math.hpp"
using namespace NMATH;

class Player {
public:
    ~Player();

    void Start();
    void Update(float dt);

    // If set, Player controls this scene object. If nullptr, a mesh-less cameraBody is used.
    Object* playerObject = nullptr;

    // Optional mesh-less rigidbody used when no playerObject is present.
    Physics::RigidBody* cameraBody = nullptr;

    Camera mainCamera;
    CameraController* cameraController = nullptr;

    // Scene reference (needed to create physics bodies)
    SceneManager* scene = nullptr;

    // Movement tuning
    float moveSpeed = 5.0f;   // meters/sec
    float jumpSpeed = 5.0f;   // upward impulse velocity

    // Camera-body tuning (used for mesh-less camera rigidbody)
    double bodyRadius = 0.3;
    double bodyHeight = 1.8; // used for cylinder collider
    int bodyColliderType = Physics::COLLIDER_CYLINDER;

    Object* cameraTargetObject = nullptr;
};

#endif
