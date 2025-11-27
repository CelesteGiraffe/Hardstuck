#include "pch.h"
#include "payload/HsPayloadBuilder.h"
#include "bakkesmod/wrappers/GameEvent/ServerWrapper.h"
#include "bakkesmod/wrappers/GameWrapper.h"
#include "settings/ISettingsService.h"
#include "utils/HsUtils.h"
#include "diagnostics/DiagnosticLogger.h"
#include "payload/PlaylistCatalog.h"

#include <sstream>
#include <unordered_map>
#include <cmath>



std::string HsPlaylistNameFromServer(ServerWrapper server)
{
    if (!server)
        return "Unknown";

    GameSettingPlaylistWrapper playlist = server.GetPlaylist();
    if (playlist)
    {
        const int playlistId = playlist.GetPlaylistId();
        if (const PlaylistInfo* playlistInfo = PlaylistCatalog::FindByServerPlaylistId(playlistId))
        {
            return playlistInfo->display;
        }

        std::string name;
        try { name = playlist.GetLocalizedName(); } catch (...) {}
        if (name.empty())
        {
            try { name = playlist.GetName(); } catch (...) {}
        }

        if (!name.empty())
            return name;

        static const std::unordered_map<int, std::string> playlistNames = {
            {1, "Duel"},
            {2, "Doubles"},
            {3, "Standard"},
            {4, "Chaos"},
            {6, "Solo Standard"},
            {8, "Hoops"},
            {10, "Rumble"},
            {11, "Dropshot"},
            {13, "Snow Day"},
            {34, "Tournament"}
        };

        auto it = playlistNames.find(playlistId);
        if (it != playlistNames.end())
            return it->second;
    }

    return "Unknown";
}

std::string HsSerializeTeams(ServerWrapper server)
{
    std::ostringstream oss;
    oss << '[';

    if (server)
    {
        ArrayWrapper<TeamWrapper> teams = server.GetTeams();
        bool firstTeam = true;
        for (int i = 0; i < teams.Count(); ++i)
        {
            TeamWrapper team = teams.Get(i);
            if (!team)
                continue;

            if (!firstTeam)
                oss << ',';
            firstTeam = false;

            const int teamIndex = team.GetTeamNum();
            const int score = team.GetScore();
            const std::string name = teamIndex == 1 ? "Orange" : "Blue";

            oss << '{'
                << "\"teamIndex\":" << teamIndex << ','
                << "\"name\":" << JsonEscape(name) << ','
                << "\"score\":" << score
                << '}';
        }
    }

    oss << ']';
    return oss.str();
}

std::string HsSerializeScoreboard(ServerWrapper server)
{
    std::ostringstream oss;
    oss << '[';

    if (server)
    {
        ArrayWrapper<CarWrapper> cars = server.GetCars();
        bool first = true;
        for (int i = 0; i < cars.Count(); ++i)
        {
            CarWrapper car = cars.Get(i);
            if (!car)
                continue;

            PriWrapper pri = car.GetPRI();
            if (!pri)
                continue;

            if (!first)
                oss << ',';
            first = false;

            const std::string playerName = pri.GetPlayerName().IsNull()
                                           ? std::string("Unknown")
                                           : pri.GetPlayerName().ToString();
            const int teamIndex = pri.GetTeamNum();
            const int score = pri.GetMatchScore();
            const int goals = pri.GetMatchGoals();
            const int assists = pri.GetMatchAssists();
            const int saves = pri.GetMatchSaves();
            const int shots = pri.GetMatchShots();

            oss << '{'
                << "\"name\":" << JsonEscape(playerName) << ','
                << "\"teamIndex\":" << teamIndex << ','
                << "\"score\":" << score << ','
                << "\"goals\":" << goals << ','
                << "\"assists\":" << assists << ','
                << "\"saves\":" << saves << ','
                << "\"shots\":" << shots
                << '}';
        }
    }

    oss << ']';
    return oss.str();
}

bool HsCollectMatchPayloadComponents(
    ServerWrapper server,
    ISettingsService* settingsService,
    HsMatchPayloadComponents& outComponents,
    int& outPlaylistMmrId
)
{
    const auto now = std::chrono::system_clock::now();
    outComponents.timestamp = FormatTimestamp(now);
    outComponents.playlistName = HsPlaylistNameFromServer(server);
    outComponents.gamesPlayedDiff = settingsService
        ? settingsService->GetGamesPlayedIncrement()
        : 1;
    outComponents.userId = settingsService
        ? settingsService->GetUserId()
        : std::string("unknown");
    outComponents.teamsJson = HsSerializeTeams(server);
    outComponents.scoreboardJson = HsSerializeScoreboard(server);

    int playlistId = 0;
    if (server)
    {
        GameSettingPlaylistWrapper playlist = server.GetPlaylist();
        if (playlist)
        {
            playlistId = playlist.GetPlaylistId();
        }
    }
    const PlaylistInfo* playlistInfo = PlaylistCatalog::FindByServerPlaylistId(playlistId);
    outPlaylistMmrId = playlistInfo ? playlistInfo->mmrId : playlistId;

    return true;
}

std::string HsBuildMatchPayloadFromComponents(
    const HsMatchPayloadComponents& components,
    int mmr
)
{
    std::ostringstream oss;
    oss << '{'
        << "\"timestamp\":" << JsonEscape(components.timestamp) << ','
        << "\"playlist\":" << JsonEscape(components.playlistName) << ','
        << "\"mmr\":" << mmr << ','
        << "\"gamesPlayedDiff\":" << components.gamesPlayedDiff << ','
        << "\"source\":\"bakkes\",";

    oss << "\"userId\":" << JsonEscape(components.userId) << ',';
    oss << "\"teams\":" << components.teamsJson << ',';
    oss << "\"scoreboard\":" << components.scoreboardJson;
    oss << '}';

    return oss.str();
}

static bool HsHasValidUniqueId(UniqueIDWrapper& uniqueId)
{
    bool hasUniqueId = false;
    try
    {
        hasUniqueId = (uniqueId.GetUID() != 0);
    }
    catch (...)
    {
        hasUniqueId = false;
    }

    if (!hasUniqueId)
    {
        try
        {
            hasUniqueId = !uniqueId.GetEpicAccountID().empty();
        }
        catch (...)
        {
            hasUniqueId = false;
        }
    }

    return hasUniqueId;
}

bool HsTryFetchPlaylistRating(GameWrapper* gameWrapper, UniqueIDWrapper& uniqueId, int playlistMmrId, float& outRating)
{
    outRating = 0.0f;
    if (!gameWrapper)
    {
        return false;
    }

    auto mmrWrapper = gameWrapper->GetMMRWrapper();
    if (mmrWrapper.memory_address == 0)
    {
        DiagnosticLogger::Log("HsTryFetchPlaylistRating: mmrWrapper invalid");
        return false;
    }

    if (!HsHasValidUniqueId(uniqueId))
    {
        DiagnosticLogger::Log("HsTryFetchPlaylistRating: unique id unavailable");
        return false;
    }

    try
    {
        outRating = mmrWrapper.GetPlayerMMR(uniqueId, playlistMmrId);
        return outRating > 0.0f;
    }
    catch (...)
    {
        DiagnosticLogger::Log("HsTryFetchPlaylistRating: exception querying MMR");
        outRating = 0.0f;
        return false;
    }
}

bool HsTryFetchPlaylistRating(GameWrapper* gameWrapper, int playlistMmrId, float& outRating)
{
    UniqueIDWrapper uniqueId = gameWrapper ? gameWrapper->GetUniqueID() : UniqueIDWrapper();
    return HsTryFetchPlaylistRating(gameWrapper, uniqueId, playlistMmrId, outRating);
}

static std::string HsSerializeSnapshotPayload(const std::string& timestamp,
                                               const std::string& userId,
                                               const PlaylistInfo& playlistInfo,
                                               float rating,
                                               bool hasRating)
{
    const int roundedRating = hasRating ? static_cast<int>(std::round(rating)) : 0;

    const bool useDisplayName =
        playlistInfo.mmrId == 0   || // Casual bucket should match match payloads
        playlistInfo.mmrId == 10  ||
        playlistInfo.mmrId == 11  ||
        playlistInfo.mmrId == 13;
    const std::string playlistName = useDisplayName ? playlistInfo.display : playlistInfo.key;

    std::ostringstream oss;
    oss << '{'
        << "\"timestamp\":" << JsonEscape(timestamp) << ','
        << "\"playlist\":" << JsonEscape(playlistName) << ','
        << "\"mmr\":" << roundedRating << ','
        << "\"gamesPlayedDiff\":0,"
        << "\"source\":\"bakkes_snapshot\","
        << "\"userId\":" << JsonEscape(userId) << ','
        << "\"teams\":[],"
        << "\"scoreboard\":[]"
        << '}';
    return oss.str();
}

std::string HsBuildMatchPayload(
    ServerWrapper server,
    GameWrapper* gameWrapper,
    ISettingsService* settingsService
)
{
    HsMatchPayloadComponents components;
    int playlistMmrId = 0;
    HsCollectMatchPayloadComponents(server, settingsService, components, playlistMmrId);

    float mmr = 0.0f;
    const bool hasRating = HsTryFetchPlaylistRating(gameWrapper, playlistMmrId, mmr);

    return HsBuildMatchPayloadFromComponents(
        components,
        hasRating ? static_cast<int>(std::round(mmr)) : 0
    );
}

std::vector<std::string> HsBuildMmrSnapshotPayloads(
    GameWrapper* gameWrapper,
    ISettingsService* settingsService
)
{
    std::vector<std::string> payloads;

    if (!gameWrapper)
    {
        DiagnosticLogger::Log("BuildMmrSnapshotPayloads: gameWrapper unavailable");
        return payloads;
    }

    const auto now = std::chrono::system_clock::now();
    const std::string timestamp = FormatTimestamp(now);
    const std::string userId = settingsService
        ? settingsService->GetUserId()
        : std::string("unknown");
    const auto snapshotTargets = PlaylistCatalog::GetManualSnapshotOrder();
    for (const PlaylistInfo* playlistInfo : snapshotTargets)
    {
        if (!playlistInfo)
            continue;

        const int playlistId = playlistInfo->mmrId;
        float rating = 0.0f;
        const bool hasRating = HsTryFetchPlaylistRating(gameWrapper, playlistId, rating);

        if (!hasRating)
        {
            DiagnosticLogger::Log(
                std::string("BuildMmrSnapshotPayloads: no rating available for playlist ")
                + playlistInfo->display + " (mmr id " + std::to_string(playlistId) + ")"
            );
        }
        else
        {
            DiagnosticLogger::Log(
                std::string("BuildMmrSnapshotPayloads: playlist ") + playlistInfo->display
                + " id " + std::to_string(playlistId)
                + " rating " + std::to_string(static_cast<int>(std::round(rating)))
            );
        }

        const int roundedRating = hasRating ? static_cast<int>(std::round(rating)) : 0;

        payloads.emplace_back(
            HsSerializeSnapshotPayload(timestamp, userId, *playlistInfo, rating, hasRating)
        );
    }

    if (payloads.empty())
    {
        DiagnosticLogger::Log("BuildMmrSnapshotPayloads: no playlists produced valid ratings");
    }

    return payloads;
}

std::string HsBuildSinglePlaylistSnapshotPayload(
    GameWrapper* gameWrapper,
    ISettingsService* settingsService,
    const PlaylistInfo& playlistInfo
)
{
    if (!gameWrapper)
    {
        DiagnosticLogger::Log("BuildSinglePlaylistSnapshotPayload: gameWrapper unavailable");
        return std::string();
    }

    float rating = 0.0f;
    const bool hasRating = HsTryFetchPlaylistRating(gameWrapper, playlistInfo.mmrId, rating);

    if (!hasRating)
    {
        DiagnosticLogger::Log(
            std::string("BuildSinglePlaylistSnapshotPayload: no rating for playlist ")
            + playlistInfo.display + " (mmr id " + std::to_string(playlistInfo.mmrId) + ")"
        );
    }
    else
    {
        DiagnosticLogger::Log(
            std::string("BuildSinglePlaylistSnapshotPayload: playlist ") + playlistInfo.display
            + " id " + std::to_string(playlistInfo.mmrId)
            + " rating " + std::to_string(static_cast<int>(std::round(rating)))
        );
    }

    const auto now = std::chrono::system_clock::now();
    const std::string timestamp = FormatTimestamp(now);
    const std::string userId = settingsService
        ? settingsService->GetUserId()
        : std::string("unknown");

    return HsSerializeSnapshotPayload(timestamp, userId, playlistInfo, rating, hasRating);
}
