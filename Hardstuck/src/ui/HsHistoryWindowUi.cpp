#include "pch.h"
#include "ui/HsHistoryWindowUi.h"
#include "utils/HsUtils.h"      // for ExtractDatePortion, FormatTimestamp

#include "ui/ui_style.h"

#include "IMGUI/imgui.h"
#include <algorithm>
#include <unordered_map>
#include <vector>
#include <string>
#include <sstream>

namespace
{
    using TrainingMinutesByDate = std::unordered_map<std::string, float>;

    struct HistoryChartData
    {
        std::vector<float> mmrSeries;
        std::vector<float> trainingSeries;
        bool hasChart{false};
        bool hasTrainingOverlay{false};
    };

    TrainingMinutesByDate BuildTrainingMinutes(const std::vector<TrainingHistoryEntry>& history)
    {
        TrainingMinutesByDate minutesByDate;
        for (const auto& entry : history)
        {
            const std::string finishedDate = ExtractDatePortion(
                entry.finishedTime.empty() ? entry.startedTime : entry.finishedTime
            );
            const float minutes = static_cast<float>(entry.actualDuration) / 60.0f;
            minutesByDate[finishedDate] += minutes;
        }
        return minutesByDate;
    }

    std::vector<const MmrHistoryEntry*> SortMmrHistory(const std::vector<MmrHistoryEntry>& mmrHistory)
    {
        std::vector<const MmrHistoryEntry*> sorted;
        sorted.reserve(mmrHistory.size());
        for (const auto& entry : mmrHistory)
        {
            sorted.push_back(&entry);
        }
        std::sort(sorted.begin(), sorted.end(), [](const MmrHistoryEntry* lhs, const MmrHistoryEntry* rhs) {
            return lhs->timestamp < rhs->timestamp;
        });
        return sorted;
    }

    HistoryChartData BuildChartData(const std::vector<const MmrHistoryEntry*>& sortedMmr,
                                    const TrainingMinutesByDate& trainingMinutes)
    {
        HistoryChartData data;
        data.hasChart = sortedMmr.size() >= 2;

        if (!data.hasChart)
        {
            return data;
        }

        data.mmrSeries.reserve(sortedMmr.size());
        data.trainingSeries.reserve(sortedMmr.size());

        for (const MmrHistoryEntry* entry : sortedMmr)
        {
            data.mmrSeries.push_back(static_cast<float>(entry->mmr));
            const std::string dateKey = ExtractDatePortion(entry->timestamp);
            const auto trainingIt = trainingMinutes.find(dateKey);
            data.trainingSeries.push_back(trainingIt != trainingMinutes.end() ? trainingIt->second : 0.0f);
        }

        data.hasTrainingOverlay = std::any_of(
            data.trainingSeries.begin(),
            data.trainingSeries.end(),
            [](float value) { return value > 0.0f; }
        );

        return data;
    }

    void RenderStatus(const std::string& errorMessage,
                      bool loading,
                      std::chrono::system_clock::time_point lastFetched)
    {
        if (loading)
        {
            ImGui::Text("Fetching history...");
        }
        if (!errorMessage.empty())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", errorMessage.c_str());
        }
        if (lastFetched.time_since_epoch().count() > 0)
        {
            ImGui::Text("Last fetched: %s", FormatTimestamp(lastFetched).c_str());
        }
    }

    void RenderStatusSummary(const HistoryStatus& status)
    {
        ImGui::Separator();
        ImGui::Text("Status: generated %s | received %s", status.generatedAt.c_str(), status.receivedAt.c_str());
        ImGui::Text("MMR entries %d/%d | Training sessions %d/%d",
                    status.mmrEntries,
                    status.mmrLimit,
                    status.trainingSessions,
                    status.sessionLimit);
        ImGui::Text("Filters: playlist=%s mmr=[%s-%s] session=[%s-%s]",
                    status.filters.playlist.c_str(),
                    status.filters.mmrFrom.c_str(),
                    status.filters.mmrTo.c_str(),
                    status.filters.sessionStart.c_str(),
                    status.filters.sessionEnd.c_str());
    }

    void DrawSeries(ImDrawList* drawList,
                    const std::vector<float>& series,
                    ImVec2 plotMin,
                    ImVec2 plotMax,
                    float minVal,
                    float maxVal,
                    ImU32 color,
                    float thickness)
    {
        if (series.size() < 2)
        {
            return;
        }

        const float range = std::max(1.0f, maxVal - minVal);
        std::vector<ImVec2> points;
        points.reserve(series.size());
        for (size_t i = 0; i < series.size(); ++i)
        {
            const float xNorm = (series.size() <= 1)
                ? 0.0f
                : static_cast<float>(i) / static_cast<float>(series.size() - 1);
            const float x = plotMin.x + xNorm * (plotMax.x - plotMin.x);
            const float yNorm = (series[i] - minVal) / range;
            const float y = plotMax.y - yNorm * (plotMax.y - plotMin.y);
            points.emplace_back(x, y);
        }
        for (int i = 1; i < static_cast<int>(points.size()); ++i)
        {
            drawList->AddLine(points[i - 1], points[i], color, thickness);
        }
    }

    void RenderActivityChart(const HistoryChartData& chartData,
                             const std::vector<const MmrHistoryEntry*>& sortedMmr)
    {
        if (!chartData.hasChart)
        {
            if (sortedMmr.size() == 1)
            {
                ImGui::TextWrapped("At least two MMR entries are required to draw the overlay. Play another ranked match to add more data.");
            }
            else
            {
                ImGui::TextWrapped("No MMR history yet. Sync matches to populate the chart.");
            }
            return;
        }

        ImVec2 chartSize = ImVec2(ImGui::GetContentRegionAvail().x, 230.0f);
        if (chartSize.x < 320.0f)
        {
            chartSize.x = 320.0f;
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 plotMin = ImGui::GetCursorScreenPos();
        ImGui::Dummy(chartSize);
        const ImVec2 plotMax = ImGui::GetItemRectMax();
        drawList->AddRectFilled(plotMin, plotMax, IM_COL32(20, 20, 20, 255), 6.0f);
        drawList->AddRect(plotMin, plotMax, IM_COL32(70, 70, 70, 255), 6.0f);

        const int gridLines = 4;
        for (int i = 1; i < gridLines; ++i)
        {
            const float t = static_cast<float>(i) / gridLines;
            const float y = plotMin.y + t * (plotMax.y - plotMin.y);
            drawList->AddLine(ImVec2(plotMin.x, y), ImVec2(plotMax.x, y), IM_COL32(50, 50, 50, 255));
        }

        const float mmrMin = *std::min_element(chartData.mmrSeries.begin(), chartData.mmrSeries.end());
        const float mmrMax = *std::max_element(chartData.mmrSeries.begin(), chartData.mmrSeries.end());
        DrawSeries(drawList, chartData.mmrSeries, plotMin, plotMax, mmrMin, mmrMax, IM_COL32(80, 200, 255, 255), 2.5f);

        if (chartData.hasTrainingOverlay)
        {
            const float trainingMin = *std::min_element(chartData.trainingSeries.begin(), chartData.trainingSeries.end());
            const float trainingMax = *std::max_element(chartData.trainingSeries.begin(), chartData.trainingSeries.end());
            DrawSeries(drawList, chartData.trainingSeries, plotMin, plotMax, trainingMin, trainingMax, IM_COL32(255, 170, 60, 220), 2.0f);
        }

        ImGui::Dummy(ImVec2(0, 4));
        ImGui::TextColored(ImVec4(0.31f, 0.78f, 1.0f, 1.0f), "MMR");
        if (chartData.hasTrainingOverlay)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.67f, 0.24f, 1.0f), "Training minutes (per day, normalized)");
        }

        if (!chartData.mmrSeries.empty())
        {
            ImGui::Text("Latest MMR: %.0f", chartData.mmrSeries.back());
        }
        if (chartData.hasTrainingOverlay)
        {
            ImGui::Text("Latest training minutes: %.1f", chartData.trainingSeries.back());
        }
    }

    void RenderMmrEntries(const std::vector<MmrHistoryEntry>& entries)
    {
        if (!ImGui::CollapsingHeader("MMR entries", ImGuiTreeNodeFlags_DefaultOpen))
        {
            return;
        }

        for (const auto& entry : entries)
        {
            ImGui::Text("%s | %s | %d (%+d) | %s",
                        entry.timestamp.c_str(),
                        entry.playlist.c_str(),
                        entry.mmr,
                        entry.gamesPlayedDiff,
                        entry.source.c_str());
        }
    }

    void RenderTrainingEntries(const std::vector<TrainingHistoryEntry>& entries)
    {
        if (!ImGui::CollapsingHeader("Training sessions", ImGuiTreeNodeFlags_DefaultOpen))
        {
            return;
        }

        for (const auto& entry : entries)
        {
            ImGui::Text("%s -> %s | preset %s | %s | dur %d | blocks %d",
                        entry.startedTime.c_str(),
                        entry.finishedTime.c_str(),
                        entry.presetId.c_str(),
                        entry.source.c_str(),
                        entry.actualDuration,
                        entry.blocks);

            if (!entry.skillIds.empty())
            {
                std::ostringstream skills;
                for (size_t i = 0; i < entry.skillIds.size(); ++i)
                {
                    if (i > 0)
                    {
                        skills << ", ";
                    }
                    skills << entry.skillIds[i];
                }
                ImGui::TextWrapped("Skills: %s", skills.str().c_str());
            }
            if (!entry.notes.empty())
            {
                ImGui::TextWrapped("Notes: %s", entry.notes.c_str());
            }
            ImGui::Separator();
        }
    }

    void RenderAggregates(const HistorySnapshot::Aggregates& aggregates)
    {
        if (!aggregates.timeBySessionType.empty())
        {
            ImGui::Separator();
            ImGui::Text("Time by session type (minutes)");
            for (const auto& kv : aggregates.timeBySessionType)
            {
                ImGui::Text("%s: %.1f", kv.first.c_str(), kv.second / 60.0);
            }
        }

        if (!aggregates.mmrDeltas.empty())
        {
            ImGui::Separator();
            ImGui::Text("MMR changes");
            int displayed = 0;
            for (auto it = aggregates.mmrDeltas.rbegin(); it != aggregates.mmrDeltas.rend() && displayed < 10; ++it, ++displayed)
            {
                const auto& delta = *it;
                ImGui::Text("%s | %s | %s | %d (%+d)",
                    delta.timestamp.c_str(),
                    delta.playlist.c_str(),
                    delta.sessionType.c_str(),
                    delta.mmr,
                    delta.delta);
            }
        }
    }
}

void HsRenderHistoryWindowUi(
    HistorySnapshot const& snapshot,
    std::string const& errorMessage,
    bool loading,
    std::chrono::system_clock::time_point lastFetched,
    bool* showHistoryWindow,
    const std::string& activeSessionLabel,
    bool manualSessionActive
)
{
    if (ImGui::GetCurrentContext() == nullptr)
    {
        return;
    }

    [[maybe_unused]] auto styleScope = hs::ui::ApplyStyle();
    ImGui::SetNextWindowSize(ImVec2(640, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("History Review##history", showHistoryWindow))
    {
        ImGui::End();
        return;
    }

    RenderStatus(errorMessage, loading, lastFetched);
    ImGui::Text("Active session: %s%s",
        activeSessionLabel.empty() ? "unknown" : activeSessionLabel.c_str(),
        manualSessionActive ? " (manual)" : "");
    RenderStatusSummary(snapshot.status);

    ImGui::Text("Training vs MMR activity");

    const TrainingMinutesByDate trainingMinutes = BuildTrainingMinutes(snapshot.trainingHistory);
    const std::vector<const MmrHistoryEntry*> sortedMmr = SortMmrHistory(snapshot.mmrHistory);
    const HistoryChartData chartData = BuildChartData(sortedMmr, trainingMinutes);
    RenderActivityChart(chartData, sortedMmr);

    ImGui::Separator();
    RenderMmrEntries(snapshot.mmrHistory);
    RenderTrainingEntries(snapshot.trainingHistory);
    RenderAggregates(snapshot.aggregates);

    ImGui::End();
}
