// HsBackend.h
#pragma once

#include <string>
#include <vector>
#include <deque>
#include <future>
#include <mutex>
#include <chrono>

#include "history/HistoryTypes.h"
#include "storage/LocalDataStore.h"
#include "payload/HsPayloadBuilder.h"

class CVarManagerWrapper;
class GameWrapper;
class SettingsService;

// Thin backend that owns local storage, async request state, history cache, and payload cache.
class HsBackend
{
public:
    HsBackend(std::unique_ptr<LocalDataStore> dataStore,
              std::string userId,
               CVarManagerWrapper* cvarManager,
               GameWrapper* gameWrapper,
               SettingsService* settingsService);

    // Network + logging of match payloads
    void DispatchPayloadAsync(const std::string& endpoint, const std::string& body);

    // Upload a snapshot of ranked MMR for all configured playlists.
    // Returns true if at least one payload was dispatched.
    bool UploadMmrSnapshot(const char* contextTag, const std::string& sessionType);

    // Fetch history from the API and update internal cache.
    void FetchHistory();

    // Cache the last successfully built match payload for retry.
    void CacheLastPayload(const std::string& payload, const char* contextTag);

    // Re-dispatch the cached payload, if any.
    bool DispatchCachedPayload(const char* reason);

    // Snapshot request state for UI (thread-safe copy).
    void SnapshotRequestState(std::string& lastResponse, std::string& lastError) const;
    void SnapshotStorageDiagnostics(std::string& status, size_t& bufferedCount) const;
    void FlushBufferedWrites();

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

    // Local data store owned by backend
    std::unique_ptr<LocalDataStore> dataStore_;
    std::string userId_;

    // Request / response state
    mutable std::mutex requestMutex_;
    std::vector<std::future<void>> pendingRequests_;
    std::string lastResponseMessage_;
    std::string lastErrorMessage_;
    std::deque<std::string> bufferedPayloads_;
    std::string lastWriteStatus_;
    static constexpr size_t kMaxBufferedPayloads = 8;

    // History state
    mutable std::mutex historyMutex_;
    HistorySnapshot historySnapshot_;
    std::string historyErrorMessage_;
    bool historyLoading_{false};
    std::chrono::system_clock::time_point historyLastFetched_{};
    bool historyDirty_{true};

    // Cached last match payload
    mutable std::mutex payloadMutex_;
    std::string lastPayload_;
    std::string lastPayloadContext_;
};
