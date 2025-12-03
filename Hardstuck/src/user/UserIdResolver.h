#pragma once

#include <string>

class GameWrapper;
class SettingsService;

namespace UserIdResolver
{
    // Resolve a filesystem-safe user identifier from platform id or install id.
    std::string ResolveUserId(GameWrapper* gameWrapper, SettingsService* settingsService);

    // Testable helper that accepts raw identifiers.
    std::string ResolveUserIdFromStrings(const std::string& platformId, const std::string& installId);
}
