#ifndef PLAYER_HPP
#define PLAYER_HPP

#include "Engine/sceneManager.hpp"

#include "Engine/game.hpp"

#include "math/math.hpp"
using namespace NMATH;

class Player : public PlayerBase {
public:
    ~Player();

    void OnStart() override;
    void OnUpdate(float dt) override;

    Object* cameraTargetObject = nullptr;
};

#endif
