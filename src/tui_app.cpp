#include "tui_app.h"
#include "config.h"
#include "launcher.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;
using namespace ftxui;

// ---------------------------------------------------------------------------
// Directory browser component
// ---------------------------------------------------------------------------

static Component DirBrowser(std::string* path, std::function<void()>& outRefresh)
{
    auto entries = std::make_shared<std::vector<std::string>>();
    auto selected = std::make_shared<int>(0);

    auto refresh = [path, entries, selected]() {
        entries->clear();
        try {
            for (const auto& entry : fs::directory_iterator(*path)) {
                if (entry.is_directory())
                    entries->push_back(entry.path().filename().string());
            }
            std::sort(entries->begin(), entries->end());
            entries->insert(entries->begin(), "..");
        } catch (...) {}
        *selected = 0;
    };

    refresh();
    outRefresh = refresh;

    auto menu = Menu(entries.get(), selected.get());
    menu |= CatchEvent([path, entries, selected, refresh](Event e) {
        if (e == Event::Return) {
            if (*selected >= 0 && *selected < (int)entries->size()) {
                std::string chosen = (*entries)[*selected];
                if (chosen == "..") {
                    auto parent = fs::path(*path).parent_path();
                    if (!parent.empty())
                        *path = parent.string();
                } else {
                    fs::path newPath = fs::path(*path) / chosen;
                    if (fs::is_directory(newPath))
                        *path = newPath.string();
                }
                refresh();
                return true;
            }
        }
        return false;
    });

    return menu;
}

// Wrap a component so up/down arrow keys pass through for parent navigation
static Component FilterVerticalNav(Component child)
{
    class Impl : public ComponentBase {
    public:
        explicit Impl(Component child) { Add(std::move(child)); }
        Element OnRender() override {
            return ActiveChild() ? ActiveChild()->Render() : text("");
        }
        bool OnEvent(Event event) override {
            if (event == Event::ArrowUp || event == Event::ArrowDown)
                return false;
            return ActiveChild() ? ActiveChild()->OnEvent(std::move(event)) : false;
        }
        bool Focusable() const override {
            return !children_.empty() && children_[0]->Focusable();
        }
    };
    return std::make_shared<Impl>(std::move(child));
}

// ---------------------------------------------------------------------------
// Loads the selected profile into `api` before delegating to the wrapped
// component.  This runs during OnRender, ensuring inputs that read `api`
// by pointer see the correct values.
// ---------------------------------------------------------------------------

static Component ProfileLoadingWrapper(Component child,
                                       const std::vector<std::string>* names,
                                       const std::map<std::string, ProfileSettings>* profiles,
                                       ProfileSettings* api,
                                       int* profileIndex)
{
    class Impl : public ComponentBase {
    public:
        Impl(Component c,
             const std::vector<std::string>* n,
             const std::map<std::string, ProfileSettings>* p,
             ProfileSettings* a,
             int* idx)
            : names_(n), profiles_(p), api_(a), profileIdx_(idx) {
            Add(std::move(c));
        }

        Element OnRender() override {
            if (names_ && profiles_ && api_ && profileIdx_) {
                int cur = *profileIdx_;
                if (cur != lastIdx_) {
                    if (cur >= 0 && cur < (int)names_->size()) {
                        auto it = profiles_->find((*names_)[cur]);
                        if (it != profiles_->end())
                            *api_ = it->second;
                    }
                    lastIdx_ = cur;
                }
            }
            return ActiveChild() ? ActiveChild()->Render() : text("");
        }

    private:
        const std::vector<std::string>* names_ = nullptr;
        const std::map<std::string, ProfileSettings>* profiles_ = nullptr;
        ProfileSettings* api_ = nullptr;
        int* profileIdx_ = nullptr;
        int lastIdx_ = -1;
    };
    return std::make_shared<Impl>(std::move(child), names, profiles, api, profileIndex);
}

// ---------------------------------------------------------------------------
// Main TUI
// ---------------------------------------------------------------------------

std::string runTUI(Config& cfg)
{
    auto screen = ScreenInteractive::Fullscreen();

    // --- State ---
    int mode = cfg.mode;
    int cloudModel = cfg.cloudModel;
    std::string workDir = cfg.workDir;
    bool yoloMode = cfg.yoloMode;

    ProfileSettings api = cfg.api;

    // Profile management state
    int profileIndex = 0;
    std::vector<std::string> profileDisplayNames = cfg.profileNames;
    if (profileDisplayNames.empty())
        profileDisplayNames.push_back("(none)");
    std::string newProfileName;

    // Status/error message
    std::string statusMsg;
    Color statusColor = Color::White;

    // Directory browser state
    bool showBrowser = false;
    std::string browserPath = workDir;
    std::string newDirName;
    bool showNewDir = false;
    std::function<void()> browserRefresh;
    auto browserComponent = DirBrowser(&browserPath, browserRefresh);

    auto newDirInput = Input(&newDirName, "new dir name");
    newDirInput |= CatchEvent([&](Event e) {
        if (e == Event::Return) {
            if (!newDirName.empty()) {
                try {
                    fs::create_directories(fs::path(browserPath) / newDirName);
                    browserRefresh();
                } catch (...) {}
            }
            newDirName.clear();
            showNewDir = false;
            return true;
        }
        return false;
    });

    // Result
    std::string resultCommand;

    // --- Mode selection ---
    static const std::vector<std::string> modeEntries = {"Cloud", "API (API key)"};
    auto modeRadiobox = Radiobox(&modeEntries, &mode);

    // --- Cloud model selection ---
    static const std::vector<std::string> modelEntries = {"Opus", "Sonnet", "Haiku"};
    auto modelRadiobox = Radiobox(&modelEntries, &cloudModel);

    // --- Work directory input ---
    auto workDirInput = Input(&workDir, "directory path");

    // --- API settings inputs ---
    auto apiUrlInput = Input(&api.url, "API URL");
    auto apiKeyInput = Input(&api.apiKey, "API key");

    // Override checkboxes + inputs
    auto currentModelCheck = Checkbox("Override current model", &api.currentModelCheck);
    auto currentModelInput = Input(&api.currentModel, "model name");
    auto opusModelCheck = Checkbox("Override ANTHROPIC_DEFAULT_OPUS_MODEL", &api.opusModelCheck);
    auto opusModelInput = Input(&api.opusModel, "model name");
    auto sonnetModelCheck = Checkbox("Override ANTHROPIC_DEFAULT_SONNET_MODEL", &api.sonnetModelCheck);
    auto sonnetModelInput = Input(&api.sonnetModel, "model name");
    auto haikuModelCheck = Checkbox("Override ANTHROPIC_DEFAULT_HAIKU_MODEL", &api.haikuModelCheck);
    auto haikuModelInput = Input(&api.haikuModel, "model name");

    auto saveKeyCheck = Checkbox("Remember key", &api.saveKey);
    auto fixAttributionCheck = Checkbox("Fix attribution header (for local LLMs)", &api.fixAttribution);
    auto yoloCheckbox = Checkbox("YOLO mode (skip all permissions)", &yoloMode);

    // --- Profile management ---
    auto profileDropdown = Dropdown(&profileDisplayNames, &profileIndex);
    auto profileNameInput = Input(&newProfileName, "New profile name");

    // --- Cloud model section ---
    auto cloudModelSection = Renderer(modelRadiobox, [=]() mutable {
        return hbox({
            text("Model: ") | bold,
            modelRadiobox->Render() | flex,
        });
    });

    // API settings section
    auto currentModelMaybe = Maybe(currentModelInput, &api.currentModelCheck);
    auto opusModelMaybe = Maybe(opusModelInput, &api.opusModelCheck);
    auto sonnetModelMaybe = Maybe(sonnetModelInput, &api.sonnetModelCheck);
    auto haikuModelMaybe = Maybe(haikuModelInput, &api.haikuModelCheck);

    auto apiSection = Container::Vertical({
        Renderer([] { return text("API Settings") | bold | underlined; }),
        FilterVerticalNav(Renderer(apiUrlInput, [=]() mutable {
            return hbox({
                text("API URL: ") | bold | size(WIDTH, EQUAL, 12),
                apiUrlInput->Render() | flex,
            });
        })),
        FilterVerticalNav(Renderer(apiKeyInput, [=]() mutable {
            return hbox({
                text("API Key: ") | bold | size(WIDTH, EQUAL, 12),
                apiKeyInput->Render() | flex,
            });
        })),
        saveKeyCheck,
        Renderer([] { return separator(); }),
        currentModelCheck,
        FilterVerticalNav(Renderer(currentModelMaybe, [=]() mutable {
            return hbox({
                text("  Model: ") | bold | size(WIDTH, EQUAL, 12),
                currentModelMaybe->Render() | flex,
            });
        })),
        opusModelCheck,
        FilterVerticalNav(Renderer(opusModelMaybe, [=]() mutable {
            return hbox({
                text("  Model: ") | bold | size(WIDTH, EQUAL, 12),
                opusModelMaybe->Render() | flex,
            });
        })),
        sonnetModelCheck,
        FilterVerticalNav(Renderer(sonnetModelMaybe, [=]() mutable {
            return hbox({
                text("  Model: ") | bold | size(WIDTH, EQUAL, 12),
                sonnetModelMaybe->Render() | flex,
            });
        })),
        haikuModelCheck,
        FilterVerticalNav(Renderer(haikuModelMaybe, [=]() mutable {
            return hbox({
                text("  Model: ") | bold | size(WIDTH, EQUAL, 12),
                haikuModelMaybe->Render() | flex,
            });
        })),
        Renderer([] { return separator(); }),
        fixAttributionCheck,
    });

    // Wrapper that loads the selected profile before apiSection renders
    auto apiSectionWithProfile = ProfileLoadingWrapper(
        apiSection, &cfg.profileNames, &cfg.profiles, &api, &profileIndex);

    // Profile section
    auto profileNameFiltered = FilterVerticalNav(profileNameInput);
    auto profileRow = Container::Horizontal({
        profileDropdown,
        profileNameFiltered,
    });

    auto profileSection = Container::Vertical({
        Renderer([] { return text("API Profiles") | bold | underlined; }),
        Renderer(profileRow, [&]() mutable {
            return hbox({
                text("Select: "),
                profileDropdown->Render() | size(WIDTH, EQUAL, 20),
                text("  "),
                profileNameInput->Render() | size(WIDTH, EQUAL, 20),
            });
        }),
        Container::Horizontal({
            Button("Save Profile", [&]() {
                std::string name = newProfileName;
                if (name.empty() && profileIndex >= 0 && profileIndex < (int)cfg.profileNames.size())
                    name = cfg.profileNames[profileIndex];

                if (name.empty()) {
                    statusMsg = "Enter a profile name first";
                    statusColor = Color::Red;
                    return;
                }

                saveProfile(cfg, name, api);
                saveConfig(cfg);
                profileDisplayNames = cfg.profileNames;
                if (profileDisplayNames.empty())
                    profileDisplayNames.push_back("(none)");
                auto it = std::find(profileDisplayNames.begin(), profileDisplayNames.end(), name);
                profileIndex = std::distance(profileDisplayNames.begin(), it);
                newProfileName.clear();
                statusMsg = "Profile '" + name + "' saved";
                statusColor = Color::Green;
            }, ButtonOption::Ascii()),
            Renderer([] { return text("  "); }),
            Button("Delete Profile", [&]() {
                if (profileIndex < 0 || profileIndex >= (int)cfg.profileNames.size()) {
                    statusMsg = "No profile selected";
                    statusColor = Color::Red;
                    return;
                }
                std::string name = cfg.profileNames[profileIndex];
                deleteProfile(cfg, name);
                saveConfig(cfg);
                api = cfg.api;
                profileDisplayNames = cfg.profileNames;
                if (profileDisplayNames.empty())
                    profileDisplayNames.push_back("(none)");
                profileIndex = 0;
                statusMsg = "Profile '" + name + "' deleted";
                statusColor = Color::Green;
            }, ButtonOption::Ascii()),
        }),
    });

    auto workDirFiltered = FilterVerticalNav(workDirInput);
    auto dirRow = Container::Horizontal({
        workDirFiltered | flex,
        Button("Browse", [&]() {
            browserPath = workDir;
            showBrowser = true;
        }, ButtonOption::Ascii()),
    });

    // --- Main container ---
    Components mainComponents = {
        Renderer([] {
            return text("Claude Code Launcher") | bold | center | color(Color::Cyan);
        }),
        Renderer([] { return separator(); }),
        Renderer([] { return text("Launch mode:"); }),
        modeRadiobox,
        Maybe(cloudModelSection, [&]() { return mode == 0; }),
        Renderer(dirRow, [=]() mutable {
            return hbox({
                text("Directory: ") | bold | size(WIDTH, EQUAL, 12),
                dirRow->Render() | flex,
            });
        }),
        yoloCheckbox,
        Maybe(apiSectionWithProfile, [&]() { return mode == 1; }),
        Maybe(profileSection, [&]() { return mode == 1; }),
        Renderer([] { return separator(); }),
        Button("[ LAUNCH CLAUDE CODE ]", [&]() {
            if (!workDir.empty() && !fs::is_directory(workDir)) {
                statusMsg = "Error: Directory does not exist";
                statusColor = Color::Red;
                return;
            }
            if (mode == 1) {
                if (api.url.empty()) {
                    statusMsg = "Error: API URL cannot be empty";
                    statusColor = Color::Red;
                    return;
                }
                if (api.apiKey.empty()) {
                    statusMsg = "Error: API Key cannot be empty";
                    statusColor = Color::Red;
                    return;
                }
            }

            cfg.mode = mode;
            cfg.cloudModel = cloudModel;
            cfg.workDir = workDir;
            cfg.yoloMode = yoloMode;
            cfg.api = api;
            saveConfig(cfg);

            if (mode == 1 && api.fixAttribution)
                applyAttributionHeaderFix();

            resultCommand = buildCommand(mode, cloudModel, workDir, yoloMode, api);
            screen.Exit();
        }, ButtonOption::Ascii()),
        Button("Quit", [&]() {
            screen.Exit();
        }, ButtonOption::Ascii()),
        Renderer([&statusMsg, &statusColor]() {
            if (statusMsg.empty())
                return text("");
            return text(statusMsg) | color(statusColor);
        }),
    };

    auto mainContainer = Container::Vertical(mainComponents);

    // --- Modal for directory browser ---
    auto browserModalDecorator = Modal(browserComponent, &showBrowser);
    auto newDirModalDecorator = Modal(newDirInput, &showNewDir);
    auto root = newDirModalDecorator(browserModalDecorator(mainContainer));

    // Handle keyboard for browser modal
    root |= CatchEvent([&](Event e) {
        if (!showBrowser) return false;
        if (showNewDir) {
            if (e == Event::Escape) {
                newDirName.clear();
                showNewDir = false;
                return true;
            }
            return false;
        }
        if (e == Event::Escape) {
            showBrowser = false;
            return true;
        }
        if (e == Event::Tab) {
            workDir = browserPath;
            showBrowser = false;
            return true;
        }
        if (e == Event::Character('n')) {
            newDirName.clear();
            showNewDir = true;
            return true;
        }
        return false;
    });

    // --- Render ---
    auto renderer = Renderer(root, [&, root]() {
        auto content = root->Render();

        if (showBrowser) {
            auto browserWindow = window(text(" Browse Directory ") | bold,
                vbox({
                    text("Current: " + browserPath) | dim,
                    separator(),
                    browserComponent->Render() | flex | vscroll_indicator | yframe | size(HEIGHT, LESS_THAN, 20),
                    separator(),
                    text("Enter: open | Tab: accept | n: new dir | Esc: cancel") | dim | center,
                })) | size(WIDTH, EQUAL, 60) | size(HEIGHT, LESS_THAN, 30) | center;

            if (showNewDir) {
                auto popup = window(text(" New Directory ") | bold,
                    vbox({
                        hbox({text("Name: ") | bold, newDirInput->Render() | flex}),
                        separator(),
                        text("Enter: create  |  Esc: cancel") | dim | center,
                    })) | size(WIDTH, EQUAL, 40) | center;
                return dbox({browserWindow, popup});
            }

            return browserWindow;
        }

        return vbox({
            content | vscroll_indicator | yframe | flex,
        }) | border | size(WIDTH, EQUAL, 72);
    });

    screen.Loop(renderer);
    return resultCommand;
}
