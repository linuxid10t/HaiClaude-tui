#pragma once

#include <map>
#include <string>
#include <vector>

struct ProfileSettings {
    std::string url = "https://api.anthropic.com/v1";
    std::string apiKey;
    bool saveKey = false;

    bool currentModelCheck = false;
    std::string currentModel = "claude-sonnet-4-6";

    bool opusModelCheck = false;
    std::string opusModel = "claude-opus-4-20250514";

    bool sonnetModelCheck = false;
    std::string sonnetModel = "claude-sonnet-4-6";

    bool haikuModelCheck = false;
    std::string haikuModel = "claude-haiku-4-20250514";

    bool fixAttribution = false;
};

struct Config {
    int mode = 0;           // 0=Cloud, 1=API
    int cloudModel = 0;     // 0=Opus, 1=Sonnet, 2=Haiku
    std::string workDir;
    bool yoloMode = false;

    // Current API settings (not tied to a profile)
    ProfileSettings api;

    // Profiles
    std::vector<std::string> profileNames;
    std::map<std::string, ProfileSettings> profiles;
    std::string activeProfile;
};

// Returns ~/.config/haiclaude-tui/config.json
std::string getConfigPath();

Config loadConfig();
void saveConfig(const Config& cfg);

// Profile helpers
void saveProfile(Config& cfg, const std::string& name, const ProfileSettings& settings);
void deleteProfile(Config& cfg, const std::string& name);
ProfileSettings* loadProfile(Config& cfg, const std::string& name);
