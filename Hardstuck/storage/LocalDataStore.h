#pragma once

#include <filesystem>
#include <mutex>
#include <cstdint>
#include <string>
#include <vector>

#include "history/HistoryTypes.h"

// Append-only local persistence for match/MMR snapshots.
class LocalDataStore
{
public:
    explicit LocalDataStore(std::filesystem::path baseDirectory, std::string userId);

    // Append one or more payloads to disk (JSONL).
    bool AppendPayload(const std::string& payload, std::string& error);
    bool AppendPayloads(const std::vector<std::string>& payloads, std::string& error);

    // Build a HistorySnapshot from persisted payloads.
    bool LoadHistory(HistorySnapshot& snapshot, std::string& error) const;

    // Append and verify last line persisted.
    bool AppendPayloadsWithVerification(const std::vector<std::string>& payloads, std::string& error);

    // Import cached payloads from older queue files, if any.
    bool ReplayLegacyCache(std::string& error);

    void SetLimits(uint64_t maxBytes, int maxFiles);

    std::filesystem::path GetStorePath() const { return storePath_; }

private:
    struct PayloadSummary
    {
        std::string timestamp;
        std::string playlist;
        int mmr{0};
        int gamesPlayedDiff{0};
        std::string source;
        std::string sessionType;
        int durationSeconds{0};
    };

    bool ParsePayloadSummary(const std::string& payload, PayloadSummary& summary, std::string& error) const;
    bool BuildSnapshot(const std::vector<PayloadSummary>& entries, HistorySnapshot& snapshot, std::string& error) const;
    bool ReadPayloadLines(std::vector<std::string>& lines, std::string& error) const;
    bool AppendLines(const std::vector<std::string>& payloads, std::string& error);
    bool RotateIfNeeded(std::string& error);

    std::filesystem::path baseDirectory_;
    std::filesystem::path userDirectory_;
    std::filesystem::path storePath_;
    std::filesystem::path legacyCachePath_;
    std::filesystem::path legacyBackupPath_;
    mutable std::mutex fileMutex_;
    uint64_t maxBytes_{0};
    int maxFiles_{1};
};
