#include "WorldState.h"
#include "GameContext.h"
#include "GameMode.h"

#include "bakkesmod/wrappers/GameEvent/ServerWrapper.h"
#include "bakkesmod/wrappers/GameObject/BallWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarComponent/BoostWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarWrapper.h"
#include "bakkesmod/wrappers/GameObject/RBActorWrapper.h"
#include "bakkesmod/wrappers/SettingsWrapper.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {
VectorState toState(const Vector& value) {
    return {value.X, value.Y, value.Z};
}

Vector toVector(const VectorState& value) {
    return {value.x, value.y, value.z};
}

float hitboxError(Vector extent, Vector dimensions, float scale) {
    const auto dx = (extent.X * scale - dimensions.X) / dimensions.X;
    const auto dy = (extent.Y * scale - dimensions.Y) / dimensions.Y;
    const auto dz = (extent.Z * scale - dimensions.Z) / dimensions.Z;
    return dx * dx + dy * dy + dz * dz;
}
}

bool WorldState::capture(GameWrapper& game, TasData& tas, std::string& error) {
    if (!GameMode::requireSupported(game, error)) {
        return false;
    }
    auto car = GameContext::localCar(game);
    auto server = GameContext::server(game);
    if (!car || !server) {
        error = "The local car is not available";
        return false;
    }
    auto ball = server.GetBall();
    if (!ball) {
        error = "The ball is not available";
        return false;
    }

    tas.expected = currentCompatibility(game, car);
    tas.car = captureCar(car);
    tas.ball = captureRigid(ball);
    return true;
}

bool WorldState::restore(GameWrapper& game, const TasData& tas, std::string& error) {
    if (!GameMode::requireSupported(game, error)) {
        return false;
    }
    auto car = GameContext::localCar(game);
    auto server = GameContext::server(game);
    if (!car || !server) {
        error = "The local car is not available";
        return false;
    }
    auto ball = server.GetBall();
    if (!ball) {
        error = "The ball is not available";
        return false;
    }

    applyCar(car, tas.car);
    ControllerInput neutral{};
    car.SetInput(neutral);
    applyRigid(ball, tas.ball);
    return true;
}

bool WorldState::restoreFrame(
    GameWrapper& game,
    const TasFrame& frame,
    std::string& error
) {
    if (!GameMode::requireSupported(game, error)) {
        return false;
    }
    auto car = GameContext::localCar(game);
    auto server = GameContext::server(game);
    if (!car || !server) {
        error = "The local car is not available";
        return false;
    }
    auto ball = server.GetBall();
    if (!ball) {
        error = "The ball is not available";
        return false;
    }

    applyCar(car, frame.car);
    ControllerInput neutral{};
    car.SetInput(neutral);
    applyRigid(ball, frame.ball);
    return true;
}

bool WorldState::validate(GameWrapper& game, const TasData& tas, std::string& error) {
    if (!GameMode::requireSupported(game, error)) {
        return false;
    }
    auto car = GameContext::localCar(game);
    if (!car) {
        error = "The local car is not available";
        return false;
    }

    const auto current = currentCompatibility(game, car);
    if (current.hitbox != tas.expected.hitbox) {
        error = "Hitbox mismatch: expected " + tas.expected.hitbox;
        return false;
    }
    if (std::abs(current.steerSensitivity - tas.expected.steerSensitivity) > 0.001f) {
        error = "Steering sensitivity mismatch";
        return false;
    }
    if (std::abs(current.airSensitivity - tas.expected.airSensitivity) > 0.001f) {
        error = "Air sensitivity mismatch";
        return false;
    }
    return true;
}

bool WorldState::captureFrame(
    GameWrapper& game,
    CarWrapper car,
    const ControllerInput& input,
    TasFrame& frame
) {
    auto server = GameContext::server(game);
    if (!car || !server) {
        return false;
    }
    auto ball = server.GetBall();
    if (!ball) {
        return false;
    }
    frame.input = InputFrame::capture(input);
    frame.car = captureCar(car);
    frame.ball = captureRigid(ball);
    return true;
}

bool WorldState::applyFrame(
    GameWrapper& game,
    CarWrapper car,
    const TasFrame& frame,
    ControllerInput& input
) {
    auto server = GameContext::server(game);
    if (!car || !server) {
        return false;
    }
    auto ball = server.GetBall();
    if (!ball) {
        return false;
    }
    applyCar(car, frame.car);
    applyRigid(ball, frame.ball);
    frame.input.apply(input);
    return true;
}

RigidState WorldState::captureRigid(RBActorWrapper actor) {
    const auto body = actor.GetRBState();
    return {
        {body.Quaternion.X, body.Quaternion.Y, body.Quaternion.Z, body.Quaternion.W},
        toState(body.Location),
        toState(body.LinearVelocity),
        toState(body.AngularVelocity),
        body.bSleeping != 0
    };
}

void WorldState::applyRigid(RBActorWrapper actor, const RigidState& value) {
    auto body = actor.GetRBState();
    body.Quaternion = {
        value.rotation.w, value.rotation.x, value.rotation.y, value.rotation.z
    };
    body.Location = toVector(value.location);
    body.LinearVelocity = toVector(value.velocity);
    body.AngularVelocity = toVector(value.angularVelocity);
    body.bSleeping = value.sleeping;
    body.bNewData = true;
    actor.SetRBState(body);
    actor.SetPhysicsState(body);
    actor.SetLocation(body.Location);
    actor.SetVelocity(body.LinearVelocity);
    actor.SetAngularVelocity(body.AngularVelocity, false);
}

CarState WorldState::captureCar(CarWrapper car) {
    CarState state;
    state.rigidBody = captureRigid(car);
    auto boost = car.GetBoostComponent();
    state.boost = boost ? boost.GetCurrentBoostAmount() : 0.0f;
    state.jumped = car.GetbJumped() != 0;
    state.doubleJumped = car.GetbDoubleJumped() != 0;
    state.onGround = car.GetbOnGround() != 0;
    state.canJump = car.GetbCanJump() != 0;
    return state;
}

void WorldState::applyCar(CarWrapper car, const CarState& state) {
    applyRigid(car, state.rigidBody);
    car.SetbJumped(state.jumped);
    car.SetbDoubleJumped(state.doubleJumped);
    car.SetbOnGround(state.onGround);
    car.SetbCanJump(state.canJump);
    auto boost = car.GetBoostComponent();
    if (boost) {
        boost.SetCurrentBoostAmount(state.boost);
    }
}

Compatibility WorldState::currentCompatibility(GameWrapper& game, CarWrapper car) {
    const auto extent = car.GetLocalCollisionExtent();
    const auto gamepad = game.GetSettings().GetGamepadSettings();
    const auto mode = GameMode::current(game);
    Compatibility result;
    result.mode = mode.id;
    result.map = game.GetCurrentMap();
    result.matchType = mode.matchType;
    result.trainingShot = mode.trainingShot;
    result.hitbox = classifyHitbox(extent);
    result.hitboxExtent = toState(extent);
    result.steerSensitivity = gamepad.SteeringSensitivity;
    result.airSensitivity = gamepad.AirControlSensitivity;
    return result;
}

std::string WorldState::classifyHitbox(Vector extent) {
    struct Hitbox {
        const char* name;
        Vector dimensions;
    };
    const std::array<Hitbox, 6> hitboxes{{
        {"Octane", {118.01f, 84.20f, 36.16f}},
        {"Dominus", {127.93f, 83.28f, 31.30f}},
        {"Plank", {128.82f, 84.67f, 29.39f}},
        {"Breakout", {131.49f, 80.52f, 30.30f}},
        {"Hybrid", {127.02f, 82.19f, 34.16f}},
        {"Merc", {120.72f, 76.71f, 41.66f}}
    }};

    const Hitbox* best = nullptr;
    float bestError = 1000.0f;
    for (const auto& hitbox : hitboxes) {
        const auto error = std::min(
            hitboxError(extent, hitbox.dimensions, 1.0f),
            hitboxError(extent, hitbox.dimensions, 2.0f)
        );
        if (error < bestError) {
            best = &hitbox;
            bestError = error;
        }
    }
    return best && bestError < 0.08f ? best->name : "Unknown";
}
