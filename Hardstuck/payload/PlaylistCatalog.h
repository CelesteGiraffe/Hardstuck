#pragma once

#include <vector>
#include <string>

#include "playlist.h"

namespace PlaylistCatalog
{
    const PlaylistInfo* FindByMmrId(int mmrId);
    const PlaylistInfo* FindByKey(const std::string& key);
    const PlaylistInfo* FindByServerPlaylistId(int serverPlaylistId);

    const PlaylistInfo* GetCasualPlaylist();
    std::vector<const PlaylistInfo*> GetCoreRankedPlaylists();
    std::vector<const PlaylistInfo*> GetRankedExtraModePlaylists();
    const PlaylistInfo* GetTournamentPlaylist();

    // Ordered: casual, core ranked (1v1/2v2/3v3/4v4), ranked extra modes,
    // tournaments. Used for manual snapshot uploads.
    std::vector<const PlaylistInfo*> GetManualSnapshotOrder();
}
