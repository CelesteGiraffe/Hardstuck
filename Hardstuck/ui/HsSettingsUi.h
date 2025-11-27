// HsSettingsUi.h
#pragma once

#include <functional>

class ISettingsService;
class CVarManagerWrapper;

// Callback the settings UI can invoke.
using HsTriggerManualUploadFn       = std::function<void()>;

// Renders the settings ImGui UI.
void HsRenderSettingsUi(
    ISettingsService* settingsService,
    CVarManagerWrapper* cvarManager,
    HsTriggerManualUploadFn triggerManualUpload
);
