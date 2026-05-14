#include "launcher.h"
#include "config.h"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;
using json = nlohmann::json;

// ---------------------------------------------------------------------------
// shellEscape
// ---------------------------------------------------------------------------

std::string shellEscape(const std::string& s)
{
    std::string result = "'";
    for (char c : s) {
        if (c == '\'')
            result += "'\\''";
        else
            result += c;
    }
    result += "'";
    return result;
}

// ---------------------------------------------------------------------------
// findClaude
// ---------------------------------------------------------------------------

static bool fileExists(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

std::string findClaude()
{
    std::string home = std::getenv("HOME");
    const char* pathEnv = std::getenv("PATH");
    if (!pathEnv) pathEnv = "";

    std::vector<std::string> candidates = {
        home + "/.npm-global/bin/claude",
        home + "/.local/bin/claude",
        "/usr/local/bin/claude",
    };

    for (const auto& path : candidates) {
        if (fileExists(path))
            return path;
    }

    // Search PATH
    std::istringstream paths(pathEnv);
    std::string dir;
    while (std::getline(paths, dir, ':')) {
        std::string full = dir + "/claude";
        if (fileExists(full))
            return full;
    }

    return "claude";
}

// ---------------------------------------------------------------------------
// findTerminal
// ---------------------------------------------------------------------------

static std::string which(const std::string& name)
{
    const char* pathEnv = std::getenv("PATH");
    if (!pathEnv) return "";

    std::istringstream paths(pathEnv);
    std::string dir;
    while (std::getline(paths, dir, ':')) {
        std::string full = dir + "/" + name;
        if (fileExists(full))
            return full;
    }
    return "";
}

std::string findTerminal()
{
    const char* term = std::getenv("TERMINAL");
    if (term && term[0] != '\0') {
        std::string p = which(term);
        if (!p.empty()) return p;
    }

    const char* names[] = {
        "xterm", "alacritty", "kitty",
        "gnome-terminal", "konsole",
        "xfce4-terminal", "lxterminal", "x-terminal-emulator",
    };
    for (const char* n : names) {
        std::string p = which(n);
        if (!p.empty()) return p;
    }
    return "";
}

// ---------------------------------------------------------------------------
// buildCommand
// ---------------------------------------------------------------------------

std::string buildCommand(int mode, int cloudModel, const std::string& workDir,
                         bool yoloMode, const ProfileSettings& api)
{
    std::string claudeBin = findClaude();
    std::string cmd;

    if (!workDir.empty())
        cmd += "cd " + shellEscape(workDir) + " && ";

    if (mode == 0) {
        // Cloud mode
        cmd += "unset ANTHROPIC_API_KEY && " + claudeBin;
        if (cloudModel == 1)      cmd += " --model sonnet";
        else if (cloudModel == 2) cmd += " --model haiku";
        else                      cmd += " --model opus";
    } else {
        // API mode
        cmd += "trap 'mv \"$HOME/.claude/.credentials.json.bak\" "
               "\"$HOME/.claude/.credentials.json\" 2>/dev/null' EXIT; ";
        cmd += "mv \"$HOME/.claude/.credentials.json\" "
               "\"$HOME/.claude/.credentials.json.bak\" 2>/dev/null; ";
        cmd += "ANTHROPIC_BASE_URL=" + shellEscape(api.url) +
               " ANTHROPIC_API_KEY=" + shellEscape(api.apiKey);

        auto addEnv = [&](bool check, const std::string& val, const std::string& var) {
            if (check && !val.empty())
                cmd += " " + var + "=" + shellEscape(val);
        };
        addEnv(api.opusModelCheck,   api.opusModel,   "ANTHROPIC_DEFAULT_OPUS_MODEL");
        addEnv(api.sonnetModelCheck, api.sonnetModel, "ANTHROPIC_DEFAULT_SONNET_MODEL");
        addEnv(api.haikuModelCheck,  api.haikuModel,  "ANTHROPIC_DEFAULT_HAIKU_MODEL");

        cmd += " " + claudeBin;

        if (api.currentModelCheck && !api.currentModel.empty())
            cmd += " --model " + shellEscape(api.currentModel);
    }

    if (yoloMode)
        cmd += " --dangerously-skip-permissions";

    return cmd;
}

// ---------------------------------------------------------------------------
// applyAttributionHeaderFix
// ---------------------------------------------------------------------------

void applyAttributionHeaderFix()
{
    std::string home = std::getenv("HOME");
    std::string settingsPath = home + "/.claude/settings.json";
    json root;

    std::ifstream inf(settingsPath);
    if (inf.is_open()) {
        try {
            inf >> root;
        } catch (...) {}
        inf.close();
    }

    if (!root.contains("env") || !root["env"].is_object())
        root["env"] = json::object();
    root["env"]["CLAUDE_CODE_ATTRIBUTION_HEADER"] = "0";

    std::ofstream outf(settingsPath);
    if (outf.is_open())
        outf << root.dump(2) << std::endl;
}

// ---------------------------------------------------------------------------
// launchOrExec / launchInTerminal
// ---------------------------------------------------------------------------

void launchInTerminal(const std::string& cmd)
{
    std::string termPath = findTerminal();
    if (termPath.empty()) return;

    std::string termName = fs::path(termPath).filename().string();

    if (fork() == 0) {
        // Detach from parent
        setsid();

        if (termName == "gnome-terminal")
            execl(termPath.c_str(), termName.c_str(), "--", "bash", "-l", "-c", cmd.c_str(), nullptr);
        else if (termName == "kitty")
            execl(termPath.c_str(), termName.c_str(), "bash", "-l", "-c", cmd.c_str(), nullptr);
        else
            execl(termPath.c_str(), termName.c_str(), "-e", "bash", "-l", "-c", cmd.c_str(), nullptr);
    }
}

void launchOrExec(const std::string& cmd)
{
    if (isatty(STDIN_FILENO)) {
        // In a TTY: exec directly (replaces this process)
        execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
    } else {
        // Not in TTY: launch in a terminal emulator
        launchInTerminal(cmd);
    }
}
