#pragma once

#include <filesystem>
#include <string>

namespace settings
{
    constexpr char kBaseUrlCvarName[] = "hs_api_base_url";
    constexpr char kForceLocalhostCvarName[] = "hs_force_localhost";
    constexpr char kUserIdCvarName[] = "hs_user_id";
    constexpr char kGamesPlayedCvarName[] = "hs_games_played_increment";
    constexpr char kUiEnabledCvarName[] = "hs_ui_enabled";
    constexpr char kPostMatchDelayCvarName[] = "hs_post_match_mmr_delay";
    constexpr char kLocalhostBaseUrl[] = "http://localhost:4000";
    constexpr char kDefaultBaseUrl[] = "http://localhost:4000";
    constexpr char kLanExampleBaseUrl[] = "http://192.168.1.x:4000";
}

class ISettingsService
{
public:
    virtual ~ISettingsService() = default;

    virtual void RegisterCVars() = 0;
    virtual void LoadPersistedSettings() = 0;
    virtual void SavePersistedSettings() = 0;
    virtual std::filesystem::path GetSettingsPath() const = 0;
    virtual std::string ApplyBaseUrl(const std::string& newUrl) = 0;
    virtual std::string GetBaseUrl() const = 0;
    virtual void SetBaseUrl(const std::string& newUrl) = 0;
    virtual std::string GetUserId() const = 0;
    virtual void SetUserId(const std::string& userId) = 0;
    virtual int GetGamesPlayedIncrement() const = 0;
    virtual float GetPostMatchMmrDelaySeconds() const = 0;
    virtual bool ForceLocalhostEnabled() const = 0;
    virtual void SetForceLocalhost(bool enabled) = 0;
};
