#include "JsonCodec.h"

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

json encodeFrame(const TasFrame& frame) {
    return {
        {"input", encodeInput(frame.input)},
        {"car", encodeCar(frame.car)},
        {"ball", encodeRigid(frame.ball)}
    };
}

TasFrame decodeFrame(const json& value) {
    return {
        decodeInput(value.at("input")),
        decodeCar(value.at("car")),
        decodeRigid(value.at("ball"))
    };
}
}

json JsonCodec::encodeTas(const TasData& tas) {
    json frames = json::array();
    for (const auto& frame : tas.frames) {
        frames.push_back(encodeFrame(frame));
    }

    return {
        {"schema_version", tas.schemaVersion},
        {"name", tas.name},
        {"expected", {
            {"map", tas.expected.map}, {"hitbox", tas.expected.hitbox},
            {"hitbox_extent", encodeVector(tas.expected.hitboxExtent)},
            {"steer_sensitivity", tas.expected.steerSensitivity},
            {"air_sensitivity", tas.expected.airSensitivity}
        }},
        {"speeds", {{"replay", tas.replaySpeed}, {"record", tas.recordSpeed}}},
        {"start", {{"car", encodeCar(tas.car)}, {"ball", encodeRigid(tas.ball)}}},
        {"frames", std::move(frames)}
    };
}

TasData JsonCodec::decodeTas(const json& value) {
    TasData tas;
    tas.schemaVersion = value.at("schema_version").get<int>();
    if (tas.schemaVersion == 1) {
        throw std::runtime_error("Older TAS files must be recreated");
    }
    if (tas.schemaVersion != 2) {
        throw std::runtime_error("Unsupported TAS schema version");
    }
    tas.name = value.at("name").get<std::string>();

    const auto& expected = value.at("expected");
    tas.expected.map = expected.at("map").get<std::string>();
    tas.expected.hitbox = expected.at("hitbox").get<std::string>();
    tas.expected.hitboxExtent = decodeVector(expected.at("hitbox_extent"));
    tas.expected.steerSensitivity = expected.at("steer_sensitivity").get<float>();
    tas.expected.airSensitivity = expected.at("air_sensitivity").get<float>();

    const auto& speeds = value.at("speeds");
    tas.replaySpeed = speeds.at("replay").get<float>();
    tas.recordSpeed = speeds.at("record").get<float>();
    tas.car = decodeCar(value.at("start").at("car"));
    tas.ball = decodeRigid(value.at("start").at("ball"));

    const auto& frames = value.at("frames");
    if (!frames.is_array() || frames.size() > 10'000'000) {
        throw std::runtime_error("Invalid frame list");
    }
    tas.frames.reserve(frames.size());
    for (const auto& frame : frames) {
        tas.frames.push_back(decodeFrame(frame));
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

