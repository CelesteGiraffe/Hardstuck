# Hardstuck — Contributor Guide (short)

One-liner: Hardstuck is a BakkesMod plugin that provides backend/API hooks and payload builders used by an external front-end to track training and MMR progress.

This doc is short, exact, and aimed at maintainers who want to build, package, and review contributions quickly.

---

## How it works (internals, in a paragraph)

The plugin hooks into BakkesMod's plugin lifecycle and Rocket League game events. Right now the important parts are the backend, API client, and payload builders.The in-game GUI overlay is NOT implemented yet and should be considered TODO.

Primary runtime flow (current submission): game events & input → backend/payload builder → API client → external front-end app for telemetry/tracking.

---

## Where the main files live (explicit)

Top-level plugin files (root of `Hardstuck/`):

- `Hardstuck.cpp`, `Hardstuck.h` — plugin registration & lifecycle (entry point)
- `pch.*` — precompiled header used across the project


Primary implementation under `Hardstuck/src/` (API-focused):

- `src/backend/ApiClient.cpp` — HTTP client wrapper and API integration (important for front-end)
- `src/backend/HsBackend.cpp` — backend logic and game-event handlers (where telemetry is collected)
- `src/payload/HsPayloadBuilder.cpp` — constructs payloads sent to external services
- `src/history/HistoryJson.cpp` — local history persistence (matches/training data)
- `src/settings/SettingsService.cpp` — settings and configuration used by backend pieces

UI files exist (`src/ui/*`, `IMGUI/`) but the overlay/front-end UI is not functional in this submission.

Support libraries and UI code are in `IMGUI/` (lots of ImGui helpers and controls).

Diagnostics and logging helpers in `diagnostics/`.

---

## Build — exact steps you need

Follow these steps to get a clean Release build if you need to test the plugin locally:

1. Install Visual Studio 2022/2023 with the Desktop C++ workload and ensure Platform Toolset `v143` is installed.
   - If you’re missing v143: open Visual Studio Installer → Modify → Individual components → search for "MSVC v143" and install. (BakkesMod SDK compatibility.)
2. Open `Hardstuck.sln`.
3. Select `Release` configuration and platform `x64`.
4. In project properties: General → Platform Toolset → set to `v143` (if not already).
5. Build the solution (Build → Build Solution).

Command line example:

```powershell
# Run from a Developer PowerShell for Visual Studio which already has MSVC in PATH
msbuild Hardstuck.sln /p:Configuration=Release /p:Platform=x64
```

Build artifacts are usually placed in `build/` and `plugins/` contains linker/output helper files.

---

## Packaging and the JSON manifest

BakkesMod expects that a plugin's release zip contains the plugin DLL and a JSON manifest at the root of the zip. For this repo, the required files you should include are:

- `Hardstuck.dll` — `Release/x64` build output
- `Hardstuck.json` — plugin manifest (must sit next to the DLL in the plugin folder)

Optional: any assets or data used by the backend; keep assets in `assets/` or `data/` in the same zip. No in-game GUI assets are required for this submission.

If you have a packaging script, make sure it copies `Hardstuck.dll` and `Hardstuck.json` into the package root before zipping so BakkesMod can read the JSON at plugin install time.

---

## Known pitfalls and gotchas (short, for maintainers)

1. Playlist IDs & game events: Rocket League sends different event payloads for client vs dedicated server, so code that assumes certain fields will be fragile. Add defensive checks and logging in `src/backend/HsBackend.cpp` and `src/history/HistoryJson.cpp`.
2. Inconsistent wrapper behavior: several engine wrappers can be missing fields or return null pointers on certain events. Validate pointers and add null checks early.

---

## Reviewing a contribution quickly (short checklist for maintainers)

1. Ensure PR changes only affect relevant areas (UI vs payload vs backend) and the new code is tested locally.
2. Confirm no build flag changes that force a toolset change or new external dependency.
3. Build `Release/x64` and test plugin load with a clean BakkesMod `plugins/` folder.
4. Validate JSON manifest still contains correct plugin version and metadata.

---