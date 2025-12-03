// UserIdResolver.cpp
#include "pch.h"
#include "src/user/UserIdResolver.h"

#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>

#include "bakkesmod/wrappers/GameWrapper.h"
#include "bakkesmod/wrappers/UniqueIDWrapper.h"
#include "settings/SettingsService.h"
#include "diagnostics/DiagnosticLogger.h"

namespace
{
    std::string SanitizeId(const std::string& raw)
    {
        std::string safe;
        safe.reserve(raw.size());
        for (char c : raw)
        {
            if ((c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') ||
                c == '-')
            {
                safe.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            }
            else if (c == '_' || c == ':')
            {
                safe.push_back('-');
            }
        }
        if (safe.size() > 64)
        {
            safe.resize(64);
        }
        if (safe.empty())
        {
            safe = "anon";
        }
        return safe;
    }

    std::string HashInstallId(const std::string& installId)
    {
        std::hash<std::string> hasher;
        const auto value = hasher(installId);
        std::ostringstream oss;
        oss << std::hex << std::setw(12) << std::setfill('0') << (value & 0xFFFFFFFFFFFFULL);
        return oss.str();
    }

    std::string ResolvePlatformId(GameWrapper* gameWrapper)
    {
        if (!gameWrapper)
        {
            return std::string();
        }

        try
        {
            UniqueIDWrapper uid = gameWrapper->GetUniqueID();
            try
            {
                const std::string epic = uid.GetEpicAccountID();
                if (!epic.empty())
                {
                    return epic;
                }
            }
            catch (...) {}

            try
            {
                const uint64_t raw = uid.GetUID();
                if (raw != 0)
                {
                    return std::to_string(raw);
                }
            }
            catch (...) {}
        }
        catch (...) {}

        return std::string();
    }
}

std::string UserIdResolver::ResolveUserIdFromStrings(const std::string& platformId, const std::string& installId)
{
    if (!platformId.empty())
    {
        return SanitizeId(platformId);
    }
    if (!installId.empty())
    {
        return SanitizeId(HashInstallId(installId));
    }

    // Last resort: random ephemeral id
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist;
    uint64_t v = dist(gen);
    std::ostringstream oss;
    oss << std::hex << std::setw(12) << std::setfill('0') << (v & 0xFFFFFFFFFFFFULL);
    return SanitizeId(oss.str());
}

std::string UserIdResolver::ResolveUserId(GameWrapper* gameWrapper, SettingsService* settingsService)
{
    const std::string platformId = ResolvePlatformId(gameWrapper);
    std::string installId;
    if (settingsService)
    {
        installId = settingsService->GetInstallId();
    }
    const std::string resolved = ResolveUserIdFromStrings(platformId, installId);
    DiagnosticLogger::Log(std::string("Resolved user id: ") + resolved);
    return resolved;
}
