#include "pch.h"

#include "payload/PlaylistCatalog.h"

#include <cstring>
#include <vector>

namespace
{
    struct ServerPlaylistMapping
    {
        int serverPlaylistId;
        int mmrId;
    };

    // Maps the playlist IDs reported by ServerWrapper to the MMR buckets that
    // GetPlayerMMR understands. Casual queues (1/2/3/4) all point at the single
    // casual bucket (0).
    static const ServerPlaylistMapping kServerPlaylistMappings[] = {
        {0, 0},  // fallback when Rocket League reports playlist 0
        {1, 0},  // Casual Duel
        {2, 0},  // Casual Doubles
        {3, 0},  // Casual Standard
        {4, 0},  // Casual Chaos / 4v4
        {10, 10},
        {11, 11},
        {13, 13},
        {27, 27},
        {28, 28},
        {29, 29},
        {30, 30},
        {34, 34},
        {61, 61},
    };

    template <typename Predicate>
    const PlaylistInfo* FindFirstPlaylist(Predicate predicate)
    {
        for (size_t i = 0; i < kPlaylistCount; ++i)
        {
            const PlaylistInfo& info = kPlaylists[i];
            if (predicate(info))
            {
                return &info;
            }
        }
        return nullptr;
    }

    template <typename Predicate>
    std::vector<const PlaylistInfo*> CollectPlaylists(Predicate predicate)
    {
        std::vector<const PlaylistInfo*> result;
        for (size_t i = 0; i < kPlaylistCount; ++i)
        {
            const PlaylistInfo& info = kPlaylists[i];
            if (predicate(info))
            {
                result.push_back(&info);
            }
        }
        return result;
    }

    bool IsTournament(const PlaylistInfo& info)
    {
        return std::strcmp(info.key, "ranked_tournament_3v3") == 0;
    }
}

namespace PlaylistCatalog
{
    const PlaylistInfo* FindByMmrId(int mmrId)
    {
        return FindFirstPlaylist([mmrId](const PlaylistInfo& info) {
            return info.mmrId == mmrId;
        });
    }

    const PlaylistInfo* FindByKey(const std::string& key)
    {
        return FindFirstPlaylist([&key](const PlaylistInfo& info) {
            return key == info.key;
        });
    }

    const PlaylistInfo* FindByServerPlaylistId(int serverPlaylistId)
    {
        for (const auto& mapping : kServerPlaylistMappings)
        {
            if (mapping.serverPlaylistId == serverPlaylistId)
            {
                return FindByMmrId(mapping.mmrId);
            }
        }

        // Some ranked playlists report their MMR bucket directly; fall back to
        // treating the server ID as an MMR ID if we have a definition.
        return FindByMmrId(serverPlaylistId);
    }

    const PlaylistInfo* GetCasualPlaylist()
    {
        return FindFirstPlaylist([](const PlaylistInfo& info) {
            return !info.isRanked;
        });
    }

    std::vector<const PlaylistInfo*> GetCoreRankedPlaylists()
    {
        return CollectPlaylists([](const PlaylistInfo& info) {
            return info.isRanked && !info.isExtraMode && !IsTournament(info);
        });
    }

    std::vector<const PlaylistInfo*> GetRankedExtraModePlaylists()
    {
        return CollectPlaylists([](const PlaylistInfo& info) {
            return info.isRanked && info.isExtraMode;
        });
    }

    const PlaylistInfo* GetTournamentPlaylist()
    {
        return FindFirstPlaylist([](const PlaylistInfo& info) {
            return IsTournament(info);
        });
    }

    std::vector<const PlaylistInfo*> GetManualSnapshotOrder()
    {
        std::vector<const PlaylistInfo*> ordered;
        ordered.reserve(kPlaylistCount);

        if (const PlaylistInfo* casual = GetCasualPlaylist())
        {
            ordered.push_back(casual);
        }

        auto coreRanked = GetCoreRankedPlaylists();
        ordered.insert(ordered.end(), coreRanked.begin(), coreRanked.end());

        auto extraRanked = GetRankedExtraModePlaylists();
        ordered.insert(ordered.end(), extraRanked.begin(), extraRanked.end());

        if (const PlaylistInfo* tournament = GetTournamentPlaylist())
        {
            ordered.push_back(tournament);
        }

        return ordered;
    }
}
