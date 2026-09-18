#include "JsonStore.h"
#include "JsonCodec.h"

#include <algorithm>
#include <cctype>
#include <fstream>

JsonStore::JsonStore(std::filesystem::path root) : root_(std::move(root)) {}

void JsonStore::setRoot(std::filesystem::path root) {
    root_ = std::move(root);
}

bool JsonStore::initialize(std::string& error) const {
    try {
        std::filesystem::create_directories(root_);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

std::filesystem::path JsonStore::save(const TasData& tas, std::string& error) const {
    try {
        if (tas.name.empty()) {
            throw std::runtime_error("TAS name cannot be empty");
        }
        std::filesystem::create_directories(root_);
        const auto target = root_ / (safeName(tas.name) + ".json");
        auto temporary = target;
        temporary += ".tmp";

        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output << JsonCodec::encodeTas(tas).dump(2);
        output.close();
        if (!output) {
            throw std::runtime_error("Could not write TAS file");
        }

        std::error_code code;
        std::filesystem::remove(target, code);
        code.clear();
        std::filesystem::rename(temporary, target, code);
        if (code) {
            throw std::runtime_error(code.message());
        }
        return target;
    } catch (const std::exception& exception) {
        error = exception.what();
        return {};
    }
}

std::optional<TasData> JsonStore::load(
    const std::filesystem::path& path,
    std::string& error
) const {
    try {
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            throw std::runtime_error("Could not open TAS file");
        }
        nlohmann::json value;
        input >> value;
        return JsonCodec::decodeTas(value);
    } catch (const std::exception& exception) {
        error = exception.what();
        return std::nullopt;
    }
}

bool JsonStore::erase(const std::filesystem::path& path, std::string& error) const {
    try {
        if (path.extension() != ".json" || path.filename() == "settings.json") {
            throw std::runtime_error("Invalid TAS file");
        }
        if (!std::filesystem::remove(path)) {
            throw std::runtime_error("TAS file does not exist");
        }
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

std::vector<std::filesystem::path> JsonStore::list(std::string& error) const {
    std::vector<std::filesystem::path> result;
    try {
        std::filesystem::create_directories(root_);
        for (const auto& entry : std::filesystem::directory_iterator(root_)) {
            const auto& path = entry.path();
            if (entry.is_regular_file() && path.extension() == ".json" &&
                path.filename() != "settings.json") {
                result.push_back(path);
            }
        }
        std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
            return left.filename().wstring() < right.filename().wstring();
        });
    } catch (const std::exception& exception) {
        error = exception.what();
    }
    return result;
}

bool JsonStore::saveSettings(const PluginSettings& settings, std::string& error) const {
    try {
        std::filesystem::create_directories(root_);
        std::ofstream output(settingsPath(), std::ios::binary | std::ios::trunc);
        output << JsonCodec::encodeSettings(settings).dump(2);
        output.close();
        if (!output) {
            throw std::runtime_error("Could not write settings");
        }
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

std::optional<PluginSettings> JsonStore::loadSettings(std::string& error) const {
    try {
        if (!std::filesystem::exists(settingsPath())) {
            return std::nullopt;
        }
        std::ifstream input(settingsPath(), std::ios::binary);
        nlohmann::json value;
        input >> value;
        return JsonCodec::decodeSettings(value);
    } catch (const std::exception& exception) {
        error = exception.what();
        return std::nullopt;
    }
}

const std::filesystem::path& JsonStore::root() const {
    return root_;
}

std::string JsonStore::safeName(const std::string& name) {
    std::string result;
    result.reserve(name.size());
    for (const unsigned char character : name) {
        const bool accepted = std::isalnum(character) || character == ' ' ||
            character == '-' || character == '_' || character == '(' || character == ')';
        result.push_back(accepted ? static_cast<char>(character) : '_');
        if (result.size() == 80) {
            break;
        }
    }
    while (!result.empty() && (result.back() == ' ' || result.back() == '.')) {
        result.pop_back();
    }
    return result.empty() ? "tas" : result;
}

std::filesystem::path JsonStore::settingsPath() const {
    return root_ / "settings.json";
}
