#pragma once

#include <string>
#include <vector>
#include <map>

struct HistoryFilters {
    std::string playlist;
    std::string mmrFrom;
    std::string mmrTo;
    std::string sessionStart;
    std::string sessionEnd;
};

struct HistoryStatus {
    std::string receivedAt;
    std::string generatedAt;
    int mmrEntries = 0;
    int trainingSessions = 0;
    std::string lastMmrTimestamp;
    std::string lastTrainingTimestamp;
    int mmrLimit = 0;
    int sessionLimit = 0;
    HistoryFilters filters;
};

struct MmrHistoryEntry {
    std::string id;
    std::string timestamp;
    std::string playlist;
    int mmr = 0;
    int gamesPlayedDiff = 0;
    std::string source;
};

struct TrainingHistoryEntry {
    std::string id;
    std::string startedTime;
    std::string finishedTime;
    std::string source;
    std::string presetId;
    std::string notes;
    int actualDuration = 0;
    std::vector<std::string> skillIds;
    int blocks = 0;
};

struct HistorySnapshot {
    std::vector<MmrHistoryEntry> mmrHistory;
    std::vector<TrainingHistoryEntry> trainingHistory;
    HistoryStatus status;
    struct Aggregates
    {
        std::map<std::string, double> timeBySessionType; // seconds
        struct MmrDelta
        {
            std::string timestamp;
            std::string playlist;
            std::string sessionType;
            int mmr = 0;
            int delta = 0;
        };
        std::vector<MmrDelta> mmrDeltas;
    } aggregates;
};
