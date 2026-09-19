#include "GameContext.h"

#include "bakkesmod/wrappers/PlayerControllerWrapper.h"

ServerWrapper GameContext::server(GameWrapper& game) {
    auto current = game.GetCurrentGameState();
    if (current) {
        return current;
    }
    return game.GetGameEventAsServer();
}

CarWrapper GameContext::localCar(GameWrapper& game) {
    auto car = game.GetLocalCar();
    if (car) {
        return car;
    }
    auto controller = game.GetPlayerController();
    return controller ? controller.GetCar() : CarWrapper(0);
}

BallWrapper GameContext::ball(GameWrapper& game) {
    return ball(game, server(game));
}

BallWrapper GameContext::ball(GameWrapper&, ServerWrapper currentServer) {
    return currentServer ? currentServer.GetBall() : BallWrapper(0);
}

bool GameContext::isNetworked(GameWrapper& game) {
    return isNetworked(game, server(game));
}

bool GameContext::isNetworked(GameWrapper& game, ServerWrapper currentServer) {
    if (game.GetOnlineGame()) {
        return true;
    }
    if (!currentServer) {
        return game.IsInOnlineGame();
    }
    return currentServer.IsOnlineMultiplayer() ||
        currentServer.IsPlayingPublic() ||
        currentServer.IsPlayingPrivate() ||
        currentServer.IsPlayingLan();
}
