# Claude Usage Monitor

Small Qt 6 desktop accessory for Claude Code usage.

## Features

- Frameless floating window
- Drag anywhere to move
- Double-click to collapse or expand details
- Right-click menu for update, opacity, always-on-top, usage pages, and quit
- Reads Claude Code credentials from `CLAUDE_CONFIG_DIR/.credentials.json` or `%USERPROFILE%/.claude/.credentials.json`
- Fetches Claude usage from `https://api.anthropic.com/api/oauth/usage`
- Opens the Codex usage page at `https://chatgpt.com/codex/cloud/settings/analytics#usage`

Codex usage is intentionally not scraped or automated. The app opens the authenticated usage page instead, because there is not a stable public personal-usage API equivalent to the Claude usage endpoint.

## Platform Notes

- Windows uses WinHTTP for the Claude usage request.
- Linux uses libcurl for the Claude usage request.
- Claude Code credentials are read from `CLAUDE_CONFIG_DIR/.credentials.json` when set, otherwise from the user's home directory at `.claude/.credentials.json`.

## Build

Open this folder in Qt Creator, configure with Qt 6, and build the `ClaudeUsageMonitor` target.

From a terminal with Qt and CMake on `PATH`:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

On Linux, install Qt 6 Widgets development files and libcurl development files first. For example:

```bash
sudo apt install qt6-base-dev libcurl4-openssl-dev cmake build-essential
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
