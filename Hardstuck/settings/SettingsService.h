#pragma once

#include "ISettingsService.h"

#include <memory>

class CVarManagerWrapper;

class SettingsService final : public ISettingsService
{
public:
    explicit SettingsService(std::shared_ptr<CVarManagerWrapper> cvarManager);

    void RegisterCVars() override;
    void LoadPersistedSettings() override;
    void SavePersistedSettings() override;
    std::filesystem::path GetSettingsPath() const override;
    std::filesystem::path GetDataDirectory() const override;
    void SetDataDirectory(const std::filesystem::path& dir) override;
    std::string GetInstallId() override;
    uint64_t GetMaxStoreBytes() const override;
    void SetMaxStoreBytes(uint64_t bytes) override;
    int GetMaxStoreFiles() const override;
    void SetMaxStoreFiles(int files) override;
    std::string GetFocusFreeplayKey() const override;
    void SetFocusFreeplayKey(const std::string& key) override;
    std::string GetTrainingPackKey() const override;
    void SetTrainingPackKey(const std::string& key) override;
    std::string GetManualSessionKey() const override;
    void SetManualSessionKey(const std::string& key) override;
    int GetGamesPlayedIncrement() const override;
    float GetPostMatchMmrDelaySeconds() const override;

private:
    uint64_t ParseUint64Cvar(const char* name, uint64_t defaultValue) const;
    int ParseIntCvar(const char* name, int defaultValue) const;
    std::string ReadStringCvar(const char* name, const char* fallback) const;
    std::string GenerateInstallId() const;

    std::shared_ptr<CVarManagerWrapper> cvarManager_;
    std::filesystem::path dataDirectory_;
    uint64_t maxStoreBytes_ = 5 * 1024 * 1024; // 5MB default cap
    int maxStoreFiles_ = 4;
    std::string focusFreeplayKey_ = "F7";
    std::string trainingPackKey_ = "F8";
    std::string manualSessionKey_ = "F9";
    mutable std::string installId_;
};
