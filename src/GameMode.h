#pragma once

#include "bakkesmod/wrappers/GameWrapper.h"

#include <string>

struct GameModeInfo {
    std::string id;
    std::string matchType;
    int trainingShot{-1};
};

class GameMode {
public:
    static GameModeInfo current(GameWrapper& game);
    static bool isSupported(GameWrapper& game);
    static bool requireSupported(GameWrapper& game, std::string& error);
    static std::string displayName(const std::string& mode);
};
