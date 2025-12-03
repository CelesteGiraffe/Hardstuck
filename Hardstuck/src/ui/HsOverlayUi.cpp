#include "pch.h"
#include "ui/HsOverlayUi.h"

#include "ui/ui_style.h"
#include "utils/HsUtils.h" // FormatTimestamp, ExtractDatePortion
#include <algorithm>
#include <cstdio>

#if __has_include("bakkesmod/wrappers/cvarmanagerwrapper.h")
#include "bakkesmod/wrappers/cvarmanagerwrapper.h"
#endif

#if __has_include("imgui.h")
#include "imgui.h"
#elif __has_include("imgui/imgui.h")
#include "imgui/imgui.h"
#endif

namespace
{
    struct HistoryOverlaySummary
    {
        int mmrEntries{0};
        int trainingEntries{0};
        int latestMmr{0};
        int latestMmrDelta{0};
        float latestTrainingMinutes{0.0f};
        std::string lastMmrTimestamp;
        std::string lastTrainingTimestamp;
    };

    const MmrHistoryEntry* FindLatest(const std::vector<MmrHistoryEntry>& history, const MmrHistoryEntry* skip)
    {
        const MmrHistoryEntry* latest = nullptr;
        for (const auto& entry : history)
        {
            if (&entry == skip)
            {
                continue;
            }
            if (latest == nullptr || entry.timestamp > latest->timestamp)
            {
                latest = &entry;
            }
        }
        return latest;
    }

    float TrainingMinutesForDate(const std::vector<TrainingHistoryEntry>& history, const std::string& date)
    {
        float minutes = 0.0f;
        for (const auto& entry : history)
        {
            const std::string finishedDate = ExtractDatePortion(
                entry.finishedTime.empty() ? entry.startedTime : entry.finishedTime
            );
            if (finishedDate == date)
            {
                minutes += static_cast<float>(entry.actualDuration) / 60.0f;
            }
        }
        return minutes;
    }

    HistoryOverlaySummary BuildOverlaySummary(const HistorySnapshot& snapshot)
    {
        HistoryOverlaySummary summary;
        summary.mmrEntries = snapshot.status.mmrEntries;
        summary.trainingEntries = snapshot.status.trainingSessions;
        summary.lastMmrTimestamp = snapshot.status.lastMmrTimestamp;
        summary.lastTrainingTimestamp = snapshot.status.lastTrainingTimestamp;

        const MmrHistoryEntry* latest = FindLatest(snapshot.mmrHistory, nullptr);
        const MmrHistoryEntry* previous = FindLatest(snapshot.mmrHistory, latest);
        if (latest != nullptr)
        {
            summary.latestMmr = latest->mmr;
        }
        if (latest != nullptr && previous != nullptr)
        {
            summary.latestMmrDelta = latest->mmr - previous->mmr;
        }

        if (latest != nullptr)
        {
            const std::string dateKey = ExtractDatePortion(latest->timestamp);
            summary.latestTrainingMinutes = TrainingMinutesForDate(snapshot.trainingHistory, dateKey);
        }
        else if (!snapshot.trainingHistory.empty())
        {
            summary.latestTrainingMinutes = static_cast<float>(snapshot.trainingHistory.back().actualDuration) / 60.0f;
        }

        return summary;
    }
}

void HsRenderOverlayUi(
    CVarManagerWrapper* cvarManager,
    const std::string& lastResponse,
    const std::string& lastError,
    HistorySnapshot const& historySnapshot,
    std::string const& historyError,
    bool historyLoading,
    std::chrono::system_clock::time_point historyLastFetched,
    const std::string& activeSessionLabel,
    bool manualSessionActive,
    int dailyGoalMinutes,
    const std::vector<std::string>& focuses,
    const std::string& activeFocus,
    std::function<void(const std::string&)> setActiveFocus,
    std::function<void()> toggleFocusTimer,
    HsExecuteHistoryWindowFn executeHistoryWindowCommand,
    HsFetchHistoryFn fetchHistoryFn
)
{
    if (ImGui::GetCurrentContext() == nullptr)
    {
        return;
    }

    [[maybe_unused]] auto styleScope = hs::ui::ApplyStyle();

    // Optional ImGui demo toggle
    bool showDemo = false;
    if (cvarManager)
    {
        try
        {
            showDemo = cvarManager->getCvar("hs_ui_debug_show_demo").getBoolValue();
        }
        catch (...)
        {
            showDemo = false;
        }
    }
    if (showDemo)
    {
        ImGui::ShowDemoWindow(&showDemo);
    }

    if (!ImGui::Begin("Hardstuck : Rocket League Training Journal##overlay", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::End();
        return;
    }

    ImGui::TextWrapped("Local capture + history viewer");
    ImGui::TextWrapped("Active session: %s%s",
        activeSessionLabel.empty() ? "unknown" : activeSessionLabel.c_str(),
        manualSessionActive ? " (timer active)" : "");

    if (!lastResponse.empty())
    {
        ImGui::TextWrapped("Last response: %s", lastResponse.c_str());
    }
    if (manualSessionActive)
    {
        ImGui::TextWrapped("Manual session active");
    }
    if (!lastError.empty())
    {
        ImGui::TextWrapped("Last error: %s", lastError.c_str());
    }

    const HistoryOverlaySummary summary = BuildOverlaySummary(historySnapshot);
    ImGui::Separator();
    ImGui::TextUnformatted("History snapshot");
    if (historyLoading)
    {
        ImGui::TextColored(ImVec4(0.71f, 0.86f, 1.0f, 1.0f), "Fetching history…");
    }
    if (!historyError.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.49f, 0.49f, 1.0f), "%s", historyError.c_str());
    }
    ImGui::Text("Entries: %d mmr | %d training", summary.mmrEntries, summary.trainingEntries);
    if (historyLastFetched.time_since_epoch().count() > 0)
    {
        ImGui::Text("Last fetched: %s", FormatTimestampUk(historyLastFetched).c_str());
    }
    ImGui::Text("Latest MMR: %d (%+d)", summary.latestMmr, summary.latestMmrDelta);
    ImGui::Text("Training on that day: %.1f min", summary.latestTrainingMinutes);
    const float goalMinutes = dailyGoalMinutes > 0 ? static_cast<float>(dailyGoalMinutes) : 60.0f;
    const float trainingTargetProgress = std::clamp(summary.latestTrainingMinutes / goalMinutes, 0.0f, 1.0f);
    char goalLabel[64];
    std::snprintf(goalLabel, sizeof(goalLabel), "%dm daily target", static_cast<int>(goalMinutes));
    ImGui::ProgressBar(trainingTargetProgress, ImVec2(240.0f, 0.0f), goalLabel);

    ImGui::Separator();
    ImGui::TextWrapped("Training focus");
    if (!focuses.empty())
    {
        static int selectedFocus = 0;
        if (!activeFocus.empty())
        {
            for (int i = 0; i < static_cast<int>(focuses.size()); ++i)
            {
                if (focuses[i] == activeFocus)
                {
                    selectedFocus = i;
                    break;
                }
            }
        }
        if (selectedFocus >= static_cast<int>(focuses.size()))
        {
            selectedFocus = static_cast<int>(focuses.size()) - 1;
        }
        if (selectedFocus < 0)
        {
            selectedFocus = 0;
        }
        if (ImGui::BeginCombo("##focus_combo", activeFocus.empty() ? focuses[selectedFocus].c_str() : activeFocus.c_str()))
        {
            for (int i = 0; i < static_cast<int>(focuses.size()); ++i)
            {
                const bool isSelected = (i == selectedFocus);
                if (ImGui::Selectable(focuses[i].c_str(), isSelected))
                {
                    selectedFocus = i;
                    if (setActiveFocus)
                    {
                        setActiveFocus(focuses[i]);
                    }
                }
                if (isSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }
    else
    {
        ImGui::TextWrapped("No focuses defined. Add one in settings.");
    }

    if (ImGui::Button(manualSessionActive ? "Stop timer" : "Start timer"))
    {
        if (toggleFocusTimer)
        {
            toggleFocusTimer();
        }
    }
    ImGui::SameLine();
    ImGui::TextWrapped("%s", activeFocus.empty() ? "No focus selected" : activeFocus.c_str());

    if (ImGui::Button("Refresh History"))
    {
        if (fetchHistoryFn)
        {
            fetchHistoryFn();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Open History Window Now"))
    {
        if (executeHistoryWindowCommand)
        {
            executeHistoryWindowCommand();
        }
    }

    ImGui::End();
}
