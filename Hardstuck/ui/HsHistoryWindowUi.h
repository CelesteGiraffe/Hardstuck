#pragma once

#include <string>
#include <chrono>
#include <vector>
#include <unordered_map>

#include "history/HistoryTypes.h"   // or wherever HistorySnapshot / MmrHistoryEntry live

// Renders the history window ImGui UI.
void HsRenderHistoryWindowUi(
    HistorySnapshot const& snapshot,
    std::string const& errorMessage,
    bool loading,
    std::chrono::system_clock::time_point lastFetched,
    bool* showHistoryWindow
);
