#include "JsonCodec.h"

#include <algorithm>
#include <stdexcept>

using nlohmann::json;

namespace {
json encodeVector(const VectorState& value) {
    return {{"x", value.x}, {"y", value.y}, {"z", value.z}};
}

VectorState decodeVector(const json& value) {
    return {
        value.at("x").get<float>(), value.at("y").get<float>(), value.at("z").get<float>()
    };
}

json encodeQuaternion(const QuaternionState& value) {
    return {{"x", value.x}, {"y", value.y}, {"z", value.z}, {"w", value.w}};
}

QuaternionState decodeQuaternion(const json& value) {
    return {
        value.at("x").get<float>(), value.at("y").get<float>(),
        value.at("z").get<float>(), value.at("w").get<float>()
    };
}

json encodeRigid(const RigidState& value) {
    return {
        {"rotation", encodeQuaternion(value.rotation)},
        {"location", encodeVector(value.location)},
        {"velocity", encodeVector(value.velocity)},
        {"angular_velocity", encodeVector(value.angularVelocity)},
        {"sleeping", value.sleeping}
    };
}

RigidState decodeRigid(const json& value) {
    RigidState result;
    result.rotation = decodeQuaternion(value.at("rotation"));
    result.location = decodeVector(value.at("location"));
    result.velocity = decodeVector(value.at("velocity"));
    result.angularVelocity = decodeVector(value.at("angular_velocity"));
    result.sleeping = value.value("sleeping", false);
    return result;
}

json encodeCar(const CarState& value) {
    return {
        {"rigid_body", encodeRigid(value.rigidBody)}, {"boost", value.boost},
        {"jumped", value.jumped}, {"double_jumped", value.doubleJumped},
        {"on_ground", value.onGround}, {"can_jump", value.canJump}
    };
}

CarState decodeCar(const json& value) {
    CarState result;
    result.rigidBody = decodeRigid(value.at("rigid_body"));
    result.boost = value.at("boost").get<float>();
    result.jumped = value.value("jumped", false);
    result.doubleJumped = value.value("double_jumped", false);
    result.onGround = value.value("on_ground", false);
    result.canJump = value.value("can_jump", false);
    return result;
}

json encodeInput(const InputFrame& value) {
    return {
        {"throttle", value.throttle}, {"steer", value.steer},
        {"pitch", value.pitch}, {"yaw", value.yaw}, {"roll", value.roll},
        {"dodge_forward", value.dodgeForward}, {"dodge_strafe", value.dodgeStrafe},
        {"handbrake", value.handbrake}, {"jump", value.jump},
        {"activate_boost", value.activateBoost}, {"holding_boost", value.holdingBoost},
        {"jumped", value.jumped}
    };
}

InputFrame decodeInput(const json& value) {
    InputFrame result;
    result.throttle = value.at("throttle").get<float>();
    result.steer = value.at("steer").get<float>();
    result.pitch = value.at("pitch").get<float>();
    result.yaw = value.at("yaw").get<float>();
    result.roll = value.at("roll").get<float>();
    result.dodgeForward = value.value("dodge_forward", 0.0f);
    result.dodgeStrafe = value.value("dodge_strafe", 0.0f);
    result.handbrake = value.at("handbrake").get<bool>();
    result.jump = value.at("jump").get<bool>();
    result.activateBoost = value.at("activate_boost").get<bool>();
    result.holdingBoost = value.at("holding_boost").get<bool>();
    result.jumped = value.value("jumped", false);
    return result;
}

json encodePlayer(const PlayerFrame& player) {
    return {{"input", encodeInput(player.input)}, {"car", encodeCar(player.car)}};
}

PlayerFrame decodePlayer(const json& value) {
    return {decodeInput(value.at("input")), decodeCar(value.at("car"))};
}

json encodeFrame(const TasFrame& frame, int playerCount) {
    json players = json::array();
    for (int index = 0; index < playerCount; ++index) {
        players.push_back(encodePlayer(frame.players[static_cast<std::size_t>(index)]));
    }
    return {
        {"players", std::move(players)},
        {"ball", encodeRigid(frame.ball)},
        {"ball_touch_players", frame.ballTouchPlayers}
    };
}

TasFrame decodeFrameV3(const json& value, int playerCount) {
    const auto& players = value.at("players");
    if (!players.is_array() || players.size() < static_cast<std::size_t>(playerCount)) {
        throw std::runtime_error("Invalid player frame list");
    }
    TasFrame frame;
    for (int index = 0; index < playerCount; ++index) {
        frame.players[static_cast<std::size_t>(index)] = decodePlayer(players.at(index));
    }
    frame.ball = decodeRigid(value.at("ball"));
    frame.ballTouchPlayers = value.value("ball_touch_players", 0U);
    frame.ballTouchPlayers &= (1U << playerCount) - 1U;
    return frame;
}

TasFrame decodeFrameV2(const json& value) {
    TasFrame frame;
    frame.players[0] = {decodeInput(value.at("input")), decodeCar(value.at("car"))};
    frame.ball = decodeRigid(value.at("ball"));
    return frame;
}

void inferLegacyBallTouches(TasData& tas) {
    constexpr float touchDistanceSquared = 230.0f * 230.0f;
    for (auto& frame : tas.frames) {
        for (int player = 0; player < tas.playerCount; ++player) {
            const auto playerIndex = static_cast<std::size_t>(player);
            if ((tas.recordedPlayers & (1U << playerIndex)) == 0) {
                continue;
            }
            const auto& car = frame.players[playerIndex].car.rigidBody.location;
            const auto& ball = frame.ball.location;
            const auto dx = car.x - ball.x;
            const auto dy = car.y - ball.y;
            const auto dz = car.z - ball.z;
            if (dx * dx + dy * dy + dz * dz <= touchDistanceSquared) {
                frame.markBallTouch(playerIndex);
            }
        }
    }
}

void decodeCompatibility(const json& value, Compatibility& result) {
    result.mode = value.value("mode", "freeplay");
    result.map = value.value("map", "");
    result.matchType = value.value("match_type", "");
    result.trainingShot = value.value("training_shot", -1);
    result.hitbox = value.at("hitbox").get<std::string>();
    result.hitboxExtent = decodeVector(value.at("hitbox_extent"));
    result.steerSensitivity = value.at("steer_sensitivity").get<float>();
    result.airSensitivity = value.at("air_sensitivity").get<float>();
}
}

json JsonCodec::encodeTas(const TasData& tas) {
    const auto playerCount = std::clamp(tas.playerCount, 1, static_cast<int>(maxTasPlayers));
    json frames = json::array();
    for (const auto& frame : tas.frames) {
        frames.push_back(encodeFrame(frame, playerCount));
    }
    json startPlayers = json::array();
    for (int index = 0; index < playerCount; ++index) {
        startPlayers.push_back(encodeCar(tas.startCars[static_cast<std::size_t>(index)]));
    }

    return {
        {"schema_version", 4},
        {"name", tas.name},
        {"player_count", playerCount},
        {"recorded_players", tas.recordedPlayers},
        {"expected", {
            {"mode", tas.expected.mode}, {"map", tas.expected.map},
            {"match_type", tas.expected.matchType},
            {"training_shot", tas.expected.trainingShot},
            {"hitbox", tas.expected.hitbox},
            {"hitbox_extent", encodeVector(tas.expected.hitboxExtent)},
            {"steer_sensitivity", tas.expected.steerSensitivity},
            {"air_sensitivity", tas.expected.airSensitivity}
        }},
        {"speeds", {{"replay", tas.replaySpeed}, {"record", tas.recordSpeed}}},
        {"start", {{"players", std::move(startPlayers)}, {"ball", encodeRigid(tas.ball)}}},
        {"frames", std::move(frames)}
    };
}

TasData JsonCodec::decodeTas(const json& value) {
    const auto schema = value.at("schema_version").get<int>();
    if (schema == 1) {
        throw std::runtime_error("Older TAS files must be recreated");
    }
    if (schema != 2 && schema != 3 && schema != 4) {
        throw std::runtime_error("Unsupported TAS schema version");
    }

    TasData tas;
    tas.schemaVersion = 4;
    tas.name = value.at("name").get<std::string>();
    decodeCompatibility(value.at("expected"), tas.expected);
    const auto& speeds = value.at("speeds");
    tas.replaySpeed = speeds.at("replay").get<float>();
    tas.recordSpeed = speeds.at("record").get<float>();

    const auto& frames = value.at("frames");
    if (!frames.is_array() || frames.size() > 10'000'000) {
        throw std::runtime_error("Invalid frame list");
    }

    if (schema == 2) {
        tas.playerCount = 1;
        tas.startCars[0] = decodeCar(value.at("start").at("car"));
        tas.ball = decodeRigid(value.at("start").at("ball"));
        tas.frames.reserve(frames.size());
        for (const auto& frame : frames) {
            tas.frames.push_back(decodeFrameV2(frame));
        }
        tas.recordedPlayers = tas.frames.empty() ? 0U : 1U;
        inferLegacyBallTouches(tas);
        return tas;
    }

    tas.playerCount = value.value("player_count", 1);
    if (tas.playerCount < 1 || tas.playerCount > static_cast<int>(maxTasPlayers)) {
        throw std::runtime_error("Unsupported TAS player count");
    }
    const auto& startPlayers = value.at("start").at("players");
    if (!startPlayers.is_array() ||
        startPlayers.size() < static_cast<std::size_t>(tas.playerCount)) {
        throw std::runtime_error("Invalid starting player list");
    }
    for (int index = 0; index < tas.playerCount; ++index) {
        tas.startCars[static_cast<std::size_t>(index)] = decodeCar(startPlayers.at(index));
    }
    tas.ball = decodeRigid(value.at("start").at("ball"));
    tas.frames.reserve(frames.size());
    for (const auto& frame : frames) {
        tas.frames.push_back(decodeFrameV3(frame, tas.playerCount));
    }
    tas.recordedPlayers = value.value(
        "recorded_players",
        tas.frames.empty() ? 0U : 1U
    );
    tas.recordedPlayers &= (1U << tas.playerCount) - 1U;
    if (schema < 4) {
        inferLegacyBallTouches(tas);
    }
    return tas;
}

json JsonCodec::encodeSettings(const PluginSettings& settings) {
    const auto& trigger = settings.triggers;
    const auto& popup = settings.popups;
    return {
        {"triggers", {
            {"throttle", trigger.throttle}, {"steer", trigger.steer},
            {"pitch", trigger.pitch}, {"yaw", trigger.yaw}, {"roll", trigger.roll},
            {"jump", trigger.jump}, {"boost", trigger.boost},
            {"handbrake", trigger.handbrake}, {"analog_threshold", trigger.analogThreshold}
        }},
        {"popups", {
            {"new_tas", popup.confirmNew}, {"load", popup.confirmLoad},
            {"update", popup.confirmUpdate}, {"delete", popup.confirmDelete}
        }}
    };
}

PluginSettings JsonCodec::decodeSettings(const json& value) {
    PluginSettings settings;
    const auto& trigger = value.at("triggers");
    settings.triggers.throttle = trigger.value("throttle", false);
    settings.triggers.steer = trigger.value("steer", false);
    settings.triggers.pitch = trigger.value("pitch", false);
    settings.triggers.yaw = trigger.value("yaw", false);
    settings.triggers.roll = trigger.value("roll", false);
    settings.triggers.jump = trigger.value("jump", true);
    settings.triggers.boost = trigger.value("boost", true);
    settings.triggers.handbrake = trigger.value("handbrake", true);
    settings.triggers.analogThreshold = trigger.value("analog_threshold", 0.15f);

    const auto& popup = value.at("popups");
    settings.popups.confirmNew = popup.value("new_tas", true);
    settings.popups.confirmLoad = popup.value("load", true);
    settings.popups.confirmUpdate = popup.value("update", true);
    settings.popups.confirmDelete = popup.value("delete", true);
    return settings;
}
