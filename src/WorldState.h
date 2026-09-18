#pragma once

#include "Model.h"
#include "bakkesmod/wrappers/GameWrapper.h"

#include <string>

class WorldState {
public:
    static bool capture(GameWrapper& game, TasData& tas, std::string& error);
    static bool restore(GameWrapper& game, const TasData& tas, std::string& error);
    static bool restoreFrame(GameWrapper& game, const TasFrame& frame, std::string& error);
    static bool validate(GameWrapper& game, const TasData& tas, std::string& error);
    static bool captureFrame(
        GameWrapper& game,
        class CarWrapper car,
        const ControllerInput& input,
        TasFrame& frame
    );
    static bool applyFrame(
        GameWrapper& game,
        class CarWrapper car,
        const TasFrame& frame,
        ControllerInput& input
    );

private:
    static RigidState captureRigid(class RBActorWrapper actor);
    static void applyRigid(class RBActorWrapper actor, const RigidState& state);
    static CarState captureCar(class CarWrapper car);
    static void applyCar(class CarWrapper car, const CarState& state);
    static Compatibility currentCompatibility(GameWrapper& game, class CarWrapper car);
    static std::string classifyHitbox(Vector extent);
};
