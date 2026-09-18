#pragma once

#include "Model.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

class JsonStore {
public:
    JsonStore() = default;
    explicit JsonStore(std::filesystem::path root);

    void setRoot(std::filesystem::path root);
    bool initialize(std::string& error) const;
    std::filesystem::path save(const TasData& tas, std::string& error) const;
    std::optional<TasData> load(const std::filesystem::path& path, std::string& error) const;
    bool erase(const std::filesystem::path& path, std::string& error) const;
    std::vector<std::filesystem::path> list(std::string& error) const;
    bool saveSettings(const PluginSettings& settings, std::string& error) const;
    std::optional<PluginSettings> loadSettings(std::string& error) const;
    const std::filesystem::path& root() const;

private:
    std::filesystem::path root_;

    static std::string safeName(const std::string& name);
    std::filesystem::path settingsPath() const;
};
