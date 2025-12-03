// HsSettingsUi.h
#pragma once

#include <filesystem>
#include <functional>

class ISettingsService;
class CVarManagerWrapper;

// Callbacks the settings UI can invoke.
using HsToggleMenuFn = std::function<void()>;
using HsToggleOverlayFn = std::function<void()>;

// Renders the settings ImGui UI.
void HsRenderSettingsUi(
    ISettingsService* settingsService,
    CVarManagerWrapper* cvarManager,
    HsToggleMenuFn toggleMenu,
    HsToggleOverlayFn toggleOverlay,
    const std::filesystem::path& storePath
);
