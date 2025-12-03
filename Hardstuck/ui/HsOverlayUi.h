// HsOverlayUi.h
#pragma once

#include <string>
#include <functional>
#include <chrono>

#include "history/HistoryTypes.h"

class CVarManagerWrapper;

// Reuse the same callback types as settings UI
using HsExecuteHistoryWindowFn = std::function<void()>;
using HsFetchHistoryFn         = std::function<void()>;

// Draws the small overlay window and handles the optional demo toggle.
void HsRenderOverlayUi(
    CVarManagerWrapper* cvarManager,
    const std::string& lastResponse,
    const std::string& lastError,
    HistorySnapshot const& historySnapshot,
    std::string const& historyError,
    bool historyLoading,
    std::chrono::system_clock::time_point historyLastFetched,
    const std::string& activeSessionLabel,
    bool manualSessionActive,
    HsExecuteHistoryWindowFn executeHistoryWindowCommand,
    HsFetchHistoryFn fetchHistoryFn
);
