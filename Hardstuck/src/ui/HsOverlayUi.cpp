#include "pch.h"
#include "ui/HsOverlayUi.h"

#include "ui/ui_style.h"

#if __has_include("bakkesmod/wrappers/cvarmanagerwrapper.h")
#include "bakkesmod/wrappers/cvarmanagerwrapper.h"
#endif

#if __has_include("imgui.h")
#include "imgui.h"
#elif __has_include("imgui/imgui.h")
#include "imgui/imgui.h"
#endif

void HsRenderOverlayUi(
    CVarManagerWrapper* cvarManager,
    const std::string& lastResponse,
    const std::string& lastError,
    const std::string& activeSessionLabel,
    bool manualSessionActive,
    HsTriggerManualUploadFn triggerManualUpload,
    HsExecuteHistoryWindowFn executeHistoryWindowCommand
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
        manualSessionActive ? " (manual)" : "");

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

    if (ImGui::Button("Gather && Upload Now"))
    {
        if (triggerManualUpload)
        {
            triggerManualUpload();
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
