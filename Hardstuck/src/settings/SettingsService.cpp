#include "pch.h"
#include "settings/SettingsService.h"

#include <memory>

#include "bakkesmod/wrappers/CVarManagerWrapper.h"

#include "diagnostics/DiagnosticLogger.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <system_error>

namespace
{
    std::string Trimmed(const std::string& value)
    {
        std::string trimmed = value;
        trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), [](unsigned char ch) {
                          return !std::isspace(ch);
                      }).base(),
                      trimmed.end());
        return trimmed;
    }

    std::string NormalizeBaseUrl(const std::string& candidate)
    {
        std::string trimmed = Trimmed(candidate);
        if (trimmed.empty())
        {
            return trimmed;
        }

        std::string lowered = trimmed;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });

        if (lowered.rfind("http://", 0) == 0 || lowered.rfind("https://", 0) == 0)
        {
            return trimmed;
        }

        return std::string("http://") + trimmed;
    }

    bool ParseBoolValue(const std::string& value)
    {
        std::string lowered = value;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return (lowered == "1" || lowered == "true" || lowered == "yes");
    }
}

SettingsService::SettingsService(std::shared_ptr<CVarManagerWrapper> cvarManager)
    : cvarManager_(std::move(cvarManager))
{
}

void SettingsService::RegisterCVars()
{
    if (!cvarManager_)
    {
        DiagnosticLogger::Log("SettingsService::RegisterCVars: cvarManager unavailable");
        return;
    }

    auto baseCvar = cvarManager_->registerCvar(settings::kBaseUrlCvarName, settings::kDefaultBaseUrl, "Base URL for the Hardstuck : Rocket League Training Journal API");
    auto forceCvar = cvarManager_->registerCvar(settings::kForceLocalhostCvarName, "1", "Force uploads to http://localhost:4000");
    try
    {
        forceLocalhost_ = forceCvar.getBoolValue();
    }
    catch (...)
    {
        forceLocalhost_ = true;
    }

    cvarManager_->registerCvar(settings::kUiEnabledCvarName, "1", "Legacy UI toggle (window now follows togglemenu)");
    cvarManager_->registerCvar("hs_ui_debug_show_demo", "0", "Show ImGui demo window for debugging (1 = show)");
    cvarManager_->registerCvar(settings::kUserIdCvarName, "test-player", "User identifier sent as X-User-Id when uploading matches");
    cvarManager_->registerCvar(settings::kGamesPlayedCvarName, "1", "Increment for gamesPlayedDiff payload field");
    cvarManager_->registerCvar(settings::kPostMatchDelayCvarName, "4.0", "Seconds to wait after a match before refreshing MMR");

    const std::string explicitBase = NormalizeBaseUrl(baseCvar.getStringValue());
    manualBaseUrl_ = explicitBase;
    const std::string target = forceLocalhost_ ? settings::kLocalhostBaseUrl : explicitBase;
    ApplyBaseUrl(target);
}

void SettingsService::LoadPersistedSettings()
{
    const std::filesystem::path path = GetSettingsPath();
    std::ifstream input(path);
    if (!input.is_open())
    {
        DiagnosticLogger::Log(std::string("SettingsService::LoadPersistedSettings: missing settings file at ") + path.string());
        SavePersistedSettings();
        return;
    }

    std::string line;
    std::string fileBaseUrl;
    std::string fileUserId;
    bool hasForce = false;
    bool forcedValue = forceLocalhost_;

    while (std::getline(input, line))
    {
        const std::string trimmedLine = Trimmed(line);
        if (trimmedLine.empty() || trimmedLine[0] == '#')
        {
            continue;
        }

        const auto eqPos = trimmedLine.find('=');
        if (eqPos == std::string::npos)
        {
            continue;
        }

        const std::string key = Trimmed(trimmedLine.substr(0, eqPos));
        const std::string value = Trimmed(trimmedLine.substr(eqPos + 1));

        if (key == "base_url")
        {
            fileBaseUrl = value;
        }
        else if (key == "user_id")
        {
            fileUserId = value;
        }
        else if (key == "force_localhost")
        {
            hasForce = true;
            forcedValue = ParseBoolValue(value);
        }
    }

    if (hasForce)
    {
        forceLocalhost_ = forcedValue;
        UpdateForceCvar();
    }

    if (!fileBaseUrl.empty())
    {
        const std::string sanitized = NormalizeBaseUrl(fileBaseUrl);
        manualBaseUrl_ = sanitized;
        ApplyBaseUrl(sanitized);
    }

    if (!fileUserId.empty())
    {
        SetUserId(fileUserId);
    }
}

void SettingsService::SavePersistedSettings()
{
    const std::filesystem::path path = GetSettingsPath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream output(path, std::ios::out | std::ios::trunc);
    if (!output.is_open())
    {
        DiagnosticLogger::Log(std::string("SettingsService::SavePersistedSettings: failed to open settings file at ") + path.string());
        return;
    }

    output << "base_url=" << GetBaseUrl() << "\n";
    output << "force_localhost=" << (forceLocalhost_ ? "1" : "0") << "\n";
    output << "user_id=" << GetUserId() << "\n";
}

std::filesystem::path SettingsService::GetSettingsPath() const
{
    std::filesystem::path base;
#if defined(_WIN32)
    char* appdata_env = nullptr;
    size_t env_len = 0;
    if (_dupenv_s(&appdata_env, &env_len, "APPDATA") == 0 && appdata_env && appdata_env[0] != '\0')
    {
        base = appdata_env;
    }
    if (appdata_env)
    {
        free(appdata_env);
    }
#else
    const char* appdata_env = std::getenv("APPDATA");
    if (appdata_env && appdata_env[0] != '\0')
    {
        base = appdata_env;
    }
#endif
    if (base.empty())
    {
        base = std::filesystem::temp_directory_path();
    }

    return base / "bakkesmod" / "hardstuck" / "settings.cfg";
}

std::string SettingsService::ApplyBaseUrl(const std::string& newUrl)
{
    const std::string sanitized = NormalizeBaseUrl(newUrl);
    baseUrlCache_ = sanitized;
    if (!forceLocalhost_)
    {
        manualBaseUrl_ = sanitized;
    }

    if (cvarManager_)
    {
        try
        {
            cvarManager_->getCvar(settings::kBaseUrlCvarName).setValue(sanitized);
        }
        catch (...)
        {
            DiagnosticLogger::Log("SettingsService::ApplyBaseUrl: failed to set hs_api_base_url");
        }
    }

    return sanitized;
}

std::string SettingsService::GetBaseUrl() const
{
    if (!baseUrlCache_.empty())
    {
        return baseUrlCache_;
    }

    if (!cvarManager_)
    {
        return std::string();
    }

    try
    {
        return NormalizeBaseUrl(cvarManager_->getCvar(settings::kBaseUrlCvarName).getStringValue());
    }
    catch (...)
    {
        DiagnosticLogger::Log("SettingsService::GetBaseUrl: failed to read hs_api_base_url, returning empty string");
        return std::string();
    }
}

void SettingsService::SetBaseUrl(const std::string& newUrl)
{
    const std::string sanitized = NormalizeBaseUrl(newUrl);
    manualBaseUrl_ = sanitized;
    if (!forceLocalhost_)
    {
        ApplyBaseUrl(sanitized);
    }
}

std::string SettingsService::GetUserId() const
{
    if (!cvarManager_)
    {
        return std::string("unknown");
    }

    try
    {
        return cvarManager_->getCvar(settings::kUserIdCvarName).getStringValue();
    }
    catch (...)
    {
        DiagnosticLogger::Log("SettingsService::GetUserId: failed to read hs_user_id, returning \"unknown\"");
        return std::string("unknown");
    }
}

void SettingsService::SetUserId(const std::string& userId)
{
    if (!cvarManager_)
    {
        return;
    }

    try
    {
        cvarManager_->getCvar(settings::kUserIdCvarName).setValue(userId);
    }
    catch (...)
    {
        DiagnosticLogger::Log("SettingsService::SetUserId: failed to set hs_user_id");
    }
}

int SettingsService::GetGamesPlayedIncrement() const
{
    if (!cvarManager_)
    {
        return 1;
    }

    try
    {
        return cvarManager_->getCvar(settings::kGamesPlayedCvarName).getIntValue();
    }
    catch (...)
    {
        DiagnosticLogger::Log("SettingsService::GetGamesPlayedIncrement: failed to read hs_games_played_increment, defaulting to 1");
        return 1;
    }
}

float SettingsService::GetPostMatchMmrDelaySeconds() const
{
    if (!cvarManager_)
    {
        return 4.0f;
    }

    try
    {
        const std::string value = cvarManager_->getCvar(settings::kPostMatchDelayCvarName).getStringValue();
        return std::stof(value);
    }
    catch (...)
    {
        DiagnosticLogger::Log("SettingsService::GetPostMatchMmrDelaySeconds: failed to read hs_post_match_mmr_delay, defaulting to 4.0");
        return 4.0f;
    }
}

bool SettingsService::ForceLocalhostEnabled() const
{
    return forceLocalhost_;
}

void SettingsService::SetForceLocalhost(bool enabled)
{
    if (forceLocalhost_ == enabled)
    {
        return;
    }

    forceLocalhost_ = enabled;
    UpdateForceCvar();

    const std::string target = forceLocalhost_
        ? settings::kLocalhostBaseUrl
        : (manualBaseUrl_.empty() ? settings::kDefaultBaseUrl : manualBaseUrl_);
    ApplyBaseUrl(target);
}

std::string SettingsService::NormalizeBaseUrl(const std::string& candidate) const
{
    return ::NormalizeBaseUrl(candidate);
}

void SettingsService::UpdateForceCvar() const
{
    if (!cvarManager_)
    {
        return;
    }

    try
    {
        cvarManager_->getCvar(settings::kForceLocalhostCvarName).setValue(forceLocalhost_ ? "1" : "0");
    }
    catch (...)
    {
        DiagnosticLogger::Log("SettingsService::UpdateForceCvar: failed to set hs_force_localhost");
    }
}
