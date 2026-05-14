#pragma once

#include <string>

struct ProfileSettings;

std::string shellEscape(const std::string& s);
std::string findClaude();
std::string findTerminal();
std::string buildCommand(int mode, int cloudModel, const std::string& workDir,
                         bool yoloMode, const ProfileSettings& api);
void applyAttributionHeaderFix();
void launchOrExec(const std::string& cmd);
void launchInTerminal(const std::string& cmd);
