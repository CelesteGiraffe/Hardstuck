#pragma once

#include "IMGUI/imgui.h"

namespace hs::ui
{
    class ScopedStyle
    {
    public:
        ScopedStyle()
            : backup(ImGui::GetStyle())
        {
            ImGuiStyle& style = ImGui::GetStyle();

            style.WindowRounding    = 6.0f;
            style.FrameRounding     = 5.0f;
            style.GrabRounding      = 4.0f;
            style.ChildBorderSize   = 1.0f;
            style.PopupBorderSize   = 1.0f;
            style.WindowTitleAlign  = ImVec2(0.05f, 0.5f);
            style.FramePadding      = ImVec2(10.0f, 6.0f);
            style.ItemSpacing       = ImVec2(10.0f, 8.0f);
            style.ItemInnerSpacing  = ImVec2(8.0f, 4.0f);

            ImVec4* colors = style.Colors;
            colors[ImGuiCol_WindowBg]          = ImVec4(0.11f, 0.12f, 0.15f, 1.0f);
            colors[ImGuiCol_TitleBg]           = ImVec4(0.10f, 0.14f, 0.20f, 1.0f);
            colors[ImGuiCol_TitleBgActive]     = ImVec4(0.16f, 0.22f, 0.32f, 1.0f);
            colors[ImGuiCol_Border]            = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
            colors[ImGuiCol_FrameBg]           = ImVec4(0.18f, 0.20f, 0.24f, 1.0f);
            colors[ImGuiCol_FrameBgHovered]    = ImVec4(0.24f, 0.27f, 0.33f, 1.0f);
            colors[ImGuiCol_FrameBgActive]     = ImVec4(0.28f, 0.32f, 0.40f, 1.0f);
            colors[ImGuiCol_Button]            = ImVec4(0.20f, 0.45f, 0.68f, 1.0f);
            colors[ImGuiCol_ButtonHovered]     = ImVec4(0.23f, 0.52f, 0.77f, 1.0f);
            colors[ImGuiCol_ButtonActive]      = ImVec4(0.26f, 0.58f, 0.86f, 1.0f);
            colors[ImGuiCol_Header]            = ImVec4(0.16f, 0.36f, 0.53f, 1.0f);
            colors[ImGuiCol_HeaderHovered]     = ImVec4(0.20f, 0.42f, 0.63f, 1.0f);
            colors[ImGuiCol_HeaderActive]      = ImVec4(0.23f, 0.48f, 0.70f, 1.0f);
            colors[ImGuiCol_Text]              = ImVec4(0.90f, 0.94f, 1.00f, 1.0f);
            colors[ImGuiCol_TextDisabled]      = ImVec4(0.54f, 0.60f, 0.68f, 1.0f);
            colors[ImGuiCol_CheckMark]         = ImVec4(0.19f, 0.69f, 0.96f, 1.0f);
            colors[ImGuiCol_SliderGrab]        = ImVec4(0.22f, 0.48f, 0.68f, 1.0f);
            colors[ImGuiCol_SliderGrabActive]  = ImVec4(0.32f, 0.64f, 0.86f, 1.0f);
        }

        ScopedStyle(ScopedStyle const&) = delete;
        ScopedStyle& operator=(ScopedStyle const&) = delete;

        ~ScopedStyle()
        {
            ImGui::GetStyle() = backup;
        }

    private:
        ImGuiStyle backup;
    };

    inline ScopedStyle ApplyStyle()
    {
        return ScopedStyle();
    }

    inline float SectionSpacing()
    {
        return 8.0f;
    }

    inline ImVec2 PrimaryButtonSize()
    {
        return ImVec2(200.0f, 0.0f);
    }
}
