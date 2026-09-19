#pragma once

#include "bakkesmod/wrappers/GameEvent/ServerWrapper.h"
#include "bakkesmod/wrappers/GameObject/BallWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarWrapper.h"
#include "bakkesmod/wrappers/GameWrapper.h"

class GameContext {
public:
    static ServerWrapper server(GameWrapper& game);
    static CarWrapper localCar(GameWrapper& game);
    static BallWrapper ball(GameWrapper& game);
    static BallWrapper ball(GameWrapper& game, ServerWrapper server);
    static bool isNetworked(GameWrapper& game);
    static bool isNetworked(GameWrapper& game, ServerWrapper server);
};
