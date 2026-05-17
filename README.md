# HaiClaude-tui

A terminal UI launcher for [Claude Code](https://docs.anthropic.com/en/docs/claude-code), built with C++17 and [FTXUI](https://github.com/ArthurSonzogni/ftxui).

Provides a full-screen interactive menu to configure and launch Claude Code — select models, set API endpoints, manage profiles, and pick a working directory — all without memorizing CLI flags.

## Features

- **Cloud mode** — launch Claude Code with Anthropic's hosted API (Opus / Sonnet / Haiku)
- **API mode** — point at any Anthropic-compatible endpoint with a custom API key
- **Profile management** — save and switch between named API configurations
- **Model overrides** — set custom model names for each tier (Opus / Sonnet / Haiku)
- **Directory browser** — pick a working directory with an in-app file browser; press `n` to create a new directory
- **YOLO mode** — skip all permission prompts (`--dangerously-skip-permissions`)
- **Attribution header fix** — option to disable the `x-attribution-header` for local LLMs
- **Persistent config** — settings saved to `~/.config/haiclaude-tui/config.json`

## Building

Requires CMake 3.14+ and a C++17 compiler. Dependencies (FTXUI, nlohmann/json) are fetched automatically.

```bash
cd build && cmake .. && make -j$(nproc)
```

The binary is `build/haiclaude-tui`. Install system-wide:

```bash
cmake --install .          # copies to /usr/local/bin by default
```

## Usage

Run the launcher:

```
haiclaude-tui
```

Navigate with arrow keys. Configure your launch settings and press **LAUNCH CLAUDE CODE**.

### Cloud mode

Select **Cloud**, pick a model, and launch. No API key needed — uses your existing Claude Code credentials.

### API mode

Select **API (API key)**, enter your endpoint URL and API key. Supports any Anthropic-compatible API. Optionally check model overrides to set custom model names.

### Profiles

In API mode, name a profile and click **Save Profile** to store the current settings. Switch between profiles with the dropdown. Profiles are persisted across sessions.

## Configuration

Stored at `~/.config/haiclaude-tui/config.json`. Created automatically on first save. Edit manually or through the TUI.

## License

MIT
