// HsSettingsUi.h
#pragma once

#include <filesystem>
#include <functional>

class ISettingsService;
class CVarManagerWrapper;

// Callbacks the settings UI can invoke.
using HsTriggerManualUploadFn = std::function<void()>;
using HsToggleMenuFn = std::function<void()>;

// Renders the settings ImGui UI.
void HsRenderSettingsUi(
    ISettingsService* settingsService,
    CVarManagerWrapper* cvarManager,
    HsTriggerManualUploadFn triggerManualUpload,
    HsToggleMenuFn toggleMenu,
    const std::filesystem::path& storePath
);
