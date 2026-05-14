#pragma once

#include "config.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include <functional>

// Runs the TUI main loop. Returns the command to execute (or empty on cancel/quit).
std::string runTUI(Config& cfg);
