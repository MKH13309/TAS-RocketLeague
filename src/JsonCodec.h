#pragma once

#include "Model.h"
#include "nlohmann/json.hpp"

namespace JsonCodec {
    nlohmann::json encodeTas(const TasData& tas);
    TasData decodeTas(const nlohmann::json& json);
    nlohmann::json encodeSettings(const PluginSettings& settings);
    PluginSettings decodeSettings(const nlohmann::json& json);
}
