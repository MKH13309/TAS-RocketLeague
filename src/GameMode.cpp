#include "GameMode.h"
#include "GameContext.h"

#include "bakkesmod/wrappers/gfx/GfxDataTrainingWrapper.h"

GameModeInfo GameMode::current(GameWrapper& game) {
    if (game.IsInReplay()) {
        return {};
    }

    auto currentServer = GameContext::server(game);
    GameModeInfo info;
    const bool freeplay = game.IsInFreeplay();
    const bool customTraining = game.IsInCustomTraining() ||
        (currentServer && !freeplay && currentServer.IsPlayingTraining());

    if (customTraining) {
        info.id = "custom_training";
        auto training = game.GetGfxTrainingData();
        if (training) {
            info.trainingShot = training.GetCurrentPlaylistindex();
        }
    } else if (freeplay) {
        info.id = "freeplay";
    } else if (currentServer && !GameContext::isNetworked(game, currentServer)) {
        info.id = "exhibition";
    }

    if (!info.id.empty() && currentServer) {
        info.matchType = currentServer.GetMatchTypeName();
    }
    return info;
}

bool GameMode::isSupported(GameWrapper& game) {
    return !current(game).id.empty();
}

bool GameMode::requireSupported(GameWrapper& game, std::string& error) {
    if (game.IsInReplay()) {
        error = "TAS is disabled while viewing replays";
        return false;
    }
    if (isSupported(game)) {
        return true;
    }
    if (GameContext::isNetworked(game)) {
        error = "TAS is disabled in online and LAN matches";
        return false;
    }
    error = "No supported local car, ball, and game state are available";
    return false;
}

std::string GameMode::displayName(const std::string& mode) {
    if (mode == "custom_training") {
        return "Custom training";
    }
    if (mode == "exhibition") {
        return "Exhibition";
    }
    if (mode == "freeplay") {
        return "Freeplay";
    }
    return "Unsupported";
}
