// HsBackend.cpp
#include "pch.h"
#include "backend/HsBackend.h"

#include <algorithm>
#include <sstream>

#include "diagnostics/DiagnosticLogger.h"
#include "history/HistoryJson.h"
#include "utils/HsUtils.h"
#include "settings/SettingsService.h"

#include "bakkesmod/wrappers/GameWrapper.h"
#include "bakkesmod/wrappers/MMRWrapper.h"
#include "bakkesmod/wrappers/UniqueIDWrapper.h"
#include "bakkesmod/wrappers/cvarmanagerwrapper.h"

HsBackend::HsBackend(std::unique_ptr<ApiClient> apiClient,
                     CVarManagerWrapper* cvarManager,
                     GameWrapper* gameWrapper,
                     SettingsService* settingsService)
    : cvarManager_(cvarManager)
    , gameWrapper_(gameWrapper)
    , settingsService_(settingsService)
    , apiClient_(std::move(apiClient))
{
}

void HsBackend::DispatchPayloadAsync(const std::string& endpoint, const std::string& body)
{
    if (!apiClient_)
    {
        if (cvarManager_)
        {
            cvarManager_->log("HS: API client is not configured");
        }
        return;
    }

    DiagnosticLogger::Log(std::string("DispatchPayloadAsync: endpoint=") + endpoint +
                          ", body_len=" + std::to_string(body.size()));

    CleanupFinishedRequests();

    const std::string userId = settingsService_ ? settingsService_->GetUserId() : std::string();
    std::vector<HttpHeader> headers;
    headers.emplace_back("X-User-Id", userId);
    headers.emplace_back("User-Agent", "HardstuckPlugin/1.0");

    auto future = std::async(std::launch::async, [this, endpoint, body, headers]() {
        std::string response;
        std::string error;
        bool success = apiClient_->PostJson(endpoint, body, headers, response, error);

        std::lock_guard<std::mutex> lock(requestMutex_);
        if (success)
        {
            lastResponseMessage_ = response.empty() ? "HTTP 2xx" : response;
            lastErrorMessage_.clear();
        }
        else
        {
            lastErrorMessage_ = error.empty() ? (response.empty() ? "Unknown error" : response) : error;
        }
    });

    {
        std::lock_guard<std::mutex> lock(requestMutex_);
        pendingRequests_.emplace_back(std::move(future));
    }
}

bool HsBackend::UploadMmrSnapshot(const char* contextTag)
{
    const char* tag = contextTag ? contextTag : "unknown";

    if (!gameWrapper_)
    {
        DiagnosticLogger::Log(std::string("UploadMmrSnapshot: gameWrapper unavailable for context ") + tag);
        return false;
    }

    auto payloads = HsBuildMmrSnapshotPayloads(gameWrapper_, settingsService_);
    if (payloads.empty())
    {
        DiagnosticLogger::Log(std::string("UploadMmrSnapshot: no snapshot payloads produced for context ") + tag);
        return false;
    }

    for (const auto& p : payloads)
    {
        DispatchPayloadAsync("/api/mmr-log", p);
    }

    DiagnosticLogger::Log(std::string("UploadMmrSnapshot: dispatched ") +
                          std::to_string(payloads.size()) + " snapshot payload(s) for context " + tag);
    return true;
}

void HsBackend::FetchHistory()
{
    if (!apiClient_)
    {
        if (cvarManager_)
        {
            cvarManager_->log("HS: API client is not configured for history fetch");
        }
        return;
    }

    CleanupFinishedRequests();

    const std::string userId = settingsService_ ? settingsService_->GetUserId() : std::string();
    std::vector<HttpHeader> headers;
    headers.emplace_back("X-User-Id", userId);
    headers.emplace_back("User-Agent", "HardstuckPlugin/1.0");

    {
        std::lock_guard<std::mutex> lock(historyMutex_);
        historyLoading_ = true;
        historyErrorMessage_.clear();
    }

    const std::string endpoint = "/api/bakkesmod/history";
    DiagnosticLogger::Log(std::string("FetchHistory: requesting ") + endpoint);

    auto future = std::async(std::launch::async, [this, endpoint, headers]() {
        std::string response;
        std::string error;
        bool success = apiClient_->GetJson(endpoint, headers, response, error);

        HistorySnapshot parsed;
        std::string parseError;
        if (success)
        {
            if (!::HistoryJson::ParseHistoryResponse(response, parsed, parseError))
            {
                success = false;
                error = parseError.empty() ? std::string("Failed to parse history response") : parseError;
            }
        }

        std::lock_guard<std::mutex> lock(historyMutex_);
        historyLoading_ = false;
        if (success)
        {
            historySnapshot_ = std::move(parsed);
            historyLastFetched_ = std::chrono::system_clock::now();
            historyErrorMessage_.clear();
        }
        else
        {
            historyErrorMessage_ = error.empty() ? std::string("History request failed") : error;
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
