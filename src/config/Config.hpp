#pragma once

#include "Current.hpp"
#include <filesystem>
#include <string>
#include <vector>

using json = nlohmann::json;

class Config {
public:
    ~Config() = default;
    Config(const Config&) = delete;
    Config(Config&&) = delete;
    Config& operator=(const Config&) = delete;
    Config& operator=(Config&&) = delete;

    static bool Read();
    static bool Write();

    static void EnsureDir();
    static std::filesystem::path Dir();
    static std::filesystem::path PathFor(const std::string& name);
    static std::vector<std::string> List();
    static const std::string& CurrentName();

    static bool Create(const std::string& name);
    static bool Save(const std::string& name);
    static bool Load(const std::string& name);
    static bool Remove(const std::string& name);

private:
    Config() {};

    static Config& GetInstance()
    {
        static Config i{};
        return i;
    }

    bool ReadImpl();
    bool WriteImpl();
    bool ReadFrom(const std::filesystem::path& path);
    bool WriteTo(const std::filesystem::path& path);

    static color_t JsonToColor(const json& parent, const std::string& key, const color_t& def);
    static void ColorToJson(json& parent, const std::string& key, const color_t& color);
    static void Vec2ToJson(json& parent, const std::string& key, const Vec2_t& vec);
    static Vec2_t JsonToVec2(const json& parent, const std::string& key, const Vec2_t& def);

    std::string current_name = "default";
};
