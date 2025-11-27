// HsBackend.h
#pragma once

#include <string>
#include <vector>
#include <future>
#include <mutex>
#include <chrono>

#include "history/HistoryTypes.h"
#include "backend/ApiClient.h"
#include "payload/HsPayloadBuilder.h"

class CVarManagerWrapper;
class GameWrapper;
class SettingsService;

// Thin backend that owns API client, async request state, history cache, and payload cache.
class HsBackend
{
public:
    HsBackend(std::unique_ptr<ApiClient> apiClient,
               CVarManagerWrapper* cvarManager,
               GameWrapper* gameWrapper,
               SettingsService* settingsService);

    // Network + logging of match payloads
    void DispatchPayloadAsync(const std::string& endpoint, const std::string& body);

    // Upload a snapshot of ranked MMR for all configured playlists.
    // Returns true if at least one payload was dispatched.
    bool UploadMmrSnapshot(const char* contextTag);

    // Fetch history from the API and update internal cache.
    void FetchHistory();

    // Cache the last successfully built match payload for retry.
    void CacheLastPayload(const std::string& payload, const char* contextTag);

    // Re-dispatch the cached payload, if any.
    bool DispatchCachedPayload(const char* reason);

    // Snapshot request state for UI (thread-safe copy).
    void SnapshotRequestState(std::string& lastResponse, std::string& lastError) const;

    // Snapshot history state for UI (thread-safe copy).
    void SnapshotHistory(HistorySnapshot& snapshot,
                         std::string& errorMessage,
                         bool& loading,
                         std::chrono::system_clock::time_point& lastFetched) const;

    // Should be called when shutting down to clean up ready futures.
    void CleanupFinishedRequests();

private:
    // Non-owning pointers to plugin services
    CVarManagerWrapper* cvarManager_;
    GameWrapper*        gameWrapper_;
    SettingsService*    settingsService_;

    // API client owned by backend
    std::unique_ptr<ApiClient> apiClient_;

    // Request / response state
    mutable std::mutex requestMutex_;
    std::vector<std::future<void>> pendingRequests_;
    std::string lastResponseMessage_;
    std::string lastErrorMessage_;

    // History state
    mutable std::mutex historyMutex_;
    HistorySnapshot historySnapshot_;
    std::string historyErrorMessage_;
    bool historyLoading_{false};
    std::chrono::system_clock::time_point historyLastFetched_{};

    // Cached last match payload
    mutable std::mutex payloadMutex_;
    std::string lastPayload_;
    std::string lastPayloadContext_;
};
