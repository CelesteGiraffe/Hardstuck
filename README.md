# Hardstuck — BakkesMod plugin

One-line: Hardstuck is a BakkesMod plugin for basic backend communication and telemetry, it currently exposes an API client and payload builder used by an external front-end to track training and MMR progress.

Note: The GUI overlay is not functional yet. This submission focuses on the backend/API pieces that the external front-end app uses for training and MMR tracking.

---

## Quick start (for integrating apps)

This plugin is intended to run in Rocket League and act as a local bridge for an external front-end that tracks training and MMR progress. It currently does not provide an in-game GUI.

1. Build or download a release zip and extract the plugin folder into your BakkesMod `bakkesmod/plugins` folder. The release must include at minimum:
   - `Hardstuck.dll` (Release build, x64)
   - `Hardstuck.json` (plugin manifest)
2. Launch Rocket League and verify the plugin loads (check BakkesMod logs). The front-end app should communicate with the plugin's API hooks to receive telemetry and progress data.

---

## Companion app

This plugin is designed to be used alongside a front-end companion app. The recommended companion app is RL-Trainer-2 — it communicates with this plugin's local API and provides a user-facing interface for training and MMR tracking.

Link: https://github.com/CelesteGiraffe/RL-Trainer-2



## For contributors / maintainers

This repo focuses on API/communication parts right now. If you're contributing, prefer testing the backend APIs and payload code over UI changes.

Short checklist:

- Open `Hardstuck.sln` in Visual Studio (see below for toolset notes).
- Build the `Release` configuration for `x64` and package `Hardstuck.dll` and `Hardstuck.json` for release.

### Important build environment notes

- BakkesMod and this repo require MSVC toolset v143 in most cases. If your system uses a different toolset, change the Platform Toolset in Visual Studio or install v143 via the Visual Studio Installer.
- If the BakkesMod SDK is pinned to a specific toolset, match that exact toolset (v143) to avoid runtime mismatches.


### Build steps (explicit)

1. Open `Hardstuck.sln` in Visual Studio 2022/2023.
2. Ensure the solution configuration is `Release` and platform is `x64`.
3. Confirm Platform Toolset is set to `v143` (right-click Project → Properties → General → Platform Toolset) — BakkesMod SDK compatibility.
4. Build → `Build Solution`.

Artifacts are placed in the `build/` folder and the `plugins/` subdirectory for intermediate results — final release files to ship are the DLL and the JSON manifest.

---

## Where the important code lives (API-focused)

Key source and folders (paths are relative to `/Hardstuck`):

- Main app: `Hardstuck.cpp` / `Hardstuck.h` — plugin registration and main lifecycle
- UI code: `ui/` and `IMGUI/` — present but currently non-functional; do not rely on in-game overlays in this release. Tasks for later: finish and test `src/ui/HsOverlayUi.cpp` and related files.
- Backend API and payloads: `backend/` and `payload/` (`ApiClient.cpp`, `HsBackend.cpp`, `HsPayloadBuilder.cpp`)
- History tracking: `history/` (`HistoryJson.*`, `HistoryTypes.h`)
- Settings: `settings/` (`SettingsService.*`)

The `src/` subfolders mirror these areas with implementation files.

---

## Known pitfalls and troubleshooting (short)

- Rocket League has inconsistent wrappers for some game events — test features on both client and dedicated servers. The API code is defensive but confirm fields and behavior before trusting event payloads.
- Playlist IDs and game event behavior may differ in the dedicated server vs client; watch for missing fields or different event timing.