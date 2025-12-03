#pragma once

#include "ISettingsService.h"

#include <memory>
#include <vector>

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
    std::vector<std::string> GetFocusList() const override;
    void SetFocusList(const std::vector<std::string>& focuses) override;
    int GetDailyGoalMinutes() const override;
    void SetDailyGoalMinutes(int minutes) override;
    int GetGamesPlayedIncrement() const override;
    float GetPostMatchMmrDelaySeconds() const override;

private:
    static std::vector<std::string> NormalizeFocusList(const std::vector<std::string>& focuses);
    static std::string SerializeFocusList(const std::vector<std::string>& focuses);
    static std::vector<std::string> DeserializeFocusList(const std::string& serialized);
    uint64_t ParseUint64Cvar(const char* name, uint64_t defaultValue) const;
    int ParseIntCvar(const char* name, int defaultValue) const;
    std::string ReadStringCvar(const char* name, const char* fallback) const;
    std::string GenerateInstallId() const;

    std::shared_ptr<CVarManagerWrapper> cvarManager_;
    std::filesystem::path dataDirectory_;
    uint64_t maxStoreBytes_ = 5 * 1024 * 1024; // 5MB default cap
    int maxStoreFiles_ = 4;
    std::vector<std::string> focusList_{"Freeplay focus", "Training pack focus"};
    int dailyGoalMinutes_{60};
    mutable std::string installId_;
};
