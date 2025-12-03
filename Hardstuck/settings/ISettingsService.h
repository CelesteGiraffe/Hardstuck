#pragma once

#include <filesystem>
#include <cstdint>
#include <string>

namespace settings
{
    constexpr char kDataDirCvarName[] = "hs_data_dir";
    constexpr char kStoreMaxBytesCvarName[] = "hs_store_max_bytes";
    constexpr char kStoreMaxFilesCvarName[] = "hs_store_max_files";
    constexpr char kKeyFocusFreeplayCvarName[] = "hs_key_focus_freeplay";
    constexpr char kKeyTrainingPackCvarName[] = "hs_key_training_pack";
    constexpr char kKeyManualSessionCvarName[] = "hs_key_manual_session";
    constexpr char kGamesPlayedCvarName[] = "hs_games_played_increment";
    constexpr char kUiEnabledCvarName[] = "hs_ui_enabled";
    constexpr char kPostMatchDelayCvarName[] = "hs_post_match_mmr_delay";
}

class ISettingsService
{
public:
    virtual ~ISettingsService() = default;

    virtual void RegisterCVars() = 0;
    virtual void LoadPersistedSettings() = 0;
    virtual void SavePersistedSettings() = 0;
    virtual std::filesystem::path GetSettingsPath() const = 0;
    virtual std::filesystem::path GetDataDirectory() const = 0;
    virtual void SetDataDirectory(const std::filesystem::path& dir) = 0;
    virtual std::string GetInstallId() = 0;
    virtual uint64_t GetMaxStoreBytes() const = 0;
    virtual void SetMaxStoreBytes(uint64_t bytes) = 0;
    virtual int GetMaxStoreFiles() const = 0;
    virtual void SetMaxStoreFiles(int files) = 0;
    virtual std::string GetFocusFreeplayKey() const = 0;
    virtual void SetFocusFreeplayKey(const std::string& key) = 0;
    virtual std::string GetTrainingPackKey() const = 0;
    virtual void SetTrainingPackKey(const std::string& key) = 0;
    virtual std::string GetManualSessionKey() const = 0;
    virtual void SetManualSessionKey(const std::string& key) = 0;
    virtual int GetGamesPlayedIncrement() const = 0;
    virtual float GetPostMatchMmrDelaySeconds() const = 0;
};
