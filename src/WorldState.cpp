#include "WorldState.h"
#include "GameContext.h"
#include "GameMode.h"

#include "bakkesmod/wrappers/ArrayWrapper.h"
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

std::size_t selectedPlayer(const TasData& tas, std::size_t requested) {
    return tas.playerCount > 1 ? std::min<std::size_t>(requested, 1) : 0;
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
    auto local = GameContext::localCar(game);
    auto server = GameContext::server(game);
    auto currentBall = GameContext::ball(game, server);
    if (!local || !server) {
        error = "The local car or game state is not available";
        return false;
    }
    if (!currentBall) {
        error = "The ball is not available";
        return false;
    }

    tas.expected = currentCompatibility(game, local);
    tas.startCars[0] = captureCar(local);
    tas.ball = captureRigid(currentBall);
    tas.recordedPlayers = 0;
    if (tas.playerCount > 1) {
        auto opponent = otherCar(game, local);
        if (!opponent) {
            error = "Start an offline exhibition match with an opponent first";
            return false;
        }
        tas.startCars[1] = captureCar(opponent);
    }
    return true;
}

bool WorldState::restore(
    GameWrapper& game,
    const TasData& tas,
    std::size_t activePlayer,
    std::string& error
) {
    if (!GameMode::requireSupported(game, error)) {
        return false;
    }
    auto local = GameContext::localCar(game);
    auto currentBall = GameContext::ball(game);
    if (!local || !currentBall) {
        error = "The local car or ball is not available";
        return false;
    }

    const auto active = selectedPlayer(tas, activePlayer);
    applyCar(local, tas.startCars[active]);
    setCollision(local, true);
    ControllerInput neutral{};
    local.SetInput(neutral);
    if (tas.playerCount > 1) {
        auto opponent = otherCar(game, local);
        if (!opponent) {
            error = "The second car is not available";
            return false;
        }
        applyCar(opponent, tas.startCars[1 - active]);
        setCollision(opponent, true);
        opponent.SetInput(neutral);
    }
    applyBall(currentBall, tas.ball, true);
    return true;
}

bool WorldState::restoreFrame(
    GameWrapper& game,
    const TasData& tas,
    const TasFrame& frame,
    std::size_t activePlayer,
    std::string& error
) {
    if (!GameMode::requireSupported(game, error)) {
        return false;
    }
    auto local = GameContext::localCar(game);
    auto currentBall = GameContext::ball(game);
    if (!local || !currentBall) {
        error = "The local car or ball is not available";
        return false;
    }

    const auto active = selectedPlayer(tas, activePlayer);
    applyCar(local, frame.players[active].car);
    setCollision(local, true);
    ControllerInput neutral{};
    local.SetInput(neutral);
    if (tas.playerCount > 1) {
        auto opponent = otherCar(game, local);
        if (!opponent) {
            error = "The second car is not available";
            return false;
        }
        applyCar(opponent, frame.players[1 - active].car);
        setCollision(opponent, true);
        opponent.SetInput(neutral);
    }
    applyBall(currentBall, frame.ball, true);
    return true;
}

bool WorldState::validate(GameWrapper& game, const TasData& tas, std::string& error) {
    if (!GameMode::requireSupported(game, error)) {
        return false;
    }
    auto local = GameContext::localCar(game);
    if (!local) {
        error = "The local car is not available";
        return false;
    }
    if (tas.playerCount > 1 && !otherCar(game, local)) {
        error = "This two-player TAS needs a second car";
        return false;
    }

    const auto current = currentCompatibility(game, local);
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

void WorldState::setSessionProtection(GameWrapper& game, bool active) {
    auto server = GameContext::server(game);
    if (server && GameMode::current(game).id == "exhibition") {
        if (active) {
            server.DisableGoalReset();
            server.SetbRoundActive(true);
        } else {
            server.EnableGoalReset();
        }
    }

    auto local = GameContext::localCar(game);
    if (local) {
        setCollision(local, true);
        auto opponent = otherCar(game, local);
        if (opponent) {
            setCollision(opponent, true);
        }
    }
    auto currentBall = GameContext::ball(game, server);
    if (currentBall) {
        setBallActive(currentBall, true);
    }
}

void WorldState::restoreReferenceBall(
    BallWrapper ball,
    const RigidState& state
) {
    if (ball) {
        applyBall(ball, state, true);
    }
}

bool WorldState::captureFrame(
    GameWrapper& game,
    CarWrapper local,
    const ControllerInput& input,
    const TasData& tas,
    const TasFrame* reference,
    bool continuePastReference,
    std::size_t activePlayer,
    BallTrackLock& ballLock,
    TasFrame& frame
) {
    auto currentBall = GameContext::ball(game);
    if (!local || !currentBall) {
        return false;
    }

    keepSessionAlive(game);
    setCollision(local, true);
    if (!reference) {
        ballLock.reset(false);
    }

    const auto active = selectedPlayer(tas, activePlayer);
    if (reference && !ballLock.released && reference->ballTouchedBy(active)) {
        ballLock.release();
    }
    const bool referenceBallLocked = reference && !ballLock.released;
    frame = reference ? *reference : TasFrame{};
    frame.clearBallTouch(active);
    frame.players[active].input = InputFrame::capture(input);
    frame.players[active].car = captureCar(local);

    if (tas.playerCount > 1) {
        auto opponent = otherCar(game, local);
        if (!opponent) {
            return false;
        }
        const auto inactive = 1 - active;
        if (reference) {
            applyCar(opponent, reference->players[inactive].car);
        } else if (!continuePastReference) {
            applyCar(opponent, tas.startCars[inactive]);
        }
        setCollision(opponent, !referenceBallLocked);
        ControllerInput dummyInput{};
        if (reference) {
            reference->players[inactive].input.apply(dummyInput);
        }
        opponent.SetInput(dummyInput);
        frame.players[inactive].car = captureCar(opponent);
        if (!reference) {
            frame.players[inactive].input = {};
        }
    }

    if (referenceBallLocked) {
        applyBall(currentBall, reference->ball, true);
        frame.ball = reference->ball;
    } else {
        setBallActive(currentBall, true);
        frame.ball = captureRigid(currentBall);
    }
    return true;
}

bool WorldState::applyFrame(
    GameWrapper& game,
    CarWrapper local,
    const TasData& tas,
    const TasFrame& frame,
    std::size_t activePlayer,
    ControllerInput& input
) {
    auto currentBall = GameContext::ball(game);
    if (!local || !currentBall) {
        return false;
    }

    keepSessionAlive(game);
    const auto active = selectedPlayer(tas, activePlayer);
    applyCar(local, frame.players[active].car);
    setCollision(local, false);
    frame.players[active].input.apply(input);
    if (tas.playerCount > 1) {
        auto opponent = otherCar(game, local);
        if (!opponent) {
            return false;
        }
        const auto inactive = 1 - active;
        applyCar(opponent, frame.players[inactive].car);
        setCollision(opponent, false);
        ControllerInput dummyInput{};
        frame.players[inactive].input.apply(dummyInput);
        opponent.SetInput(dummyInput);
    }
    applyBall(currentBall, frame.ball, false);
    return true;
}

CarWrapper WorldState::otherCar(GameWrapper& game, CarWrapper local) {
    auto server = GameContext::server(game);
    if (!server || !local) {
        return CarWrapper(0);
    }
    auto cars = server.GetCars();
    for (int index = 0; index < cars.Count(); ++index) {
        auto candidate = cars.Get(index);
        if (candidate && candidate.memory_address != local.memory_address) {
            return candidate;
        }
    }
    return CarWrapper(0);
}

void WorldState::keepSessionAlive(GameWrapper& game) {
    auto server = GameContext::server(game);
    if (server && GameMode::current(game).id == "exhibition") {
        server.DisableGoalReset();
        server.SetbRoundActive(true);
    }
}

void WorldState::setCollision(RBActorWrapper actor, bool enabled) {
    if (!actor) {
        return;
    }
    actor.SetbCollideActors(enabled);
    actor.SetbCollideWorld(enabled);
}

void WorldState::setBallActive(BallWrapper ball, bool collisionsEnabled) {
    ball.SetbEndOfGameHidden(false);
    ball.SetbFadeIn(false);
    ball.SetbFadeOut(false);
    ball.SetbItemFreeze(false);
    ball.SetbHiddenSelf(false);
    ball.SetHidden2(false);
    setCollision(ball, collisionsEnabled);
}

RigidState WorldState::captureRigid(RBActorWrapper actor) {
    const auto body = actor.GetRBState();
    return {
        {body.Quaternion.X, body.Quaternion.Y, body.Quaternion.Z, body.Quaternion.W},
        toState(body.Location), toState(body.LinearVelocity),
        toState(body.AngularVelocity), body.bSleeping != 0
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

void WorldState::applyBall(
    BallWrapper ball,
    const RigidState& state,
    bool collisionsEnabled
) {
    const auto location = toVector(state.location);
    setBallActive(ball, collisionsEnabled);
    ball.SetOldLocation(location);
    applyRigid(ball, state);
    ball.SetOldLocation(location);
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
