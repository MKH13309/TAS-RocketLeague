#pragma once

#include "bakkesmod/wrappers/WrapperStructs.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

constexpr std::size_t maxTasPlayers = 2;

struct VectorState {
    float x{};
    float y{};
    float z{};
};

struct QuaternionState {
    float x{};
    float y{};
    float z{};
    float w{1.0f};
};

struct RigidState {
    QuaternionState rotation;
    VectorState location;
    VectorState velocity;
    VectorState angularVelocity;
    bool sleeping{};
};

struct CarState {
    RigidState rigidBody;
    float boost{};
    bool jumped{};
    bool doubleJumped{};
    bool onGround{};
    bool canJump{};
};

struct Compatibility {
    std::string mode{"freeplay"};
    std::string map;
    std::string matchType;
    int trainingShot{-1};
    std::string hitbox;
    VectorState hitboxExtent;
    float steerSensitivity{1.0f};
    float airSensitivity{1.0f};
};

struct InputFrame {
    float throttle{};
    float steer{};
    float pitch{};
    float yaw{};
    float roll{};
    float dodgeForward{};
    float dodgeStrafe{};
    bool handbrake{};
    bool jump{};
    bool activateBoost{};
    bool holdingBoost{};
    bool jumped{};

    static InputFrame capture(const ControllerInput& input) {
        return {
            input.Throttle, input.Steer, input.Pitch, input.Yaw, input.Roll,
            input.DodgeForward, input.DodgeStrafe, input.Handbrake != 0,
            input.Jump != 0, input.ActivateBoost != 0, input.HoldingBoost != 0,
            input.Jumped != 0
        };
    }

    void apply(ControllerInput& input) const {
        input.Throttle = throttle;
        input.Steer = steer;
        input.Pitch = pitch;
        input.Yaw = yaw;
        input.Roll = roll;
        input.DodgeForward = dodgeForward;
        input.DodgeStrafe = dodgeStrafe;
        input.Handbrake = handbrake;
        input.Jump = jump;
        input.ActivateBoost = activateBoost;
        input.HoldingBoost = holdingBoost;
        input.Jumped = jumped;
    }
};

struct PlayerFrame {
    InputFrame input;
    CarState car;
};

struct TasFrame {
    std::array<PlayerFrame, maxTasPlayers> players;
    RigidState ball;
    unsigned int ballTouchPlayers{};

    bool ballTouchedBy(std::size_t player) const {
        return player < maxTasPlayers &&
            (ballTouchPlayers & (1U << player)) != 0;
    }

    void markBallTouch(std::size_t player) {
        if (player < maxTasPlayers) {
            ballTouchPlayers |= 1U << player;
        }
    }

    void clearBallTouch(std::size_t player) {
        if (player < maxTasPlayers) {
            ballTouchPlayers &= ~(1U << player);
        }
    }
};

struct TriggerSettings {
    bool throttle{};
    bool steer{};
    bool pitch{};
    bool yaw{};
    bool roll{};
    bool jump{true};
    bool boost{true};
    bool handbrake{true};
    float analogThreshold{0.15f};

    bool matches(const InputFrame& input) const {
        const auto active = [this](float value) {
            return std::abs(value) >= analogThreshold;
        };
        return (throttle && active(input.throttle)) ||
            (steer && active(input.steer)) ||
            (pitch && active(input.pitch)) ||
            (yaw && active(input.yaw)) ||
            (roll && active(input.roll)) ||
            (jump && (input.jump || input.jumped)) ||
            (boost && (input.activateBoost || input.holdingBoost)) ||
            (handbrake && input.handbrake);
    }
};

struct PopupSettings {
    bool confirmNew{true};
    bool confirmLoad{true};
    bool confirmUpdate{true};
    bool confirmDelete{true};
};

struct BallTrackLock {
    bool released{true};

    void reset(bool lockToReference) {
        released = !lockToReference;
    }

    void release() {
        released = true;
    }
};

struct PluginSettings {
    TriggerSettings triggers;
    PopupSettings popups;
};

struct TasData {
    int schemaVersion{4};
    std::string name;
    Compatibility expected;
    float replaySpeed{1.0f};
    float recordSpeed{0.25f};
    int playerCount{1};
    unsigned int recordedPlayers{};
    std::array<CarState, maxTasPlayers> startCars;
    RigidState ball;
    std::vector<TasFrame> frames;
};

enum class RunMode {
    idle,
    replaying,
    recording
};
