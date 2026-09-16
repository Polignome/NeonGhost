#pragma once
#include "imgui.h"
#define NG_USE_GUI_OLD_STYLE

#ifdef NG_USE_GUI_OLD_STYLE

static void SetupImGuiStyle()
{
    // Darcula style by ice1000 from ImThemes
    ImGuiStyle& style = ImGui::GetStyle();

    style.Alpha = 1.0f;
    style.DisabledAlpha = 0.6000000238418579f;
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.WindowRounding = 5.300000190734863f;
    style.WindowBorderSize = 1.0f;
    style.WindowMinSize = ImVec2(32.0f, 32.0f);
    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_Left;
    style.ChildRounding = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupRounding = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.FramePadding = ImVec2(4.0f, 3.0f);
    style.FrameRounding = 2.299999952316284f;
    style.FrameBorderSize = 1.0f;
    style.ItemSpacing = ImVec2(8.0f, 6.5f);
    style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
    style.CellPadding = ImVec2(4.0f, 2.0f);
    style.IndentSpacing = 21.0f;
    style.ColumnsMinSpacing = 6.0f;
    style.ScrollbarSize = 14.0f;
    style.ScrollbarRounding = 5.0f;
    style.GrabMinSize = 10.0f;
    style.GrabRounding = 2.299999952316284f;
    style.TabRounding = 4.0f;
    style.TabBorderSize = 0.0f;
    //   style.TabMinWidthForCloseButton = 0.0f;
    style.ColorButtonPosition = ImGuiDir_Right;
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

    style.Colors[ImGuiCol_Text] = ImVec4(0.7333333492279053f, 0.7333333492279053f, 0.7333333492279053f, 1.0f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.3450980484485626f, 0.3450980484485626f, 0.3450980484485626f, 1.0f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.2352941185235977f, 0.2470588237047195f, 0.2549019753932953f, 0.9399999976158142f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.2352941185235977f, 0.2470588237047195f, 0.2549019753932953f, 0.0f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.2352941185235977f, 0.2470588237047195f, 0.2549019753932953f, 0.9399999976158142f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.3333333432674408f, 0.3333333432674408f, 0.3333333432674408f, 0.5f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.1568627506494522f, 0.1568627506494522f, 0.1568627506494522f, 0.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.168627455830574f, 0.168627455830574f, 0.168627455830574f, 0.5400000214576721f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.4509803950786591f, 0.6745098233222961f, 0.9960784316062927f, 0.6700000166893005f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.4705882370471954f, 0.4705882370471954f, 0.4705882370471954f, 0.6700000166893005f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.03921568766236305f, 0.03921568766236305f, 0.03921568766236305f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.0f, 0.0f, 0.0f, 0.5099999904632568f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.1568627506494522f, 0.2862745225429535f, 0.47843137383461f, 1.0f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.2705882489681244f, 0.2862745225429535f, 0.2901960909366608f, 0.800000011920929f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.2705882489681244f, 0.2862745225429535f, 0.2901960909366608f, 0.6000000238418579f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.2196078449487686f, 0.3098039329051971f, 0.4196078479290009f, 0.5099999904632568f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.2196078449487686f, 0.3098039329051971f, 0.4196078479290009f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.1372549086809158f, 0.1921568661928177f, 0.2627451121807098f, 0.9100000262260437f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.8980392217636108f, 0.8980392217636108f, 0.8980392217636108f, 0.8299999833106995f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.6980392336845398f, 0.6980392336845398f, 0.6980392336845398f, 0.6200000047683716f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.2980392277240753f, 0.2980392277240753f, 0.2980392277240753f, 0.8399999737739563f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.3333333432674408f, 0.3529411852359772f, 0.3607843220233917f, 0.4900000095367432f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.2196078449487686f, 0.3098039329051971f, 0.4196078479290009f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.1372549086809158f, 0.1921568661928177f, 0.2627451121807098f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.3333333432674408f, 0.3529411852359772f, 0.3607843220233917f, 0.5299999713897705f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.4509803950786591f, 0.6745098233222961f, 0.9960784316062927f, 0.6700000166893005f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.4705882370471954f, 0.4705882370471954f, 0.4705882370471954f, 0.6700000166893005f);
    style.Colors[ImGuiCol_Separator] = ImVec4(0.3137255012989044f, 0.3137255012989044f, 0.3137255012989044f, 1.0f);
    style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.3137255012989044f, 0.3137255012989044f, 0.3137255012989044f, 1.0f);
    style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.3137255012989044f, 0.3137255012989044f, 0.3137255012989044f, 1.0f);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(1.0f, 1.0f, 1.0f, 0.8500000238418579f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.6000000238418579f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.8999999761581421f);

    style.Colors[ImGuiCol_Tab] = ImVec4(0.1764705926179886f, 0.3490196168422699f, 0.5764706134796143f, 0.8619999885559082f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 0.800000011920929f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.196078434586525f, 0.407843142747879f, 0.6784313917160034f, 1.0f);
    style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.06666667014360428f, 0.1019607856869698f, 0.1450980454683304f, 0.9724000096321106f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.1333333402872086f, 0.2588235437870026f, 0.4235294163227081f, 1.0f);

    style.Colors[ImGuiCol_Tab] = ImVec4(0.3490196168422699f, 0.3490196168422699f, 0.3490196168422699f, 0.8619999885559082f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 0.800000011920929f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.507843142747879f, 0.507843142747879f, 0.507843142747879f, 1.0f);
    style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.1450980454683304f, 0.1450980454683304f, 0.1450980454683304f, 0.9724000096321106f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.4235294163227081f, 0.4235294163227081f, 0.4235294163227081f, 1.0f);



    style.Colors[ImGuiCol_PlotLines] = ImVec4(0.6078431606292725f, 0.6078431606292725f, 0.6078431606292725f, 1.0f);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.0f, 0.4274509847164154f, 0.3490196168422699f, 1.0f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.8980392217636108f, 0.6980392336845398f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.0f, 0.6000000238418579f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.1882352977991104f, 0.1882352977991104f, 0.2000000029802322f, 1.0f);
    style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.3098039329051971f, 0.3098039329051971f, 0.3490196168422699f, 1.0f);
    style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.2274509817361832f, 0.2274509817361832f, 0.2470588237047195f, 1.0f);
    style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.05999999865889549f);
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.1843137294054031f, 0.3960784375667572f, 0.7921568751335144f, 0.8999999761581421f);
    style.Colors[ImGuiCol_DragDropTarget] = ImVec4(1.0f, 1.0f, 0.0f, 0.8999999761581421f);
    style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 1.0f);
    style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.699999988079071f);
    style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.800000011920929f, 0.800000011920929f, 0.800000011920929f, 0.2000000029802322f);
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.800000011920929f, 0.800000011920929f, 0.800000011920929f, 0.3499999940395355f);
}
#else

static inline ImVec4 Hex(unsigned int rgb, float a = 1.0f)
{
    return ImVec4(((rgb >> 16) & 0xFF) / 255.0f,
        ((rgb >> 8) & 0xFF) / 255.0f,
        (rgb & 0xFF) / 255.0f, a);
}

// ---------------------------------------------------------------
//  DarkGhostNeon – Palette direkt aus dem Logo (800x600)
// ---------------------------------------------------------------
namespace Pal
{
    // Hintergründe (Void / Deep Space)
    constexpr unsigned int Void = 0x000000; // Logo-Hintergrund
    constexpr unsigned int Abyss = 0x05070F; // fast schwarz, leicht blau
    constexpr unsigned int NightBlue = 0x0A1024; // Panel
    constexpr unsigned int DeepNavy = 0x0F1B3F; // Verlauf unten
    constexpr unsigned int MidnightBlue = 0x000F3F; // Kern-Glow außen
    constexpr unsigned int RoyalNavy = 0x00003F;
    constexpr unsigned int Indigo = 0x1F1F7F; // Wireframe dunkel
    constexpr unsigned int SteelBlue = 0x1F4F7F; // Scanlines
    constexpr unsigned int Cobalt = 0x001F7F;
    constexpr unsigned int SlateBlue = 0x3F3F7F; // Borders / Grid

    // Mitteltöne
    constexpr unsigned int NeonBlue = 0x2F53BF; // Hauptblau
    constexpr unsigned int SkyGlow = 0x7F9FFF; // heller Rand des Ghosts
    constexpr unsigned int IceBlue = 0x7FBFFF; // Highlight

    // Neon-Akzente
    constexpr unsigned int Cyan = 0x00E5FF; // "SOFTWARE RENDERER"
    constexpr unsigned int Violet = 0xBF7FFF; // Ghost-Aura
    constexpr unsigned int Magenta = 0xDF7FFF; // Neon-Kanten
    constexpr unsigned int HotPink = 0xFF4FD8; // Warn/Aktiv

    // Text
    constexpr unsigned int TextBright = 0xE6F3FF;
    constexpr unsigned int TextDim = 0x8FA6C4;
}

static void SetupImGuiStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();

    // ---- Layout (aus deiner Darcula-Vorlage übernommen) ----
    style.Alpha = 1.0f;
    style.DisabledAlpha = 0.60f;
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.WindowRounding = 5.3f;
    style.WindowBorderSize = 1.0f;
    style.WindowMinSize = ImVec2(32.0f, 32.0f);
    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_Left;
    style.ChildRounding = 4.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupRounding = 4.0f;
    style.PopupBorderSize = 1.0f;
    style.FramePadding = ImVec2(6.0f, 4.0f);
    style.FrameRounding = 4.0f;
    style.FrameBorderSize = 1.0f;
    style.ItemSpacing = ImVec2(8.0f, 4.0f);
    style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
    style.CellPadding = ImVec2(4.0f, 2.0f);
    style.IndentSpacing = 21.0f;
    style.ColumnsMinSpacing = 6.0f;
    style.ScrollbarSize = 13.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabMinSize = 10.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 4.0f;
    style.TabBorderSize = 1.0f;
    style.ColorButtonPosition = ImGuiDir_Right;
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

    ImVec4* c = style.Colors;

    // ---- Text ----
    c[ImGuiCol_Text] = Hex(Pal::TextBright);
    c[ImGuiCol_TextDisabled] = Hex(Pal::TextDim, 0.65f);
    c[ImGuiCol_TextSelectedBg] = Hex(Pal::Cyan, 0.35f);

    // ---- Flächen ----
    c[ImGuiCol_WindowBg] = Hex(Pal::Abyss, 1.00f);
    c[ImGuiCol_ChildBg] = Hex(Pal::Void, 0.35f);
    c[ImGuiCol_PopupBg] = Hex(Pal::NightBlue, 0.98f);
    c[ImGuiCol_MenuBarBg] = Hex(Pal::DeepNavy, 1.00f);

    // ---- Rahmen ----
    c[ImGuiCol_Border] = Hex(Pal::SlateBlue, 0.85f);
    c[ImGuiCol_BorderShadow] = Hex(Pal::Violet, 0.10f);

    // ---- Frames (Inputs, Combos, Checkbox) ----
    c[ImGuiCol_FrameBg] = Hex(Pal::MidnightBlue, 0.85f);
    c[ImGuiCol_FrameBgHovered] = Hex(Pal::Cobalt, 0.90f);
    c[ImGuiCol_FrameBgActive] = Hex(Pal::NeonBlue, 0.85f);

    // ---- Titelleisten ----
    c[ImGuiCol_TitleBg] = Hex(Pal::RoyalNavy, 1.00f);
    c[ImGuiCol_TitleBgActive] = Hex(Pal::Indigo, 1.00f);
    c[ImGuiCol_TitleBgCollapsed] = Hex(Pal::Void, 0.75f);

    // ---- Scrollbar ----
    c[ImGuiCol_ScrollbarBg] = Hex(Pal::Void, 0.60f);
    c[ImGuiCol_ScrollbarGrab] = Hex(Pal::SteelBlue, 0.90f);
    c[ImGuiCol_ScrollbarGrabHovered] = Hex(Pal::SkyGlow, 0.85f);
    c[ImGuiCol_ScrollbarGrabActive] = Hex(Pal::Cyan, 0.95f);

    // ---- Interaktion ----
    c[ImGuiCol_CheckMark] = Hex(Pal::Cyan);
    c[ImGuiCol_SliderGrab] = Hex(Pal::Violet, 0.90f);
    c[ImGuiCol_SliderGrabActive] = Hex(Pal::Magenta);

    c[ImGuiCol_Button] = Hex(Pal::Indigo, 0.90f);
    c[ImGuiCol_ButtonHovered] = Hex(Pal::NeonBlue, 1.00f);
    c[ImGuiCol_ButtonActive] = Hex(Pal::Violet, 1.00f);

    c[ImGuiCol_Header] = Hex(Pal::Cobalt, 0.75f);
    c[ImGuiCol_HeaderHovered] = Hex(Pal::NeonBlue, 0.85f);
    c[ImGuiCol_HeaderActive] = Hex(Pal::Magenta, 0.75f);

    // ---- Separator / Resize ----
    c[ImGuiCol_Separator] = Hex(Pal::SlateBlue, 0.80f);
    c[ImGuiCol_SeparatorHovered] = Hex(Pal::IceBlue, 0.85f);
    c[ImGuiCol_SeparatorActive] = Hex(Pal::Cyan, 1.00f);

    c[ImGuiCol_ResizeGrip] = Hex(Pal::Violet, 0.35f);
    c[ImGuiCol_ResizeGripHovered] = Hex(Pal::Magenta, 0.70f);
    c[ImGuiCol_ResizeGripActive] = Hex(Pal::HotPink, 0.95f);

    // ---- Tabs (nur EINMAL gesetzt – Bug deiner Vorlage) ----
    c[ImGuiCol_Tab] = Hex(Pal::DeepNavy, 1.00f);
    c[ImGuiCol_TabHovered] = Hex(Pal::NeonBlue, 1.00f);
    c[ImGuiCol_TabActive] = Hex(Pal::Indigo, 1.00f);
    c[ImGuiCol_TabUnfocused] = Hex(Pal::Abyss, 1.00f);
    c[ImGuiCol_TabUnfocusedActive] = Hex(Pal::MidnightBlue, 1.00f);

    // ---- Docking ----
#ifdef IMGUI_HAS_DOCK
    c[ImGuiCol_DockingPreview] = Hex(Pal::Cyan, 0.45f);
    c[ImGuiCol_DockingEmptyBg] = Hex(Pal::Void, 1.00f);
#endif

    // ---- Plots (Frametimes / Rasterizer-Stats) ----
    c[ImGuiCol_PlotLines] = Hex(Pal::IceBlue);
    c[ImGuiCol_PlotLinesHovered] = Hex(Pal::Cyan);
    c[ImGuiCol_PlotHistogram] = Hex(Pal::Violet);
    c[ImGuiCol_PlotHistogramHovered] = Hex(Pal::HotPink);

    // ---- Tabellen ----
    c[ImGuiCol_TableHeaderBg] = Hex(Pal::Indigo, 1.00f);
    c[ImGuiCol_TableBorderStrong] = Hex(Pal::SlateBlue, 1.00f);
    c[ImGuiCol_TableBorderLight] = Hex(Pal::SteelBlue, 0.60f);
    c[ImGuiCol_TableRowBg] = Hex(Pal::Void, 0.00f);
    c[ImGuiCol_TableRowBgAlt] = Hex(Pal::SkyGlow, 0.05f);

    // ---- Drag & Drop / Navigation / Modal ----
    c[ImGuiCol_DragDropTarget] = Hex(Pal::Magenta, 0.95f);
    c[ImGuiCol_NavHighlight] = Hex(Pal::Cyan, 1.00f);
    c[ImGuiCol_NavWindowingHighlight] = Hex(Pal::SkyGlow, 0.70f);
    c[ImGuiCol_NavWindowingDimBg] = Hex(Pal::MidnightBlue, 0.40f);
    c[ImGuiCol_ModalWindowDimBg] = Hex(Pal::Void, 0.65f);
}
#endif