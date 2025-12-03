// LocalDataStore.cpp
#include "pch.h"
#include "storage/LocalDataStore.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <map>
#include <sstream>
#include <system_error>

#include "diagnostics/DiagnosticLogger.h"
#include "history/HistoryJson.h"
#include "utils/HsUtils.h"

namespace
{
    std::filesystem::path ResolveBaseDirectory(const std::filesystem::path& base)
    {
        if (!base.empty())
        {
            return base;
        }

        std::filesystem::path fallback = std::filesystem::temp_directory_path() / "hardstuck";
        return fallback;
    }

    bool IsJsonLineEmpty(const std::string& line)
    {
        for (char c : line)
        {
            if (!std::isspace(static_cast<unsigned char>(c)))
            {
                return false;
            }
        }
        return true;
    }

    std::string SanitizeUserId(const std::string& userId)
    {
        std::string safe;
        safe.reserve(userId.size());
        for (char c : userId)
        {
            if ((c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') ||
                c == '-')
            {
                safe.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            }
        }
        if (safe.empty())
        {
            safe = "anon";
        }
        if (safe.size() > 64)
        {
            safe.resize(64);
        }
        return safe;
    }
}

LocalDataStore::LocalDataStore(std::filesystem::path baseDirectory, std::string userId)
    : baseDirectory_(ResolveBaseDirectory(baseDirectory))
{
    const std::string sanitizedUser = SanitizeUserId(userId);
    userDirectory_ = baseDirectory_ / sanitizedUser;
    storePath_ = userDirectory_ / "local_history.jsonl";
    legacyCachePath_ = userDirectory_ / "payload_cache.jsonl";
    legacyBackupPath_ = userDirectory_ / "cached_payloads.jsonl";
}

bool LocalDataStore::AppendPayload(const std::string& payload, std::string& error)
{
    error.clear();
    if (payload.empty())
    {
        error = "Payload is empty";
        return false;
    }
    return AppendPayloads(std::vector<std::string>{payload}, error);
}

bool LocalDataStore::AppendPayloads(const std::vector<std::string>& payloads, std::string& error)
{
    error.clear();
    if (payloads.empty())
    {
        return true;
    }
    return AppendLines(payloads, error);
}

bool LocalDataStore::AppendPayloadsWithVerification(const std::vector<std::string>& payloads, std::string& error)
{
    if (!AppendPayloads(payloads, error))
    {
        return false;
    }

    // Verify last line matches last payload
    std::ifstream input(storePath_);
    if (!input.is_open())
    {
        error = std::string("Verification failed: could not reopen ") + storePath_.string();
        return false;
    }

    std::string lastLine;
    std::string line;
    while (std::getline(input, line))
    {
        if (!IsJsonLineEmpty(line))
        {
            lastLine = line;
        }
    }

    if (lastLine != payloads.back())
    {
        error = "Verification failed: payload mismatch";
        return false;
    }

    return true;
}

bool LocalDataStore::ReadPayloadLines(std::vector<std::string>& lines, std::string& error) const
{
    std::lock_guard<std::mutex> lock(fileMutex_);

    std::ifstream input(storePath_);
    if (!input.is_open())
    {
        if (std::filesystem::exists(storePath_))
        {
            error = std::string("Failed to read local store at ") + storePath_.string();
            return false;
        }
        // Gracefully handle missing store; caller treats empty list as "no history yet".
        return true;
    }

    std::string line;
    while (std::getline(input, line))
    {
        if (IsJsonLineEmpty(line))
        {
            continue;
        }
        lines.push_back(line);
    }
    return true;
}

bool LocalDataStore::ParsePayloadSummary(const std::string& payload, PayloadSummary& summary, std::string& error) const
{
    HistoryJson::Parser parser(payload);
    HistoryJson::Value root;
    if (!parser.Parse(root, error))
    {
        return false;
    }

    if (root.type != HistoryJson::Type::Object)
    {
        error = "Payload is not a JSON object";
        return false;
    }

    summary.timestamp = HistoryJson::AsString(HistoryJson::GetMember(root, "timestamp"))
                            .value_or(FormatTimestamp(std::chrono::system_clock::now()));
    summary.playlist = HistoryJson::AsString(HistoryJson::GetMember(root, "playlist"))
                           .value_or(std::string("unknown"));
    summary.mmr = HistoryJson::AsInt(HistoryJson::GetMember(root, "mmr")).value_or(0);
    summary.gamesPlayedDiff = HistoryJson::AsInt(HistoryJson::GetMember(root, "gamesPlayedDiff")).value_or(0);
    summary.source = HistoryJson::AsString(HistoryJson::GetMember(root, "source"))
                         .value_or(std::string("local_cache"));
    summary.sessionType = HistoryJson::AsString(HistoryJson::GetMember(root, "sessionType"))
                              .value_or(std::string());
    summary.durationSeconds = HistoryJson::AsInt(HistoryJson::GetMember(root, "durationSeconds")).value_or(0);
    return true;
}

bool LocalDataStore::BuildSnapshot(const std::vector<PayloadSummary>& entries,
                                   HistorySnapshot& snapshot,
                                   std::string& error) const
{
    snapshot = HistorySnapshot();

    if (entries.empty())
    {
        const auto now = std::chrono::system_clock::now();
        snapshot.status.generatedAt = FormatTimestamp(now);
        snapshot.status.receivedAt = snapshot.status.generatedAt;
        return true;
    }

    std::vector<PayloadSummary> sorted = entries;
    std::sort(sorted.begin(), sorted.end(), [](const PayloadSummary& lhs, const PayloadSummary& rhs) {
        if (lhs.timestamp == rhs.timestamp)
        {
            return lhs.playlist < rhs.playlist;
        }
        return lhs.timestamp < rhs.timestamp;
    });

    int ordinal = 0;
    std::map<std::string, int> lastMmrByPlaylist;
    for (const auto& entry : sorted)
    {
        MmrHistoryEntry mmrEntry;
        mmrEntry.id = std::string("local_") + std::to_string(ordinal++);
        mmrEntry.timestamp = entry.timestamp;
        mmrEntry.playlist = entry.playlist;
        mmrEntry.mmr = entry.mmr;
        mmrEntry.gamesPlayedDiff = entry.gamesPlayedDiff;
        mmrEntry.source = entry.source.empty() ? std::string("local") : entry.source;
        snapshot.mmrHistory.emplace_back(std::move(mmrEntry));

        const std::string sessionKey = entry.sessionType.empty() ? std::string("unknown") : entry.sessionType;
        snapshot.aggregates.timeBySessionType[sessionKey] +=
            static_cast<double>(std::max(0, entry.durationSeconds));

        auto it = lastMmrByPlaylist.find(entry.playlist);
        int delta = 0;
        if (it != lastMmrByPlaylist.end())
        {
            delta = entry.mmr - it->second;
        }
        lastMmrByPlaylist[entry.playlist] = entry.mmr;

        HistorySnapshot::Aggregates::MmrDelta deltaEntry;
        deltaEntry.timestamp = entry.timestamp;
        deltaEntry.playlist = entry.playlist;
        deltaEntry.sessionType = entry.sessionType.empty() ? std::string("unknown") : entry.sessionType;
        deltaEntry.mmr = entry.mmr;
        deltaEntry.delta = delta;
        snapshot.aggregates.mmrDeltas.emplace_back(std::move(deltaEntry));
    }

    snapshot.status.mmrEntries = static_cast<int>(snapshot.mmrHistory.size());
    snapshot.status.trainingSessions = static_cast<int>(snapshot.trainingHistory.size());
    snapshot.status.mmrLimit = snapshot.status.mmrEntries;
    snapshot.status.sessionLimit = snapshot.status.trainingSessions;
    snapshot.status.lastMmrTimestamp = snapshot.mmrHistory.empty()
                                            ? std::string()
                                            : snapshot.mmrHistory.back().timestamp;
    snapshot.status.lastTrainingTimestamp = snapshot.trainingHistory.empty()
                                                ? std::string()
                                                : snapshot.trainingHistory.back().finishedTime;
    snapshot.status.receivedAt = FormatTimestamp(std::chrono::system_clock::now());
    snapshot.status.generatedAt = snapshot.status.lastMmrTimestamp.empty()
                                      ? snapshot.status.receivedAt
                                      : snapshot.status.lastMmrTimestamp;

    (void)error;
    return true;
}

bool LocalDataStore::LoadHistory(HistorySnapshot& snapshot, std::string& error) const
{
    error.clear();
    std::vector<std::string> payloadLines;
    if (!ReadPayloadLines(payloadLines, error))
    {
        return false;
    }

    std::vector<PayloadSummary> parsed;
    parsed.reserve(payloadLines.size());

    std::string firstParseError;
    size_t skipped = 0;
    for (size_t i = 0; i < payloadLines.size(); ++i)
    {
        const std::string& line = payloadLines[i];
        if (line.empty())
        {
            continue;
        }

        PayloadSummary summary;
        std::string parseError;
        if (!ParsePayloadSummary(line, summary, parseError))
        {
            ++skipped;
            if (firstParseError.empty())
            {
                firstParseError = parseError;
            }
            DiagnosticLogger::Log(
                std::string("LocalDataStore::LoadHistory: skipping payload line ")
                + std::to_string(i + 1) + ": " + parseError);
            continue;
        }

        parsed.emplace_back(std::move(summary));
    }

    if (!BuildSnapshot(parsed, snapshot, error))
    {
        return false;
    }

    if (skipped > 0 && error.empty())
    {
        std::ostringstream oss;
        oss << "Skipped " << skipped << " invalid record(s)";
        if (!firstParseError.empty())
        {
            oss << " (" << firstParseError << ")";
        }
        error = oss.str();
    }

    return true;
}

bool LocalDataStore::ReplayLegacyCache(std::string& error)
{
    error.clear();
    // Legacy upload retry cache locations (pre-LocalDataStore).
    std::vector<std::filesystem::path> candidates = {
        legacyCachePath_,
        legacyBackupPath_
    };

    std::vector<std::string> cachedPayloads;
    for (const auto& path : candidates)
    {
        std::ifstream input(path);
        if (!input.is_open())
        {
            continue;
        }

        std::string line;
        while (std::getline(input, line))
        {
            if (IsJsonLineEmpty(line))
            {
                continue;
            }
            cachedPayloads.push_back(line);
        }

        std::error_code ec;
        std::filesystem::remove(path, ec);
    }

    if (cachedPayloads.empty())
    {
        return true;
    }

    DiagnosticLogger::Log(
        std::string("LocalDataStore::ReplayLegacyCache: migrating ")
        + std::to_string(cachedPayloads.size()) + " cached payload(s)");

    const std::string note = std::string("Migrated ")
        + std::to_string(cachedPayloads.size()) + " cached payload(s)";
    if (!AppendPayloads(cachedPayloads, error))
    {
        return false;
    }

    error = note;
    return true;
}

bool LocalDataStore::AppendLines(const std::vector<std::string>& payloads, std::string& error)
{
    error.clear();
    std::lock_guard<std::mutex> lock(fileMutex_);

    std::error_code ec;
    std::filesystem::create_directories(storePath_.parent_path(), ec);

    if (!RotateIfNeeded(error))
    {
        return false;
    }

    std::ofstream output(storePath_, std::ios::out | std::ios::app);
    if (!output.is_open())
    {
        error = std::string("Failed to open local store at ") + storePath_.string();
        return false;
    }

    for (const auto& payload : payloads)
    {
        output << payload << "\n";
    }
    return true;
}

void LocalDataStore::SetLimits(uint64_t maxBytes, int maxFiles)
{
    maxBytes_ = maxBytes;
    maxFiles_ = std::max(1, maxFiles);
}

bool LocalDataStore::RotateIfNeeded(std::string& error)
{
    if (maxBytes_ == 0 || maxFiles_ <= 0)
    {
        return true;
    }

    std::error_code ec;
    const uint64_t size = std::filesystem::exists(storePath_, ec)
        ? static_cast<uint64_t>(std::filesystem::file_size(storePath_, ec))
        : 0;
    if (ec)
    {
        error = std::string("Failed to inspect local store: ") + ec.message();
        return false;
    }
    if (size < maxBytes_)
    {
        return true;
    }

    // simple rotation: store -> .1 -> .2 ...
    const int maxRotation = std::max(1, maxFiles_ - 1);
    for (int i = maxRotation; i >= 1; --i)
    {
        const std::filesystem::path older = std::filesystem::path(storePath_.string() + "." + std::to_string(i));
        const std::filesystem::path newer = std::filesystem::path(storePath_.string() + "." + std::to_string(i + 1));
        std::filesystem::remove(newer, ec);
        ec.clear();
        if (std::filesystem::exists(older, ec))
        {
            ec.clear();
            std::filesystem::rename(older, newer, ec);
        }
    }

    const std::filesystem::path first = std::filesystem::path(storePath_.string() + ".1");
    std::filesystem::remove(first, ec);
    ec.clear();
    std::filesystem::rename(storePath_, first, ec);
    if (ec)
    {
        error = std::string("Failed to rotate local store: ") + ec.message();
        return false;
    }
    return true;
}
