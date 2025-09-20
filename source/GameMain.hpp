#ifndef GAME_MAIN_HPP
#define GAME_MAIN_HPP

#include <Engine/game.hpp>

#include "Player.hpp"

class GameMain : public Game {
public:
    GameMain();
    ~GameMain();

    void Start();
    void Update(float dt);

    SceneManager* scene;
    Player* player;

    // When creating objects from code, it is recommended
    // to keep the object pointers in the header file

    // Object* sphere1;
};

#endif