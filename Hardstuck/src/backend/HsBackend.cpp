// HsBackend.cpp
#include "pch.h"
#include "backend/HsBackend.h"

#include <algorithm>
#include <filesystem>

#include "diagnostics/DiagnosticLogger.h"
#include "settings/SettingsService.h"

#include "bakkesmod/wrappers/GameWrapper.h"
#include "bakkesmod/wrappers/MMRWrapper.h"
#include "bakkesmod/wrappers/UniqueIDWrapper.h"
#include "bakkesmod/wrappers/cvarmanagerwrapper.h"

HsBackend::HsBackend(std::unique_ptr<LocalDataStore> dataStore,
                     std::string userId,
                     CVarManagerWrapper* cvarManager,
                     GameWrapper* gameWrapper,
                     SettingsService* settingsService)
    : cvarManager_(cvarManager)
    , gameWrapper_(gameWrapper)
    , settingsService_(settingsService)
    , dataStore_(std::move(dataStore))
    , userId_(std::move(userId))
{
}

void HsBackend::DispatchPayloadAsync(const std::string& endpoint, const std::string& body)
{
    if (!dataStore_)
    {
        if (cvarManager_)
        {
            cvarManager_->log("HS: local data store is not configured");
        }
        return;
    }

    DiagnosticLogger::Log(std::string("DispatchPayloadAsync: endpoint=") + endpoint +
                          ", body_len=" + std::to_string(body.size()));

    {
        std::lock_guard<std::mutex> lock(requestMutex_);
        bufferedPayloads_.push_back(body);
        if (bufferedPayloads_.size() > kMaxBufferedPayloads)
        {
            bufferedPayloads_.pop_front();
        }
    }

    CleanupFinishedRequests();

    auto future = std::async(std::launch::async, [this, body]() {
        std::string error;
        bool success = dataStore_->AppendPayloadsWithVerification({body}, error);

        std::lock_guard<std::mutex> lock(requestMutex_);
        lastResponseMessage_.clear();
        if (success)
        {
            lastResponseMessage_ = "Stored payload locally";
            lastErrorMessage_.clear();
            lastWriteStatus_ = "Last write ok";
            if (!bufferedPayloads_.empty() && bufferedPayloads_.front() == body)
            {
                bufferedPayloads_.pop_front();
            }
            {
                std::lock_guard<std::mutex> historyLock(historyMutex_);
                historyDirty_ = true;
            }
        }
        else
        {
            lastErrorMessage_ = error.empty() ? std::string("Failed to persist payload") : error;
            lastWriteStatus_ = lastErrorMessage_;
        }
    });

    {
        std::lock_guard<std::mutex> lock(requestMutex_);
        pendingRequests_.emplace_back(std::move(future));
    }
}

bool HsBackend::UploadMmrSnapshot(const char* contextTag, const std::string& sessionType)
{
	const char* tag = contextTag ? contextTag : "unknown";

	DiagnosticLogger::Log(std::string("UploadMmrSnapshot: snapshot uploads disabled (context ") + tag + ")");
	return false;
}

void HsBackend::FetchHistory()
{
    if (!dataStore_)
    {
        if (cvarManager_)
        {
            cvarManager_->log("HS: local data store is not configured for history fetch");
        }
        return;
    }

    {
        std::lock_guard<std::mutex> lock(historyMutex_);
        if (!historyDirty_)
        {
            historyLoading_ = false;
            historyErrorMessage_.clear();
            historyLastFetched_ = std::chrono::system_clock::now();
            return;
        }
        historyLoading_ = true;
        historyErrorMessage_.clear();
    }

    CleanupFinishedRequests();

    DiagnosticLogger::Log("FetchHistory: reading local store");

    auto future = std::async(std::launch::async, [this]() {
        HistorySnapshot parsed;
        std::string error;
        bool success = dataStore_->LoadHistory(parsed, error);

        std::lock_guard<std::mutex> lock(historyMutex_);
        historyLoading_ = false;
        if (success)
        {
            historySnapshot_ = std::move(parsed);
            historyLastFetched_ = std::chrono::system_clock::now();
            historyDirty_ = false;
        }

        if (!error.empty())
        {
            historyErrorMessage_ = error;
        }
        else if (!success)
        {
            historyErrorMessage_ = "History load failed";
        }
        else
        {
            historyErrorMessage_.clear();
        }
    });

    {
        std::lock_guard<std::mutex> lock(requestMutex_);
        pendingRequests_.emplace_back(std::move(future));
    }
}

void HsBackend::CacheLastPayload(const std::string& payload, const char* contextTag)
{
    std::lock_guard<std::mutex> lock(payloadMutex_);
    lastPayload_        = payload;
    lastPayloadContext_ = contextTag ? contextTag : "";
}

bool HsBackend::DispatchCachedPayload(const char* reason)
{
    std::string cached;
    std::string context;
    {
        std::lock_guard<std::mutex> lock(payloadMutex_);
        cached  = lastPayload_;
        context = lastPayloadContext_;
    }

    if (cached.empty())
    {
        DiagnosticLogger::Log(std::string("DispatchCachedPayload: no cached payload (reason=") +
                              (reason ? reason : "n/a") + ")");
        return false;
    }

    DiagnosticLogger::Log(std::string("DispatchCachedPayload: sending cached payload captured during ") +
                          context + ", reason=" + (reason ? reason : "n/a"));
    DispatchPayloadAsync("/api/mmr-log", cached);
    return true;
}

void HsBackend::SnapshotRequestState(std::string& lastResponse, std::string& lastError) const
{
    std::lock_guard<std::mutex> lock(requestMutex_);
    lastResponse = lastResponseMessage_;
    lastError    = lastErrorMessage_;
}

void HsBackend::SnapshotStorageDiagnostics(std::string& status, size_t& bufferedCount) const
{
    std::lock_guard<std::mutex> lock(requestMutex_);
    status = lastWriteStatus_;
    bufferedCount = bufferedPayloads_.size();
}

std::filesystem::path HsBackend::GetStorePath() const
{
    if (!dataStore_)
    {
        return std::filesystem::path();
    }
    return dataStore_->GetStorePath();
}

void HsBackend::SnapshotHistory(HistorySnapshot& snapshot,
                                 std::string& errorMessage,
                                 bool& loading,
                                 std::chrono::system_clock::time_point& lastFetched) const
{
    std::lock_guard<std::mutex> lock(historyMutex_);
    snapshot     = historySnapshot_;
    errorMessage = historyErrorMessage_;
    loading      = historyLoading_;
    lastFetched  = historyLastFetched_;
}

void HsBackend::CleanupFinishedRequests()
{
    std::lock_guard<std::mutex> lock(requestMutex_);
    pendingRequests_.erase(
        std::remove_if(
            pendingRequests_.begin(),
            pendingRequests_.end(),
            [](std::future<void>& f) {
                return f.valid() &&
                       f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
            }),
        pendingRequests_.end());
}

void HsBackend::FlushBufferedWrites()
{
    if (!dataStore_)
    {
        return;
    }
    std::deque<std::string> toFlush;
    {
        std::lock_guard<std::mutex> lock(requestMutex_);
        toFlush = bufferedPayloads_;
    }
    if (toFlush.empty())
    {
        return;
    }

    std::string error;
    if (dataStore_->AppendPayloadsWithVerification(std::vector<std::string>(toFlush.begin(), toFlush.end()), error))
    {
        std::lock_guard<std::mutex> lock(requestMutex_);
        bufferedPayloads_.clear();
        lastWriteStatus_ = "Buffered writes flushed";
        {
            std::lock_guard<std::mutex> historyLock(historyMutex_);
            historyDirty_ = true;
        }
    }
    else
    {
        std::lock_guard<std::mutex> lock(requestMutex_);
        lastWriteStatus_ = error.empty() ? "Flush failed" : error;
    }
}
