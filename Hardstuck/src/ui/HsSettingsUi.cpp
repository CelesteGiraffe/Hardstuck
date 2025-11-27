#include "pch.h"
#include "ui/HsSettingsUi.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"

#include "settings/ISettingsService.h"
#include "ui/ui_style.h"

#include "IMGUI/imgui.h"

#include <string>
#include <cstring>

namespace
{
    struct SettingsUiState
    {
        char baseUrlBuf[256] = {0};
        std::string cachedBaseUrl;
    };

    SettingsUiState& GetUiState()
    {
        static SettingsUiState state;
        return state;
    }

    void SafeStrCopy(char* dest, const std::string& src, size_t destSize)
    {
        if (destSize == 0)
        {
            return;
        }

        std::strncpy(dest, src.c_str(), destSize - 1);
        dest[destSize - 1] = '\0';
    }

    void SyncBuffers(SettingsUiState& uiState, ISettingsService& settingsService)
    {
        const std::string currentBase = settingsService.GetBaseUrl();

        if (uiState.cachedBaseUrl != currentBase)
        {
            SafeStrCopy(uiState.baseUrlBuf, currentBase, sizeof(uiState.baseUrlBuf));
            uiState.cachedBaseUrl = currentBase;
        }
    }

    void RenderCompanionLocationSection(SettingsUiState& uiState, ISettingsService& settingsService, CVarManagerWrapper& cvarManager)
    {
        ImGui::TextWrapped("Where is the companion app running?");

        bool companionOnThisPc = settingsService.ForceLocalhostEnabled();
        if (ImGui::Checkbox("Companion app is on this PC (localhost)", &companionOnThisPc))
        {
            settingsService.SetForceLocalhost(companionOnThisPc);
            const std::string updatedBase = settingsService.GetBaseUrl();
            SafeStrCopy(uiState.baseUrlBuf, updatedBase, sizeof(uiState.baseUrlBuf));
            uiState.cachedBaseUrl = updatedBase;
            settingsService.SavePersistedSettings();
            cvarManager.log(std::string("HS: companion app location set to ") + (companionOnThisPc ? "localhost" : "remote IP"));
        }

        ImGui::Spacing();
        ImGui::TextWrapped("Companion app is on another IP? Enter its address below (example: http://192.168.1.x:4000). Replace 'x' with the machine running the companion app so uploads reach that host.");
        ImGui::InputText("Companion app IP / URL", uiState.baseUrlBuf, sizeof(uiState.baseUrlBuf));

        if (ImGui::Button("Save IP"))
        {
            settingsService.SetForceLocalhost(false);
            settingsService.SetBaseUrl(uiState.baseUrlBuf);
            const std::string sanitized = settingsService.GetBaseUrl();
            SafeStrCopy(uiState.baseUrlBuf, sanitized, sizeof(uiState.baseUrlBuf));
            uiState.cachedBaseUrl = sanitized;
            settingsService.SavePersistedSettings();
            cvarManager.log("HS: saved remote base URL");
        }
        ImGui::SameLine();
        if (ImGui::Button("Insert example 192.168.1.x"))
        {
            SafeStrCopy(uiState.baseUrlBuf, settings::kLanExampleBaseUrl, sizeof(uiState.baseUrlBuf));
            cvarManager.log("HS: inserted LAN example base URL");
        }

        if (settingsService.ForceLocalhostEnabled())
        {
            ImGui::TextWrapped("(Saving a remote IP automatically unchecks the localhost option.)");
        }
    }

    void RenderActions(HsTriggerManualUploadFn triggerManualUpload)
    {
        ImGui::Spacing();
        if (ImGui::Button("Gather && Upload Now", hs::ui::PrimaryButtonSize()))
        {
            if (triggerManualUpload)
            {
                triggerManualUpload();
            }
        }
        ImGui::SameLine();
        ImGui::TextWrapped("Captures the active match/replay and immediately syncs it.");
    }
}

void HsRenderSettingsUi(
    ISettingsService* settingsService,
    CVarManagerWrapper* cvarManager,
    HsTriggerManualUploadFn triggerManualUpload
)
{
    if (ImGui::GetCurrentContext() == nullptr)
    {
        return;
    }

    if (!settingsService || !cvarManager)
    {
        ImGui::TextWrapped("CVar manager unavailable; settings UI cannot function.");
        return;
    }

    [[maybe_unused]] auto styleScope = hs::ui::ApplyStyle();
    auto& uiState = GetUiState();
    SyncBuffers(uiState, *settingsService);

    ImGui::TextUnformatted("Configure where Hardstuck : Rocket League Training Journal uploads are sent.");

    RenderCompanionLocationSection(uiState, *settingsService, *cvarManager);

    ImGui::Dummy(ImVec2(0, hs::ui::SectionSpacing()));
    RenderActions(triggerManualUpload);
}
