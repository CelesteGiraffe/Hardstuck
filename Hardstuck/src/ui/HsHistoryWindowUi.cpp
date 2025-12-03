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
        std::vector<float> mmrDeltas;
        std::vector<std::string> labels;
        float mmrMin{0.0f};
        float mmrMax{0.0f};
        float trainingMax{0.0f};
        bool hasChart{false};
        bool hasTrainingOverlay{false};
    };

    struct DailyComparisonRow
    {
        std::string date;
        float trainingMinutes{0.0f};
        int mmrDelta{0};
        int closingMmr{0};
    };

    struct HistoryOverview
    {
        int mmrEntries{0};
        int trainingEntries{0};
        int mmrLimit{0};
        int trainingLimit{0};
        std::string lastMmrTimestamp;
        std::string lastTrainingTimestamp;
        std::string generatedAt;
        std::string receivedAt;
        int latestMmr{0};
        float latestTrainingMinutes{0.0f};
        float totalTrainingMinutes{0.0f};
    };

    struct HistoryUiState
    {
        int maxChartPoints{60};
        bool showTrainingOverlay{true};
        bool showDailyComparison{true};
        bool highlightMmrDelta{true};
    };

    HistoryUiState& GetUiState()
    {
        static HistoryUiState state;
        return state;
    }

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
                                    const TrainingMinutesByDate& trainingMinutes,
                                    int maxPoints)
    {
        HistoryChartData data;

        if (sortedMmr.size() < 2)
        {
            return data;
        }

        const size_t start = (maxPoints > 1 && sortedMmr.size() > static_cast<size_t>(maxPoints))
            ? sortedMmr.size() - static_cast<size_t>(maxPoints)
            : 0;

        data.mmrSeries.reserve(sortedMmr.size() - start);
        data.trainingSeries.reserve(sortedMmr.size() - start);
        data.labels.reserve(sortedMmr.size() - start);
        data.mmrDeltas.reserve(sortedMmr.size() - start);

        int previousMmr = sortedMmr[start]->mmr;
        for (size_t i = start; i < sortedMmr.size(); ++i)
        {
            const MmrHistoryEntry* entry = sortedMmr[i];
            data.mmrSeries.push_back(static_cast<float>(entry->mmr));
            data.labels.push_back(entry->timestamp);

            const std::string dateKey = ExtractDatePortion(entry->timestamp);
            const auto trainingIt = trainingMinutes.find(dateKey);
            data.trainingSeries.push_back(trainingIt != trainingMinutes.end() ? trainingIt->second : 0.0f);

            data.mmrDeltas.push_back(static_cast<float>(entry->mmr - previousMmr));
            previousMmr = entry->mmr;
        }

        data.hasTrainingOverlay = std::any_of(
            data.trainingSeries.begin(),
            data.trainingSeries.end(),
            [](float value) { return value > 0.0f; }
        );
        data.hasChart = data.mmrSeries.size() >= 2;

        if (data.hasChart)
        {
            data.mmrMin = *std::min_element(data.mmrSeries.begin(), data.mmrSeries.end());
            data.mmrMax = *std::max_element(data.mmrSeries.begin(), data.mmrSeries.end());
            data.trainingMax = *std::max_element(data.trainingSeries.begin(), data.trainingSeries.end());
        }

        return data;
    }

    std::vector<DailyComparisonRow> BuildDailyComparison(const std::vector<const MmrHistoryEntry*>& sortedMmr,
                                                         const TrainingMinutesByDate& trainingMinutes)
    {
        std::vector<DailyComparisonRow> rows;
        if (sortedMmr.empty() && trainingMinutes.empty())
        {
            return rows;
        }

        std::unordered_map<std::string, size_t> indexByDate;
        indexByDate.reserve(sortedMmr.size() + trainingMinutes.size());

        if (!sortedMmr.empty())
        {
            int previousMmr = sortedMmr.front()->mmr;
            for (const MmrHistoryEntry* entry : sortedMmr)
            {
                const std::string date = ExtractDatePortion(entry->timestamp);
                const auto found = indexByDate.find(date);
                if (found == indexByDate.end())
                {
                    rows.push_back({date, 0.0f, 0, entry->mmr});
                    indexByDate.emplace(date, rows.size() - 1);
                }

                DailyComparisonRow& row = rows[indexByDate[date]];
                row.mmrDelta += entry->mmr - previousMmr;
                row.closingMmr = entry->mmr;
                previousMmr = entry->mmr;
            }
        }

        for (const auto& training : trainingMinutes)
        {
            const auto found = indexByDate.find(training.first);
            if (found == indexByDate.end())
            {
                rows.push_back({training.first, training.second, 0, 0});
                indexByDate.emplace(training.first, rows.size() - 1);
            }
            else
            {
                rows[found->second].trainingMinutes = training.second;
            }
        }

        std::sort(rows.begin(), rows.end(), [](const DailyComparisonRow& lhs, const DailyComparisonRow& rhs) {
            return lhs.date < rhs.date;
        });
        return rows;
    }

    HistoryOverview BuildOverview(const HistorySnapshot& snapshot,
                                  const std::vector<const MmrHistoryEntry*>& sortedMmr,
                                  const TrainingMinutesByDate& trainingMinutes)
    {
        HistoryOverview overview;
        overview.mmrEntries = snapshot.status.mmrEntries;
        overview.trainingEntries = snapshot.status.trainingSessions;
        overview.mmrLimit = snapshot.status.mmrLimit;
        overview.trainingLimit = snapshot.status.sessionLimit;
        overview.lastMmrTimestamp = snapshot.status.lastMmrTimestamp;
        overview.lastTrainingTimestamp = snapshot.status.lastTrainingTimestamp;
        overview.generatedAt = snapshot.status.generatedAt;
        overview.receivedAt = snapshot.status.receivedAt;

        if (!sortedMmr.empty())
        {
            overview.latestMmr = sortedMmr.back()->mmr;
        }

        if (!snapshot.trainingHistory.empty())
        {
            overview.latestTrainingMinutes = static_cast<float>(snapshot.trainingHistory.back().actualDuration) / 60.0f;
        }

        for (const auto& session : snapshot.aggregates.timeBySessionType)
        {
            overview.totalTrainingMinutes += static_cast<float>(session.second) / 60.0f;
        }

        if (!sortedMmr.empty())
        {
            const std::string lastDate = ExtractDatePortion(sortedMmr.back()->timestamp);
            const auto trainingIt = trainingMinutes.find(lastDate);
            if (trainingIt != trainingMinutes.end())
            {
                overview.latestTrainingMinutes = trainingIt->second;
            }
        }

        return overview;
    }

    void RenderStatus(const std::string& errorMessage,
                      bool loading,
                      std::chrono::system_clock::time_point lastFetched,
                      const std::string& activeSessionLabel,
                      bool manualSessionActive)
    {
        if (loading)
        {
            ImGui::TextColored(ImVec4(0.71f, 0.86f, 1.0f, 1.0f), "Fetching history...");
        }
        if (!errorMessage.empty())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.49f, 0.49f, 1.0f), "%s", errorMessage.c_str());
        }
        if (lastFetched.time_since_epoch().count() > 0)
        {
            ImGui::Text("Last fetched: %s", FormatTimestamp(lastFetched).c_str());
        }

        ImGui::Text("Active session: %s%s",
            activeSessionLabel.empty() ? "unknown" : activeSessionLabel.c_str(),
            manualSessionActive ? " (manual)" : "");
    }

    void RenderStatusSummary(const HistoryOverview& overview, const HistoryStatus& status)
    {
        ImGui::Separator();
        ImGui::Text("History status");
        ImGui::Text("Generated %s | Received %s", overview.generatedAt.c_str(), overview.receivedAt.c_str());
        ImGui::Text("Filters: playlist=%s mmr=[%s-%s] session=[%s-%s]",
                    status.filters.playlist.c_str(),
                    status.filters.mmrFrom.c_str(),
                    status.filters.mmrTo.c_str(),
                    status.filters.sessionStart.c_str(),
                    status.filters.sessionEnd.c_str());
    }

    void RenderOverviewCards(const HistoryOverview& overview)
    {
        ImGui::Columns(3, "overview_columns", false);

        ImGui::TextUnformatted("MMR log");
        ImGui::Text("Entries: %d / %d", overview.mmrEntries, overview.mmrLimit);
        ImGui::Text("Last: %s", overview.lastMmrTimestamp.empty() ? "n/a" : overview.lastMmrTimestamp.c_str());
        ImGui::Text("Latest rating: %d", overview.latestMmr);
        ImGui::NextColumn();

        ImGui::TextUnformatted("Training log");
        ImGui::Text("Sessions: %d / %d", overview.trainingEntries, overview.trainingLimit);
        ImGui::Text("Last: %s", overview.lastTrainingTimestamp.empty() ? "n/a" : overview.lastTrainingTimestamp.c_str());
        ImGui::Text("Latest day minutes: %.1f", overview.latestTrainingMinutes);
        ImGui::NextColumn();

        ImGui::TextUnformatted("Totals");
        ImGui::Text("Recorded minutes: %.1f", overview.totalTrainingMinutes);
        ImGui::Text("Timeline span: %s", overview.generatedAt.empty() ? "n/a" : overview.generatedAt.c_str());
        ImGui::Text("Last ingest: %s", overview.receivedAt.empty() ? "n/a" : overview.receivedAt.c_str());

        ImGui::Columns(1);
    }

    void DrawTrainingBands(ImDrawList* drawList,
                           const HistoryChartData& chartData,
                           ImVec2 plotMin,
                           ImVec2 plotMax,
                           ImU32 color)
    {
        if (!chartData.hasTrainingOverlay || chartData.trainingMax <= 0.0f || chartData.trainingSeries.size() < 2)
        {
            return;
        }

        const float overlayHeight = (plotMax.y - plotMin.y) * 0.35f;
        const float xStep = (plotMax.x - plotMin.x) / static_cast<float>(chartData.trainingSeries.size());

        for (size_t i = 0; i < chartData.trainingSeries.size(); ++i)
        {
            const float height = overlayHeight * (chartData.trainingSeries[i] / chartData.trainingMax);
            const float x0 = plotMin.x + xStep * static_cast<float>(i);
            const float x1 = std::min(plotMax.x, plotMin.x + xStep * static_cast<float>(i + 1));
            const float top = plotMax.y - height;
            drawList->AddRectFilled(ImVec2(x0, top), ImVec2(x1, plotMax.y), color, 2.0f);
        }
    }

    std::vector<ImVec2> BuildPoints(const std::vector<float>& series, ImVec2 plotMin, ImVec2 plotMax, float minVal, float maxVal)
    {
        std::vector<ImVec2> points;
        if (series.size() < 2)
        {
            return points;
        }

        const float range = std::max(1.0f, maxVal - minVal);
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
        return points;
    }

    void DrawMmrSeries(ImDrawList* drawList,
                       const HistoryChartData& chartData,
                       ImVec2 plotMin,
                       ImVec2 plotMax,
                       bool highlightDeltas)
    {
        if (chartData.mmrSeries.size() < 2)
        {
            return;
        }

        const std::vector<ImVec2> points = BuildPoints(chartData.mmrSeries, plotMin, plotMax, chartData.mmrMin, chartData.mmrMax);
        for (size_t i = 1; i < points.size(); ++i)
        {
            const float delta = chartData.mmrDeltas[i];
            const ImU32 color = highlightDeltas
                ? (delta >= 0.0f ? IM_COL32(82, 209, 124, 255) : IM_COL32(237, 122, 107, 255))
                : IM_COL32(80, 200, 255, 255);
            drawList->AddLine(points[i - 1], points[i], color, 2.6f);
        }
    }

    void RenderActivityChart(const HistoryChartData& chartData,
                             bool showTrainingOverlay,
                             bool highlightMmrDelta)
    {
        if (!chartData.hasChart)
        {
            ImGui::TextWrapped("Play or import at least two ranked matches to view MMR trendlines. Training minutes still show up in the comparison tables below.");
            return;
        }

        ImVec2 chartSize(ImGui::GetContentRegionAvail().x, 260.0f);
        if (chartSize.x < 360.0f)
        {
            chartSize.x = 360.0f;
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImGui::InvisibleButton("history_chart_canvas", chartSize);
        const ImVec2 plotMin = ImGui::GetItemRectMin();
        const ImVec2 plotMax = ImGui::GetItemRectMax();

        drawList->AddRectFilled(plotMin, plotMax, IM_COL32(18, 20, 24, 255), 8.0f);
        drawList->AddRect(plotMin, plotMax, IM_COL32(65, 74, 88, 255), 8.0f);

        const int gridLines = 4;
        for (int i = 1; i < gridLines; ++i)
        {
            const float t = static_cast<float>(i) / gridLines;
            const float y = plotMin.y + t * (plotMax.y - plotMin.y);
            drawList->AddLine(ImVec2(plotMin.x, y), ImVec2(plotMax.x, y), IM_COL32(48, 54, 64, 255));
        }

        if (showTrainingOverlay)
        {
            DrawTrainingBands(drawList, chartData, plotMin, plotMax, IM_COL32(255, 177, 86, 120));
        }

        DrawMmrSeries(drawList, chartData, plotMin, plotMax, highlightMmrDelta);

        const bool hovered = ImGui::IsItemHovered();
        const ImVec2 mousePos = ImGui::GetIO().MousePos;
        if (hovered && mousePos.x >= plotMin.x && mousePos.x <= plotMax.x && !chartData.labels.empty())
        {
            const float t = (plotMax.x - plotMin.x) > 0.0f
                ? (mousePos.x - plotMin.x) / (plotMax.x - plotMin.x)
                : 0.0f;
            const size_t idx = static_cast<size_t>(std::clamp(
                t * static_cast<float>(chartData.mmrSeries.size() - 1),
                0.0f,
                static_cast<float>(chartData.mmrSeries.size() - 1)));

            ImGui::BeginTooltip();
            ImGui::Text("Date: %s", chartData.labels[idx].c_str());
            ImGui::Text("MMR: %.0f (%+.0f)", chartData.mmrSeries[idx], chartData.mmrDeltas[idx]);
            if (chartData.hasTrainingOverlay)
            {
                ImGui::Text("Training: %.1f min", chartData.trainingSeries[idx]);
            }
            ImGui::EndTooltip();
        }

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::TextColored(ImVec4(0.31f, 0.78f, 1.0f, 1.0f), "MMR trend");
        if (showTrainingOverlay && chartData.hasTrainingOverlay)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.67f, 0.24f, 1.0f), "Training minutes (per day)");
        }
    }

    void RenderChartControls(HistoryUiState& uiState, const HistoryChartData& chartData)
    {
        ImGui::SliderInt("Points shown", &uiState.maxChartPoints, 10, 120, "%d entries");
        ImGui::SameLine();
        ImGui::Checkbox("Show training overlay", &uiState.showTrainingOverlay);
        ImGui::SameLine();
        ImGui::Checkbox("Highlight MMR delta", &uiState.highlightMmrDelta);

        ImGui::Text("Latest MMR: %.0f", chartData.mmrSeries.empty() ? 0.0f : chartData.mmrSeries.back());
        if (chartData.hasTrainingOverlay)
        {
            ImGui::SameLine();
            ImGui::Text("Latest training: %.1f min", chartData.trainingSeries.empty() ? 0.0f : chartData.trainingSeries.back());
        }
    }

    void RenderComparisonTable(const std::vector<DailyComparisonRow>& comparisons, bool expanded)
    {
        if (!expanded)
        {
            return;
        }
        if (comparisons.empty())
        {
            ImGui::TextWrapped("No comparison yet. Once you have training blocks and MMR entries on the same days, they will show up here.");
            return;
        }

        ImGui::BeginChild("comparison_child", ImVec2(0.0f, 200.0f), true);
        ImGui::Columns(4, "comparison_columns");
        ImGui::TextUnformatted("Date");
        ImGui::NextColumn();
        ImGui::TextUnformatted("Training (min)");
        ImGui::NextColumn();
        ImGui::TextUnformatted("MMR delta");
        ImGui::NextColumn();
        ImGui::TextUnformatted("Closing MMR");
        ImGui::NextColumn();
        ImGui::Separator();

        for (const auto& row : comparisons)
        {
            ImGui::TextUnformatted(row.date.c_str());
            ImGui::NextColumn();
            ImGui::Text("%.1f", row.trainingMinutes);
            ImGui::NextColumn();
            const ImVec4 deltaColor = row.mmrDelta > 0
                ? ImVec4(0.50f, 0.86f, 0.63f, 1.0f)
                : (row.mmrDelta < 0 ? ImVec4(0.93f, 0.58f, 0.50f, 1.0f) : ImVec4(0.78f, 0.82f, 0.90f, 1.0f));
            ImGui::TextColored(deltaColor, "%+d", row.mmrDelta);
            ImGui::NextColumn();
            ImGui::Text("%d", row.closingMmr);
            ImGui::NextColumn();
        }

        ImGui::Columns(1);
        ImGui::EndChild();
    }

    void RenderMmrEntries(const std::vector<MmrHistoryEntry>& entries)
    {
        if (!ImGui::CollapsingHeader("MMR entries", ImGuiTreeNodeFlags_DefaultOpen))
        {
            return;
        }
        if (entries.empty())
        {
            ImGui::TextWrapped("No MMR events captured yet. Play a ranked match or sync history.");
            return;
        }

        ImGui::BeginChild("mmr_entries_child", ImVec2(0.0f, 200.0f), true);
        ImGui::Columns(5, "mmr_columns");
        ImGui::TextUnformatted("Source"); ImGui::NextColumn();
        ImGui::TextUnformatted("Time"); ImGui::NextColumn();
        ImGui::TextUnformatted("Playlist"); ImGui::NextColumn();
        ImGui::TextUnformatted("MMR"); ImGui::NextColumn();
        ImGui::TextUnformatted("Game #"); ImGui::NextColumn();
        ImGui::Separator();

        for (const auto& entry : entries)
        {
            ImGui::TextUnformatted(entry.source.c_str());
            ImGui::NextColumn();
            ImGui::TextUnformatted(entry.timestamp.c_str());
            ImGui::NextColumn();
            ImGui::TextUnformatted(entry.playlist.c_str());
            ImGui::NextColumn();
            ImGui::Text("%d", entry.mmr);
            ImGui::NextColumn();
            ImGui::Text("%+d", entry.gamesPlayedDiff);
            ImGui::NextColumn();
        }
        ImGui::Columns(1);
        ImGui::EndChild();
    }

    void RenderTrainingEntries(const std::vector<TrainingHistoryEntry>& entries)
    {
        if (!ImGui::CollapsingHeader("Training sessions", ImGuiTreeNodeFlags_DefaultOpen))
        {
            return;
        }
        if (entries.empty())
        {
            ImGui::TextWrapped("Start a workshop or training pack session to see it appear here.");
            return;
        }

        ImGui::BeginChild("training_entries_child", ImVec2(0.0f, 220.0f), true);
        ImGui::Columns(6, "training_columns");
        ImGui::TextUnformatted("Start"); ImGui::NextColumn();
        ImGui::TextUnformatted("End"); ImGui::NextColumn();
        ImGui::TextUnformatted("Preset"); ImGui::NextColumn();
        ImGui::TextUnformatted("Duration (s)"); ImGui::NextColumn();
        ImGui::TextUnformatted("Blocks"); ImGui::NextColumn();
        ImGui::TextUnformatted("Notes"); ImGui::NextColumn();
        ImGui::NextColumn();
        ImGui::Separator();

        for (const auto& entry : entries)
        {
            ImGui::TextUnformatted(entry.startedTime.c_str());
            ImGui::NextColumn();
            ImGui::TextUnformatted(entry.finishedTime.c_str());
            ImGui::NextColumn();
            ImGui::TextUnformatted(entry.presetId.c_str());
            ImGui::NextColumn();
            ImGui::Text("%d", entry.actualDuration);
            ImGui::NextColumn();
            ImGui::Text("%d", entry.blocks);
            ImGui::NextColumn();
            if (!entry.notes.empty())
            {
                ImGui::TextWrapped("%s", entry.notes.c_str());
            }
            else
            {
                ImGui::TextDisabled("—");
            }
            ImGui::NextColumn();
        }
        ImGui::Columns(1);
        ImGui::EndChild();
    }

    void RenderAggregates(const HistorySnapshot::Aggregates& aggregates)
    {
        if (!ImGui::CollapsingHeader("Aggregates", ImGuiTreeNodeFlags_DefaultOpen))
        {
            return;
        }

        if (!aggregates.timeBySessionType.empty())
        {
            ImGui::Columns(2, "session_time_columns");
            ImGui::TextUnformatted("Session type"); ImGui::NextColumn();
            ImGui::TextUnformatted("Minutes"); ImGui::NextColumn();
            ImGui::NextColumn();
            ImGui::Separator();
            for (const auto& kv : aggregates.timeBySessionType)
            {
                ImGui::TextUnformatted(kv.first.c_str());
                ImGui::NextColumn();
                ImGui::Text("%.1f", kv.second / 60.0);
                ImGui::NextColumn();
            }
            ImGui::Columns(1);
        }
        else
        {
            ImGui::TextDisabled("No session breakdown yet.");
        }

        if (!aggregates.mmrDeltas.empty())
        {
            ImGui::Dummy(ImVec2(0.0f, hs::ui::SectionSpacing()));
            ImGui::TextUnformatted("Recent MMR deltas");
            ImGui::Columns(5, "recent_deltas_columns");
            ImGui::TextUnformatted("Time"); ImGui::NextColumn();
            ImGui::TextUnformatted("Playlist"); ImGui::NextColumn();
            ImGui::TextUnformatted("Session type"); ImGui::NextColumn();
            ImGui::TextUnformatted("MMR"); ImGui::NextColumn();
            ImGui::TextUnformatted("Delta"); ImGui::NextColumn();
            ImGui::NextColumn();
            ImGui::Separator();

            int displayed = 0;
            for (auto it = aggregates.mmrDeltas.rbegin(); it != aggregates.mmrDeltas.rend() && displayed < 12; ++it, ++displayed)
            {
                const auto& delta = *it;
                ImGui::TextUnformatted(delta.timestamp.c_str());
                ImGui::NextColumn();
                ImGui::TextUnformatted(delta.playlist.c_str());
                ImGui::NextColumn();
                ImGui::TextUnformatted(delta.sessionType.c_str());
                ImGui::NextColumn();
                ImGui::Text("%d", delta.mmr);
                ImGui::NextColumn();
                const ImVec4 deltaColor = delta.delta >= 0 ? ImVec4(0.50f, 0.86f, 0.63f, 1.0f)
                                                           : ImVec4(0.93f, 0.58f, 0.50f, 1.0f);
                ImGui::TextColored(deltaColor, "%+d", delta.delta);
                ImGui::NextColumn();
            }
            ImGui::Columns(1);
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
    ImGui::SetNextWindowSize(ImVec2(740, 640), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("History Review##history", showHistoryWindow))
    {
        ImGui::End();
        return;
    }

    HistoryUiState& uiState = GetUiState();
    const TrainingMinutesByDate trainingMinutes = BuildTrainingMinutes(snapshot.trainingHistory);
    const std::vector<const MmrHistoryEntry*> sortedMmr = SortMmrHistory(snapshot.mmrHistory);
    const HistoryChartData chartData = BuildChartData(sortedMmr, trainingMinutes, uiState.maxChartPoints);
    const HistoryOverview overview = BuildOverview(snapshot, sortedMmr, trainingMinutes);
    const std::vector<DailyComparisonRow> comparisons = BuildDailyComparison(sortedMmr, trainingMinutes);

    RenderStatus(errorMessage, loading, lastFetched, activeSessionLabel, manualSessionActive);
    RenderOverviewCards(overview);
    RenderStatusSummary(overview, snapshot.status);

    ImGui::Dummy(ImVec2(0.0f, hs::ui::SectionSpacing()));
    ImGui::TextUnformatted("Training vs MMR activity");
    RenderChartControls(uiState, chartData);
    RenderActivityChart(chartData, uiState.showTrainingOverlay, uiState.highlightMmrDelta);

    ImGui::Dummy(ImVec2(0.0f, hs::ui::SectionSpacing()));
    ImGui::Checkbox("Show daily comparison table", &uiState.showDailyComparison);
    RenderComparisonTable(comparisons, uiState.showDailyComparison);

    ImGui::Dummy(ImVec2(0.0f, hs::ui::SectionSpacing()));
    RenderMmrEntries(snapshot.mmrHistory);
    ImGui::Dummy(ImVec2(0.0f, hs::ui::SectionSpacing()));
    RenderTrainingEntries(snapshot.trainingHistory);
    ImGui::Dummy(ImVec2(0.0f, hs::ui::SectionSpacing()));
    RenderAggregates(snapshot.aggregates);

    ImGui::End();
}
