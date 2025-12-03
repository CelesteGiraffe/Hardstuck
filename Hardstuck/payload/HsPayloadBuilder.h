#pragma once
// Minimal includes; implementation file pulls in heavy wrapper headers.
#include <string>
#include <vector>

class ServerWrapper;
class GameWrapper;
class ISettingsService;
class UniqueIDWrapper;

struct HsMatchPayloadComponents
{
    std::string timestamp;
    std::string playlistName;
    int gamesPlayedDiff = 1;
    std::string userId;
    std::string sessionType;
    std::string teamsJson;
    std::string scoreboardJson;
};

// Playlist name + JSON payloads
std::string HsPlaylistNameFromServer(ServerWrapper server);
std::string HsSerializeTeams(ServerWrapper server);
std::string HsSerializeScoreboard(ServerWrapper server);
bool HsCollectMatchPayloadComponents(
    ServerWrapper server,
    ISettingsService* settingsService,
    HsMatchPayloadComponents& outComponents,
    int& outPlaylistMmrId
);
std::string HsBuildMatchPayloadFromComponents(
    const HsMatchPayloadComponents& components,
    int mmr
);
bool HsTryFetchPlaylistRating(GameWrapper* gameWrapper, int playlistMmrId, float& outRating);
bool HsTryFetchPlaylistRating(GameWrapper* gameWrapper, UniqueIDWrapper& uniqueId, int playlistMmrId, float& outRating);

// Full match payload (for a finished match / replay)
std::string HsBuildMatchPayload(ServerWrapper server, GameWrapper* gameWrapper, ISettingsService* settingsService, const std::string& sessionType);

// Snapshot payloads (for "current MMR for all queues")
std::vector<std::string> HsBuildMmrSnapshotPayloads(GameWrapper* gameWrapper, ISettingsService* settingsService, const std::string& sessionType);
