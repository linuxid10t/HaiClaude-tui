# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Configure and build (from project root)
cd build && cmake .. && make -j$(nproc)

# Rebuild after changes (from build/)
make -j$(nproc)
```

No tests, linter, or CI pipeline exists. Build verification is compilation only.

## Architecture

A C++17 TUI launcher for Claude Code. Four source files in `src/`:

- **main.cpp** — Entry point. Loads config, runs TUI, executes the resulting command.
- **tui_app.cpp/h** — Full-screen FTXUI interface with mode selection (Cloud/API), directory browser, profile management. `runTUI(Config&)` returns the command string to execute. Uses `ProfileLoadingWrapper` to load the selected profile's settings into the working `api` variable before rendering, ensuring inputs reflect the correct values.
- **config.cpp/h** — JSON-based config at `~/.config/haiclaude-tui/config.json`. `Config` struct holds mode, model, workDir, and a `ProfileSettings` map. `saveProfile`/`deleteProfile`/`loadProfile` manage named profiles in memory; `saveConfig` persists everything to disk.
- **launcher.cpp/h** — `buildCommand()` constructs the Claude CLI invocation. `launchOrExec()` runs it, auto-detecting the terminal emulator.

Flow: `loadConfig()` → `runTUI()` → user interaction → `buildCommand()` → `launchOrExec()`

## Dependencies (FetchContent)

- **FTXUI v6.1.9** — Terminal UI framework. Provides Screen, Components (Menu, Input, Button, Radiobox, Checkbox, Modal, Dropdown), DOM styling, and event handling.
- **nlohmann/json v3.11.3** — JSON serialization for config persistence.

Both are downloaded automatically by CMake on first build.

## FTXUI Patterns Used Here

- `Container::Vertical`/`Horizontal` for layout navigation; selector defaults to child index 0
- `Renderer(component, lambda)` to add labels alongside focusable inputs — the lambda renders both label and input element, while the child handles events
- `FilterVerticalNav(component)` — custom wrapper that blocks ArrowUp/ArrowDown from reaching the child (returns false), so parent `Container::Vertical` handles row navigation instead. Used on Input components to prevent them from consuming up/down for cursor movement
- `Maybe(component, condition)` for conditional visibility
- `CatchEvent` via `|=` attaches handlers to a component, but note that `|=` creates a **new** wrapper component — if the original was already added to a container, the handler won't be in the event chain. Apply `|=` **before** adding the component to containers
- Container::Horizontal children that are non-focusable `Renderer` labels receive initial focus at index 0 but can't display a cursor — avoid putting non-focusable children in horizontal containers, or the cursor becomes invisible
- `yframe` + `vscroll_indicator` for scrollable menus with more items than the visible area; also used on the main UI content so the layout scrolls instead of clipping on short terminals
- `ProfileLoadingWrapper(component, names, profiles, api, profileIndex)` — custom wrapper component that loads the selected profile into the working `api` variable during `OnRender()`. Required because `apiSection` renders before `profileSection` in the main container tree; without it the inputs would read stale values. The wrapper must be placed inline in the tree where it needs to run, not as a separate hook.
- Nested `Modal` decorators for layered popups: `Modal(innerModal, &showInner)(Modal(outerContent, &showOuter)(mainContainer))`. The outermost CatchEvent runs first, so key handlers for the inner modal (e.g. Esc to close popup) must be checked before the outer modal's handlers.
- `dbox` (deck box) overlays one element on top of another — used to render the "new directory" popup on top of the browser modal window.
