#pragma once

#include "Model.h"
#include "bakkesmod/wrappers/GameWrapper.h"

#include <cstddef>
#include <string>

class WorldState {
public:
    static bool capture(GameWrapper& game, TasData& tas, std::string& error);
    static bool restore(
        GameWrapper& game,
        const TasData& tas,
        std::size_t activePlayer,
        std::string& error
    );
    static bool restoreFrame(
        GameWrapper& game,
        const TasData& tas,
        const TasFrame& frame,
        std::size_t activePlayer,
        std::string& error
    );
    static bool validate(GameWrapper& game, const TasData& tas, std::string& error);
    static void setSessionProtection(GameWrapper& game, bool active);
    static void restoreReferenceBall(
        class BallWrapper ball,
        const RigidState& state
    );
    static bool captureFrame(
        GameWrapper& game,
        class CarWrapper localCar,
        const ControllerInput& input,
        const TasData& tas,
        const TasFrame* reference,
        bool continuePastReference,
        std::size_t activePlayer,
        BallTrackLock& ballLock,
        TasFrame& frame
    );
    static bool applyFrame(
        GameWrapper& game,
        class CarWrapper localCar,
        const TasData& tas,
        const TasFrame& frame,
        std::size_t activePlayer,
        ControllerInput& input
    );

private:
    static class CarWrapper otherCar(GameWrapper& game, class CarWrapper localCar);
    static void keepSessionAlive(GameWrapper& game);
    static void setCollision(class RBActorWrapper actor, bool enabled);
    static void setBallActive(class BallWrapper ball, bool collisionsEnabled);
    static RigidState captureRigid(class RBActorWrapper actor);
    static void applyRigid(class RBActorWrapper actor, const RigidState& state);
    static void applyBall(
        class BallWrapper ball,
        const RigidState& state,
        bool collisionsEnabled
    );
    static CarState captureCar(class CarWrapper car);
    static void applyCar(class CarWrapper car, const CarState& state);
    static Compatibility currentCompatibility(GameWrapper& game, class CarWrapper car);
    static std::string classifyHitbox(Vector extent);
};
