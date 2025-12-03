// HsOverlayUi.h
#pragma once

#include <string>
#include <functional>

class CVarManagerWrapper;

// Reuse the same callback types as settings UI
using HsTriggerManualUploadFn  = std::function<void()>;
using HsExecuteHistoryWindowFn = std::function<void()>;

// Draws the small overlay window and handles the optional demo toggle.
void HsRenderOverlayUi(
    CVarManagerWrapper* cvarManager,
    const std::string& lastResponse,
    const std::string& lastError,
    const std::string& activeSessionLabel,
    bool manualSessionActive,
    HsTriggerManualUploadFn triggerManualUpload,
    HsExecuteHistoryWindowFn executeHistoryWindowCommand
);
