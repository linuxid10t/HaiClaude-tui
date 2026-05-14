#include "config.h"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using json = nlohmann::json;

static ProfileSettings profileFromJson(const json& j)
{
    ProfileSettings p;
    p.url = j.value("url", p.url);
    p.apiKey = j.value("apiKey", "");
    p.saveKey = j.value("saveKey", false);
    p.currentModelCheck = j.value("currentModelCheck", false);
    p.currentModel = j.value("currentModel", p.currentModel);
    p.opusModelCheck = j.value("opusModelCheck", false);
    p.opusModel = j.value("opusModel", p.opusModel);
    p.sonnetModelCheck = j.value("sonnetModelCheck", false);
    p.sonnetModel = j.value("sonnetModel", p.sonnetModel);
    p.haikuModelCheck = j.value("haikuModelCheck", false);
    p.haikuModel = j.value("haikuModel", p.haikuModel);
    p.fixAttribution = j.value("fixAttribution", false);
    return p;
}

static json profileToJson(const ProfileSettings& p)
{
    json j;
    j["url"] = p.url;
    j["saveKey"] = p.saveKey;
    if (p.saveKey)
        j["apiKey"] = p.apiKey;
    j["currentModelCheck"] = p.currentModelCheck;
    j["currentModel"] = p.currentModel;
    j["opusModelCheck"] = p.opusModelCheck;
    j["opusModel"] = p.opusModel;
    j["sonnetModelCheck"] = p.sonnetModelCheck;
    j["sonnetModel"] = p.sonnetModel;
    j["haikuModelCheck"] = p.haikuModelCheck;
    j["haikuModel"] = p.haikuModel;
    j["fixAttribution"] = p.fixAttribution;
    return j;
}

std::string getConfigPath()
{
    std::string configHome;
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0] != '\0')
        configHome = xdg;
    else
        configHome = std::string(std::getenv("HOME")) + "/.config";
    return configHome + "/haiclaude-tui/config.json";
}

Config loadConfig()
{
    Config cfg;
    cfg.workDir = std::string(std::getenv("HOME"));

    std::string path = getConfigPath();
    std::ifstream f(path);
    if (!f.is_open())
        return cfg;

    try {
        json j = json::parse(f);

        cfg.mode = j.value("mode", 0);
        cfg.cloudModel = j.value("cloudModel", 0);
        cfg.workDir = j.value("workDir", cfg.workDir);
        cfg.yoloMode = j.value("yoloMode", false);
        cfg.activeProfile = j.value("activeProfile", "");

        if (j.contains("api") && j["api"].is_object())
            cfg.api = profileFromJson(j["api"]);

        if (j.contains("profiles") && j["profiles"].is_object()) {
            for (auto& [key, val] : j["profiles"].items()) {
                cfg.profileNames.push_back(key);
                cfg.profiles[key] = profileFromJson(val);
            }
        }
    } catch (...) {
        // Return defaults on parse error
    }

    return cfg;
}

void saveConfig(const Config& cfg)
{
    std::string path = getConfigPath();

    // Create directory if needed
    fs::create_directories(fs::path(path).parent_path());

    json j;
    j["mode"] = cfg.mode;
    j["cloudModel"] = cfg.cloudModel;
    j["workDir"] = cfg.workDir;
    j["yoloMode"] = cfg.yoloMode;
    j["activeProfile"] = cfg.activeProfile;
    j["api"] = profileToJson(cfg.api);

    json profiles;
    for (const auto& name : cfg.profileNames)
        profiles[name] = profileToJson(cfg.profiles.at(name));
    j["profiles"] = profiles;

    std::ofstream f(path);
    if (f.is_open())
        f << j.dump(2) << std::endl;
}

void saveProfile(Config& cfg, const std::string& name, const ProfileSettings& settings)
{
    if (std::find(cfg.profileNames.begin(), cfg.profileNames.end(), name) == cfg.profileNames.end())
        cfg.profileNames.push_back(name);
    cfg.profiles[name] = settings;
    cfg.activeProfile = name;
}

void deleteProfile(Config& cfg, const std::string& name)
{
    cfg.profileNames.erase(
        std::remove(cfg.profileNames.begin(), cfg.profileNames.end(), name),
        cfg.profileNames.end());
    cfg.profiles.erase(name);
    if (cfg.activeProfile == name)
        cfg.activeProfile.clear();
}

ProfileSettings* loadProfile(Config& cfg, const std::string& name)
{
    auto it = cfg.profiles.find(name);
    if (it != cfg.profiles.end()) {
        cfg.activeProfile = name;
        return &it->second;
    }
    return nullptr;
}
