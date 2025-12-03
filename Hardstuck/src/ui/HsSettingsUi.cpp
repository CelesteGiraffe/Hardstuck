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
        int dailyGoalMinutes = 60;
        std::filesystem::path storePath;
        uint64_t storeSize = 0;
        std::string lastWrite;
        bool initialized = false;
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
        uiState.dailyGoalMinutes = settingsService.GetDailyGoalMinutes();
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
                uiState.lastWrite = FormatTimestampUk(sctp);
            }
        }
    }

    void RenderStorageSection(SettingsUiState& uiState, ISettingsService& settingsService, CVarManagerWrapper& cvarManager)
    {
        ImGui::Columns(2, "storage_columns", false);
        ImGui::TextUnformatted("Data directory");
        ImGui::NextColumn();
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##data_dir", uiState.dataDirBuf, sizeof(uiState.dataDirBuf));
        ImGui::NextColumn();

        ImGui::TextUnformatted("Max file bytes");
        ImGui::NextColumn();
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputScalar("##max_bytes", ImGuiDataType_U64, &uiState.maxBytes);
        ImGui::NextColumn();

        ImGui::TextUnformatted("Max files to keep");
        ImGui::NextColumn();
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputInt("##max_files", &uiState.maxFiles);
        ImGui::Columns(1);

        ImGui::TextUnformatted("Daily goal (minutes)");
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputInt("##daily_goal", &uiState.dailyGoalMinutes);

        if (ImGui::Button("Save storage"))
        {
            settingsService.SetDataDirectory(uiState.dataDirBuf);
            settingsService.SetMaxStoreBytes(uiState.maxBytes);
            settingsService.SetMaxStoreFiles(uiState.maxFiles);
            settingsService.SetDailyGoalMinutes(uiState.dailyGoalMinutes);
            settingsService.SavePersistedSettings();
            cvarManager.log("HS: saved storage settings");
            SyncBuffers(uiState, settingsService, uiState.storePath);
        }

        if (ImGui::CollapsingHeader("Store info", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextWrapped("Path: %s", uiState.storePath.string().c_str());
            ImGui::Text("Size: %llu bytes", static_cast<unsigned long long>(uiState.storeSize));
            ImGui::TextWrapped("Last write: %s", uiState.lastWrite.empty() ? "n/a" : uiState.lastWrite.c_str());
        }
    }

    void RenderFocusSection(SettingsUiState& uiState, ISettingsService& settingsService, CVarManagerWrapper& cvarManager)
    {
        ImGui::TextWrapped("Training focuses");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
        ImGui::InputText("##new_focus", uiState.newFocusBuf, sizeof(uiState.newFocusBuf));
        ImGui::SameLine();
        if (ImGui::Button("Add", ImVec2(80.0f, 0.0f)))
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
        ImGui::Columns(2, "focus_columns", false);
        ImGui::TextUnformatted("Focus");
        ImGui::NextColumn();
        ImGui::TextUnformatted("Actions");
        ImGui::NextColumn();
        ImGui::Separator();

        for (size_t i = 0; i < uiState.focuses.size(); ++i)
        {
            const bool selected = static_cast<int>(i) == uiState.selectedFocusIdx;
            if (ImGui::Selectable(uiState.focuses[i].c_str(), selected, ImGuiSelectableFlags_SpanAllColumns))
            {
                uiState.selectedFocusIdx = static_cast<int>(i);
            }
            ImGui::NextColumn();
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::SmallButton("Remove"))
            {
                uiState.focuses.erase(uiState.focuses.begin() + static_cast<long long>(i));
                settingsService.SetFocusList(uiState.focuses);
                uiState.focuses = settingsService.GetFocusList();
                settingsService.SavePersistedSettings();
                cvarManager.log("HS: removed focus");
                if (uiState.selectedFocusIdx >= static_cast<int>(uiState.focuses.size()))
                {
                    uiState.selectedFocusIdx = static_cast<int>(uiState.focuses.size()) - 1;
                }
            }
            ImGui::PopID();
            ImGui::NextColumn();
        }
        ImGui::Columns(1);
    }

    void RenderActions(HsToggleMenuFn toggleMenu, HsToggleOverlayFn toggleOverlay, CVarManagerWrapper& cvarManager)
    {
        ImGui::Spacing();
        const ImVec2 actionSize = ImVec2(hs::ui::PrimaryButtonSize().x, 0.0f);
        if (ImGui::Button("Open Hardstuck Menu", actionSize))
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
        if (ImGui::Button("Open Overlay", actionSize))
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
    if (!uiState.initialized)
    {
        SyncBuffers(uiState, *settingsService, storePath);
        uiState.initialized = true;
    }

    RenderStorageSection(uiState, *settingsService, *cvarManager);
    ImGui::Dummy(ImVec2(0, hs::ui::SectionSpacing()));
    RenderFocusSection(uiState, *settingsService, *cvarManager);

    ImGui::Dummy(ImVec2(0, hs::ui::SectionSpacing()));
    RenderActions(std::move(toggleMenu), std::move(toggleOverlay), *cvarManager);
}
