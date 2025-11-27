#pragma once

#include <cstddef>

// All the playlists you care about, in the ID space used by GetPlayerMMR.
// This is *not* the same as the menu enum or ServerWrapper playlist id.
struct PlaylistInfo
{
    int   mmrId;          // ID for MMRWrapper::GetPlayerMMR
    const char* key;      // stable internal key
    const char* display;  // human-readable label
    bool  isRanked;       // true = ranked playlist, false = casual bucket
    bool  isExtraMode;    // true = Rumble/Hoops/Dropshot/Snow Day
};

// Note: casual MMR is a single bucket (id 0) even though there are multiple
// casual queues (1v1, 2v2, 3v3, 4v4). The game does not expose per-casual-mode MMR.
static const PlaylistInfo kPlaylists[] = {
    // ---- Casual (single MMR bucket) ----
    { 0,  "casual_all",           "Casual",             false, false },

    // ---- Core ranked modes ----
    { 10, "ranked_duel_1v1",      "Ranked Duel 1v1",    true,  false }, // RankedSoloDuel
    { 11, "ranked_doubles_2v2",   "Ranked Doubles 2v2", true,  false }, // RankedTeamDoubles
    { 13, "ranked_standard_3v3",  "Ranked Standard 3v3",true,  false }, // RankedStandard 3v3
    { 61, "ranked_quads_4v4",     "Ranked 4v4 Quads",   true,  false }, // Ranked 4v4 Quads

    // ---- Ranked extra modes ----
    { 27, "ranked_hoops_2v2",     "Ranked Hoops 2v2",   true,  true  }, // Hoops
    { 28, "ranked_rumble_3v3",    "Ranked Rumble 3v3",  true,  true  }, // Rumble
    { 29, "ranked_dropshot_3v3",  "Ranked Dropshot 3v3",true,  true  }, // Dropshot
    { 30, "ranked_snowday_3v3",   "Ranked Snow Day 3v3",true,  true  }, // Snow Day

    // ---- Tournaments (ranked 3v3 tourney MMR, optional) ----
    { 34, "ranked_tournament_3v3","Ranked Tournament 3v3", true, false }, // Comp tourneys
};

static const size_t kPlaylistCount = sizeof(kPlaylists) / sizeof(kPlaylists[0]);
