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
    std::string ApplyBaseUrl(const std::string& newUrl) override;
    std::string GetBaseUrl() const override;
    void SetBaseUrl(const std::string& newUrl) override;
    std::string GetUserId() const override;
    void SetUserId(const std::string& userId) override;
    int GetGamesPlayedIncrement() const override;
    float GetPostMatchMmrDelaySeconds() const override;
    bool ForceLocalhostEnabled() const override;
    void SetForceLocalhost(bool enabled) override;

private:
    std::string NormalizeBaseUrl(const std::string& candidate) const;
    void UpdateForceCvar() const;

    std::shared_ptr<CVarManagerWrapper> cvarManager_;
    bool forceLocalhost_ = true;
    std::string baseUrlCache_;
    std::string manualBaseUrl_;
};
