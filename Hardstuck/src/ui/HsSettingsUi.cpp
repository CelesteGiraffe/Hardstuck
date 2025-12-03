#include "pch.h"
#include "ui/HsSettingsUi.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"

#include "settings/ISettingsService.h"
#include "ui/ui_style.h"

#include "IMGUI/imgui.h"

#include <string>
#include <cstring>
#include <filesystem>
#include <chrono>
#include <vector>

namespace
{
    struct SettingsUiState
    {
        char dataDirBuf[260] = {0};
        uint64_t maxBytes = 0;
        int maxFiles = 0;
        std::vector<std::string> focuses;
        int selectedFocusIdx = 0;
        char newFocusBuf[64] = {0};
        std::filesystem::path storePath;
        uint64_t storeSize = 0;
        std::string lastWrite;
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

    void SyncBuffers(SettingsUiState& uiState, ISettingsService& settingsService, const std::filesystem::path& currentStorePath)
    {
        const std::filesystem::path dataDir = settingsService.GetDataDirectory();
        SafeStrCopy(uiState.dataDirBuf, dataDir.string(), sizeof(uiState.dataDirBuf));
        uiState.maxBytes = settingsService.GetMaxStoreBytes();
        uiState.maxFiles = settingsService.GetMaxStoreFiles();
        uiState.focuses = settingsService.GetFocusList();
        if (uiState.selectedFocusIdx >= static_cast<int>(uiState.focuses.size()))
        {
            uiState.selectedFocusIdx = std::max(0, static_cast<int>(uiState.focuses.size()) - 1);
        }

        uiState.storePath = currentStorePath.empty() ? dataDir / "local_history.jsonl" : currentStorePath;
        uiState.storeSize = 0;
        uiState.lastWrite.clear();
        std::error_code ec;
        if (std::filesystem::exists(uiState.storePath, ec))
        {
            uiState.storeSize = static_cast<uint64_t>(std::filesystem::file_size(uiState.storePath, ec));
            auto writeTime = std::filesystem::last_write_time(uiState.storePath, ec);
            if (!ec)
            {
                const auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    writeTime - decltype(writeTime)::clock::now() + std::chrono::system_clock::now());
                uiState.lastWrite = FormatTimestamp(sctp);
            }
        }
    }

    void RenderStorageSection(SettingsUiState& uiState, ISettingsService& settingsService, CVarManagerWrapper& cvarManager)
    {
        ImGui::TextWrapped("Local storage location");
        ImGui::InputText("Data directory", uiState.dataDirBuf, sizeof(uiState.dataDirBuf));
        ImGui::InputScalar("Max file bytes", ImGuiDataType_U64, &uiState.maxBytes);
        ImGui::InputInt("Max files to keep", &uiState.maxFiles);

        if (ImGui::Button("Save storage settings"))
        {
            settingsService.SetDataDirectory(uiState.dataDirBuf);
            settingsService.SetMaxStoreBytes(uiState.maxBytes);
            settingsService.SetMaxStoreFiles(uiState.maxFiles);
            settingsService.SavePersistedSettings();
            cvarManager.log("HS: saved storage settings");
            SyncBuffers(uiState, settingsService, uiState.storePath);
        }

        ImGui::Separator();
        ImGui::TextWrapped("Current store: %s", uiState.storePath.string().c_str());
        ImGui::Text("Size: %llu bytes", static_cast<unsigned long long>(uiState.storeSize));
        ImGui::TextWrapped("Last write: %s", uiState.lastWrite.empty() ? "n/a" : uiState.lastWrite.c_str());
    }

    void RenderFocusSection(SettingsUiState& uiState, ISettingsService& settingsService, CVarManagerWrapper& cvarManager)
    {
        ImGui::TextWrapped("Training focuses");
        ImGui::InputText("New focus", uiState.newFocusBuf, sizeof(uiState.newFocusBuf));
        ImGui::SameLine();
        if (ImGui::Button("Add Focus", hs::ui::PrimaryButtonSize()))
        {
            const std::string newFocus = uiState.newFocusBuf;
            if (!newFocus.empty())
            {
                uiState.focuses.push_back(newFocus);
                settingsService.SetFocusList(uiState.focuses);
                uiState.focuses = settingsService.GetFocusList();
                settingsService.SavePersistedSettings();
                cvarManager.log("HS: added focus");
                uiState.selectedFocusIdx = static_cast<int>(uiState.focuses.size()) - 1;
            }
            uiState.newFocusBuf[0] = '\0';
        }

        ImGui::Separator();
        ImGui::TextWrapped("Available focuses");
        ImGui::BeginChild("focus_list_child", ImVec2(0.0f, 160.0f), true);
        for (size_t i = 0; i < uiState.focuses.size(); ++i)
        {
            const bool selected = static_cast<int>(i) == uiState.selectedFocusIdx;
            if (ImGui::Selectable(uiState.focuses[i].c_str(), selected))
            {
                uiState.selectedFocusIdx = static_cast<int>(i);
            }
        }
        ImGui::EndChild();

        const bool hasFocus = !uiState.focuses.empty() && uiState.selectedFocusIdx >= 0 && uiState.selectedFocusIdx < static_cast<int>(uiState.focuses.size());
        if (ImGui::Button("Remove Selected") && hasFocus)
        {
            uiState.focuses.erase(uiState.focuses.begin() + uiState.selectedFocusIdx);
            settingsService.SetFocusList(uiState.focuses);
            uiState.focuses = settingsService.GetFocusList();
            settingsService.SavePersistedSettings();
            cvarManager.log("HS: removed focus");
            if (uiState.selectedFocusIdx >= static_cast<int>(uiState.focuses.size()))
            {
                uiState.selectedFocusIdx = static_cast<int>(uiState.focuses.size()) - 1;
            }
        }
    }

    void RenderActions(HsToggleMenuFn toggleMenu, HsToggleOverlayFn toggleOverlay, CVarManagerWrapper& cvarManager)
    {
        ImGui::Spacing();
        if (ImGui::Button("Open Hardstuck Menu", hs::ui::PrimaryButtonSize()))
        {
            if (toggleMenu)
            {
                toggleMenu();
            }
            else
            {
                cvarManager.log("HS: menu toggle callback unavailable");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Open Overlay", hs::ui::PrimaryButtonSize()))
        {
            if (toggleOverlay)
            {
                toggleOverlay();
            }
            else
            {
                cvarManager.log("HS: overlay toggle callback unavailable");
            }
        }
    }
}

void HsRenderSettingsUi(
    ISettingsService* settingsService,
    CVarManagerWrapper* cvarManager,
    HsToggleMenuFn toggleMenu,
    HsToggleOverlayFn toggleOverlay,
    const std::filesystem::path& storePath
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
    SyncBuffers(uiState, *settingsService, storePath);

    RenderStorageSection(uiState, *settingsService, *cvarManager);
    ImGui::Dummy(ImVec2(0, hs::ui::SectionSpacing()));
    RenderFocusSection(uiState, *settingsService, *cvarManager);

    ImGui::Dummy(ImVec2(0, hs::ui::SectionSpacing()));
    RenderActions(std::move(toggleMenu), std::move(toggleOverlay), *cvarManager);
}
