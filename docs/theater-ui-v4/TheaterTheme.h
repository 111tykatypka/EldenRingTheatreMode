#pragma once
// ============================================================================
// TheaterTheme.h
// Elden Ring Theater Mode - design tokens, layout constants and ImGui style.
//
// Single source of truth for every colour, size and font role used by
// TheaterUI. All pixel values are authored at the primary target
// (2560x1440, UiScale = 1.0) and multiplied by Theme::Scale() at runtime.
//
// Requires Dear ImGui 1.89+ (compile-checked against 1.93 WIP).
// ============================================================================

#include <algorithm>
#include <cstdint>

#include "imgui.h"

namespace TheaterUI::Theme
{
    // ------------------------------------------------------------------------
    // Colour primitive
    // ------------------------------------------------------------------------
    struct Rgba
    {
        uint8_t r, g, b, a;

        constexpr ImU32 U32() const { return IM_COL32(r, g, b, a); }
        ImVec4 Vec4() const { return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f); }
        constexpr Rgba Alpha(uint8_t na) const { return { r, g, b, na }; }
        // Multiplies the existing alpha (0..1). Used for disabled states.
        constexpr Rgba Fade(float f) const { return { r, g, b, (uint8_t)(a * f) }; }
    };

    // ------------------------------------------------------------------------
    // COLORS
    // Neutrals carry a slight cool (blue-grey) bias so they sit with the
    // blue accent. Panels are semi-transparent over the game viewport.
    // ------------------------------------------------------------------------
    namespace Color
    {
        // Backgrounds
        inline constexpr Rgba AppBg         { 10,  11,  13, 255 }; // behind everything (letterbox, empty dock)
        inline constexpr Rgba PanelBg       { 15,  17,  20, 232 }; // ~91% opaque editor panel
        inline constexpr Rgba PanelBgSolid  { 15,  17,  20, 255 }; // panel when "Opaque panels" is on
        inline constexpr Rgba ChildBg       { 19,  21,  25, 214 }; // nested region (track tree, inspector body)
        inline constexpr Rgba HeaderBg      { 22,  25,  29, 240 }; // panel header strip, section header
        inline constexpr Rgba FrameBg       { 27,  30,  35, 255 }; // inputs, sliders, search
        inline constexpr Rgba HoverBg       { 34,  38,  44, 255 };
        inline constexpr Rgba ActiveBg      { 42,  47,  55, 255 }; // pressed
        inline constexpr Rgba SelectedBg    { 33,  52,  78, 255 }; // selected row / active tab
        inline constexpr Rgba SelectedBgDim { 33,  52,  78, 140 }; // selected row in unfocused panel
        inline constexpr Rgba OverlayBg     {  8,   9,  11, 168 }; // viewport overlay chips, toasts
        inline constexpr Rgba ModalDim      {  0,   0,   0, 120 }; // behind modal

        // Borders
        inline constexpr Rgba Border        { 44,  48,  55, 255 }; // 1px normal
        inline constexpr Rgba BorderSubtle  { 31,  34,  39, 255 }; // row separators, grid
        inline constexpr Rgba BorderStrong  { 60,  66,  75, 255 }; // focused panel outline
        inline constexpr Rgba BorderActive  { 82, 138, 200, 255 }; // focused input, active tool
        inline constexpr Rgba BorderWarning { 190, 142,  62, 255 };
        inline constexpr Rgba BorderError   { 190,  80,  74, 255 };

        // Text
        inline constexpr Rgba TextPrimary   { 221, 224, 229, 255 };
        inline constexpr Rgba TextSecondary { 158, 164, 173, 255 };
        inline constexpr Rgba TextMuted     { 106, 112, 122, 255 };
        inline constexpr Rgba TextDisabled  {  70,  75,  83, 255 };
        inline constexpr Rgba TextOnAccent  { 245, 247, 250, 255 };

        // Accents (deliberately desaturated - nothing neon)
        inline constexpr Rgba AccentBlue    {  86, 148, 214, 255 }; // camera, selection, focus
        inline constexpr Rgba AccentAmber   { 208, 160,  74, 255 }; // world / environment, warnings
        inline constexpr Rgba AccentGreen   { 102, 172, 118, 255 }; // player actions, success, READY
        inline constexpr Rgba AccentRed     { 200,  86,  80, 255 }; // damage, death, record, error
        inline constexpr Rgba AccentPurple  { 148, 120, 198, 255 }; // pose / animation
        inline constexpr Rgba AccentGold    { 214, 190, 128, 255 }; // playhead + brand mark only

        // Tinted fills for badges (accent at low alpha over PanelBg)
        inline constexpr Rgba TintBlue      {  86, 148, 214,  38 };
        inline constexpr Rgba TintAmber     { 208, 160,  74,  38 };
        inline constexpr Rgba TintGreen     { 102, 172, 118,  38 };
        inline constexpr Rgba TintRed       { 200,  86,  80,  44 };
        inline constexpr Rgba TintPurple    { 148, 120, 198,  38 };
        inline constexpr Rgba TintNeutral   { 158, 164, 173,  26 };

        // Timeline
        inline constexpr Rgba TimelineBg        { 13,  15,  18, 236 };
        inline constexpr Rgba TimelineRowAlt    { 255, 255, 255,   6 }; // zebra on every other track
        inline constexpr Rgba RulerBg           { 18,  20,  24, 255 };
        inline constexpr Rgba RulerTickMajor    { 118, 124, 134, 255 };
        inline constexpr Rgba RulerTickMinor    {  58,  63,  71, 255 };
        inline constexpr Rgba RulerText         { 138, 144, 153, 255 };
        inline constexpr Rgba GridLine          { 255, 255, 255,  10 }; // vertical lines at major ticks
        inline constexpr Rgba Playhead          { 214, 190, 128, 255 }; // = AccentGold
        inline constexpr Rgba PlayheadLabelBg   { 214, 190, 128, 255 };
        inline constexpr Rgba PlayheadLabelText {  14,  15,  17, 255 };
        inline constexpr Rgba SelectionFill     {  86, 148, 214,  30 };
        inline constexpr Rgba SelectionEdge     {  86, 148, 214, 150 };
        inline constexpr Rgba InOutFill         { 255, 255, 255,   8 }; // outside Mark In/Out is dimmed with this
        inline constexpr Rgba InOutDim          {   0,   0,   0,  90 };
        inline constexpr Rgba Keyframe          { 196, 201, 209, 255 };
        inline constexpr Rgba KeyframeSelected  { 236, 214, 150, 255 };
        inline constexpr Rgba KeyframeOutline   {  10,  11,  13, 255 };
        inline constexpr Rgba RangeBar          { 255, 255, 255,  22 }; // clip / section bar body

        // Event marker colours (see MarkerColor())
        inline constexpr Rgba EventCamera   = AccentBlue;
        inline constexpr Rgba EventPlayer   = AccentGreen;
        inline constexpr Rgba EventDamage   = AccentRed;
        inline constexpr Rgba EventWorld    = AccentAmber;
        inline constexpr Rgba EventPose     = AccentPurple;
        inline constexpr Rgba EventNeutral  { 150, 156, 165, 255 }; // bookmark, spawn/despawn
    }

    // ------------------------------------------------------------------------
    // SPACING (px @ 1440p)
    // ------------------------------------------------------------------------
    namespace Space
    {
        inline constexpr float XS = 2.0f;
        inline constexpr float SM = 4.0f;
        inline constexpr float MD = 8.0f;
        inline constexpr float LG = 12.0f;
        inline constexpr float XL = 16.0f;
    }

    // ------------------------------------------------------------------------
    // RADIUS (px @ 1440p) - minimal by design
    // ------------------------------------------------------------------------
    namespace Radius
    {
        inline constexpr float Panel  = 2.0f;
        inline constexpr float Button = 2.0f;
        inline constexpr float Input  = 2.0f;
        inline constexpr float Badge  = 2.0f;
        inline constexpr float Toast  = 3.0f;
        inline constexpr float None   = 0.0f; // panels docked to screen edges
    }

    // ------------------------------------------------------------------------
    // BORDERS
    // ------------------------------------------------------------------------
    namespace Stroke
    {
        inline constexpr float Hairline = 1.0f;   // never scaled below 1px
        inline constexpr float Playhead = 1.0f;
        inline constexpr float Focus    = 1.0f;
        inline constexpr float ToolAccentStrip = 2.0f; // active tool rail indicator
    }

    // ------------------------------------------------------------------------
    // TYPOGRAPHY (px @ 1440p). UI = Inter, Mono = JetBrains Mono.
    // Icons = Material Symbols Rounded (filled) merged into the UI fonts.
    // ------------------------------------------------------------------------
    enum class Font : int
    {
        Body = 0,     // Inter Regular       14 px  - labels, rows, buttons
        BodyStrong,   // Inter SemiBold      14 px  - selected names, identity row
        PanelTitle,   // Inter SemiBold      12 px  - UPPERCASE, +0.10em tracking
        Meta,         // Inter Regular       12 px  - hints, units
        RailLabel,    // Inter Medium        10.5 px - label under each rail icon
        Mono,         // JetBrains Mono      12.5 px - technical values, ids, coordinates
        MonoSmall,    // JetBrains Mono      11 px  - ruler labels, event log
        Timecode,     // JetBrains Mono Med  18 px  - sequencer timecode
        Count
    };

    namespace FontSize
    {
        inline constexpr float Body       = 14.0f;
        inline constexpr float PanelTitle = 12.0f;
        inline constexpr float Meta       = 12.0f;
        inline constexpr float RailLabel  = 10.5f;
        inline constexpr float Mono       = 12.5f;
        inline constexpr float MonoSmall  = 11.0f;
        inline constexpr float Timecode   = 18.0f;
        inline constexpr float Icon       = 18.0f; // in 30 px buttons
        inline constexpr float IconSmall  = 16.0f; // in rows and 24 px buttons
        inline constexpr float IconRail   = 22.0f; // in the 52 px rail buttons
    }

    // ------------------------------------------------------------------------
    // COMPONENT METRICS (px @ 1440p)
    // ------------------------------------------------------------------------
    namespace Metric
    {
        inline constexpr float SpeedSliderWidth   = 150.0f; // log 0.01x-4x
        inline constexpr float LoadCardWidth      = 440.0f;
        inline constexpr float RailButton         = 52.0f; // square, icon 22 + label 10.5
        inline constexpr float PanelHeaderHeight  = 34.0f;
        inline constexpr float SectionHeaderHeight= 28.0f;
        inline constexpr float PropertyRowHeight  = 28.0f;
        inline constexpr float TreeRowHeight      = 28.0f;
        inline constexpr float ListRowHeight      = 30.0f; // camera keys
        inline constexpr float LogLineHeight      = 20.0f;
        inline constexpr float TrackRowHeight     = 28.0f; // v3: was 26
        inline constexpr float IconButtonSize     = 30.0f;
        inline constexpr float IconButtonSmall    = 24.0f;
        inline constexpr float TextButtonHeight   = 28.0f;
        inline constexpr float InputHeight        = 24.0f;
        inline constexpr float BadgeHeight        = 20.0f;
        inline constexpr float StatusDot          = 7.0f;
        inline constexpr float IndentPerLevel     = 16.0f;
        inline constexpr float PropertyLabelFrac  = 0.38f;
        inline constexpr float KeyframeSize       = 10.0f;
        inline constexpr float EventMarkerWidth   = 3.0f;
        inline constexpr float EventMarkerHeight  = 14.0f;
        inline constexpr float RulerMajorTick     = 10.0f;
        inline constexpr float RulerHalfTick      = 2.0f;  // 0.5 s ticks when the view spans <= 40 s
        inline constexpr float PlayheadWidth      = 2.0f;
        inline constexpr float KeySegmentHeight   = 2.0f;  // line between keys = interpolated span
        inline constexpr float GizmoArrowLength   = 78.0f; // camera key gizmo in the viewport
        inline constexpr float GizmoHandleRadius  = 7.0f;
        inline constexpr float RulerMinorTick     = 4.0f;
        inline constexpr float SplitterThickness  = 6.0f;  // hit area; draws as 1px line
        inline constexpr float ToastHeight        = 34.0f;
        inline constexpr float RecPillHeight      = 34.0f;
        inline constexpr float ScrollbarSize      = 8.0f;
        inline constexpr float CornerRadius       = 3.0f;
    }

    // ------------------------------------------------------------------------
    // LAYOUT CONSTANTS (logical px, i.e. @ UiScale 1.0). See TheaterLayout.h
    // for the solver that turns these into rects for any resolution.
    // ------------------------------------------------------------------------
    struct LayoutDefaults
    {
        float MenuBarHeight      = 28.0f;   // v4: File / Edit / Layout / Help, the only row above the editor
        float RailWidth          = 64.0f;
        float PanelWidth         = 340.0f;  float PanelMin = 300.0f;  float PanelMax = 440.0f;  float PanelHardMax = 640.0f; // takes leftover width when no Selection column fits
        float SelectionWidth     = 360.0f;  float SelectionMin = 280.0f; // right column, only when width is spare
        float SequencerMin       = 300.0f;  /* v4: was 260; 7 rows at 1440p, lanes scroll for the rest */  float SequencerMaxFraction = 0.50f;
        float SequencerToolbar   = 40.0f;
        float RulerHeight        = 24.0f;
        float NavigatorHeight    = 16.0f;   // overview strip under the lanes, doubles as the h-scrollbar
        float TrackTreeWidth     = 250.0f;  // v4: room for group icon + name + count
    };
    inline constexpr LayoutDefaults Layout{};

    // ------------------------------------------------------------------------
    // SCALE
    // Uses the smaller of the width and height ratios so ultrawide (2560x1100)
    // and tall (2560x1600) displays both stay readable. Floor 0.8 keeps 14 px
    // body text at ~11 px on 1080p and 810p.
    // 2560x1440 -> 1.00, 1920x1080 -> 0.80, 3840x2160 -> 1.50,
    // 2560x1100 -> 0.80, 3440x1440 -> 1.00, 1920x810 -> 0.80
    // ------------------------------------------------------------------------
    inline float ComputeUiScale(float displayW, float displayH, float userMultiplier = 1.0f)
    {
        const float s = std::clamp(std::min(displayH / 1440.0f, displayW / 2560.0f), 0.80f, 2.0f);
        return s * std::clamp(userMultiplier, 0.75f, 1.5f);
    }

    // Snap a scaled length to whole pixels so 1px borders stay crisp.
    inline float Px(float valueAt1440, float uiScale) { return (float)(int)(valueAt1440 * uiScale + 0.5f); }

    // ------------------------------------------------------------------------
    // SEMANTIC TONES (badges, toasts, status dots, property warnings)
    // ------------------------------------------------------------------------
    enum class Tone : uint8_t { Neutral, Info, Success, Warning, Error, Accent /*purple*/, Live /*red, recording*/ };

    struct ToneColors { Rgba fg, fill, border; };

    inline ToneColors ColorsFor(Tone t)
    {
        using namespace Color;
        switch (t)
        {
        case Tone::Info:    return { AccentBlue,    TintBlue,    AccentBlue.Alpha(110) };
        case Tone::Success: return { AccentGreen,   TintGreen,   AccentGreen.Alpha(110) };
        case Tone::Warning: return { AccentAmber,   TintAmber,   BorderWarning };
        case Tone::Error:   return { AccentRed,     TintRed,     BorderError };
        case Tone::Accent:  return { AccentPurple,  TintPurple,  AccentPurple.Alpha(110) };
        case Tone::Live:    return { AccentRed,     TintRed,     AccentRed.Alpha(140) };
        case Tone::Neutral:
        default:            return { TextSecondary, TintNeutral, Border };
        }
    }

    // ------------------------------------------------------------------------
    // TIMELINE MARKER TYPES
    // ------------------------------------------------------------------------
    enum class MarkerType : uint8_t
    {
        CameraKey, PlayerAction, Roll, Attack, Jump, Hit, Death,
        Spawn, Despawn, Effect, WorldChange, Bookmark, PoseKey
    };

    enum class MarkerShape : uint8_t { Diamond, Tick, Triangle, Square, Flag, Circle };

    struct MarkerStyle { Rgba color; MarkerShape shape; };

    inline MarkerStyle MarkerFor(MarkerType m)
    {
        using namespace Color;
        switch (m)
        {
        case MarkerType::CameraKey:    return { EventCamera,  MarkerShape::Diamond  };
        case MarkerType::PlayerAction: return { EventPlayer,  MarkerShape::Tick     };
        case MarkerType::Roll:         return { EventPlayer,  MarkerShape::Tick     };
        case MarkerType::Attack:       return { EventPlayer,  MarkerShape::Triangle };
        case MarkerType::Jump:         return { EventPlayer,  MarkerShape::Tick     };
        case MarkerType::Hit:          return { EventDamage,  MarkerShape::Triangle };
        case MarkerType::Death:        return { EventDamage,  MarkerShape::Square   };
        case MarkerType::Spawn:        return { EventNeutral, MarkerShape::Circle   };
        case MarkerType::Despawn:      return { EventNeutral, MarkerShape::Circle   };
        case MarkerType::Effect:       return { EventPose,    MarkerShape::Tick     };
        case MarkerType::WorldChange:  return { EventWorld,   MarkerShape::Square   };
        case MarkerType::Bookmark:     return { EventNeutral, MarkerShape::Flag     };
        case MarkerType::PoseKey:      return { EventPose,    MarkerShape::Diamond  };
        }
        return { EventNeutral, MarkerShape::Tick };
    }

    // ------------------------------------------------------------------------
    // ImGui style application. Call once at init and again whenever the
    // display height or user UI-scale changes (then rebuild fonts too).
    // ------------------------------------------------------------------------
    inline void ApplyStyle(ImGuiStyle& s, float uiScale)
    {
        s = ImGuiStyle(); // reset before ScaleAllSizes so repeated calls don't compound

        s.WindowPadding     = ImVec2(Space::MD, Space::MD);
        s.FramePadding      = ImVec2(8.0f, 4.0f);
        s.CellPadding       = ImVec2(Space::SM + 2.0f, 2.0f);
        s.ItemSpacing       = ImVec2(Space::SM + 2.0f, Space::SM);
        s.ItemInnerSpacing  = ImVec2(Space::SM, Space::SM);
        s.IndentSpacing     = Metric::IndentPerLevel;
        s.GrabMinSize       = 10.0f;
        s.ScrollbarSize     = Metric::ScrollbarSize;
        s.GrabMinSize       = 8.0f;

        s.WindowBorderSize  = Stroke::Hairline;
        s.ChildBorderSize   = Stroke::Hairline;
        s.FrameBorderSize   = Stroke::Hairline;
        s.PopupBorderSize   = Stroke::Hairline;
        s.TabBorderSize     = 0.0f;

        s.WindowRounding    = Radius::None;
        s.ChildRounding     = Radius::Panel;
        s.FrameRounding     = Metric::CornerRadius;
        s.PopupRounding     = Radius::Panel;
        s.ScrollbarRounding = Radius::Panel;
        s.GrabRounding      = Radius::Input;
        s.TabRounding       = Radius::Button;

        s.WindowTitleAlign  = ImVec2(0.0f, 0.5f);
        s.WindowMenuButtonPosition = ImGuiDir_None;
        s.SeparatorTextBorderSize = 1.0f;
        s.AntiAliasedLines  = true;
        s.AntiAliasedFill   = true;

        s.ScaleAllSizes(uiScale);
        // Keep hairlines at exactly 1px whatever the scale.
        s.WindowBorderSize = s.ChildBorderSize = s.FrameBorderSize = s.PopupBorderSize = 1.0f;

        using namespace Color;
        ImVec4* c = s.Colors;
        c[ImGuiCol_Text]                  = TextPrimary.Vec4();
        c[ImGuiCol_TextDisabled]          = TextDisabled.Vec4();
        c[ImGuiCol_WindowBg]              = PanelBg.Vec4();
        c[ImGuiCol_ChildBg]               = Rgba{ 0, 0, 0, 0 }.Vec4(); // children inherit panel; opt-in ChildBg explicitly
        c[ImGuiCol_PopupBg]               = PanelBgSolid.Vec4();
        c[ImGuiCol_Border]                = Border.Vec4();
        c[ImGuiCol_BorderShadow]          = Rgba{ 0, 0, 0, 0 }.Vec4();
        c[ImGuiCol_FrameBg]               = FrameBg.Vec4();
        c[ImGuiCol_FrameBgHovered]        = HoverBg.Vec4();
        c[ImGuiCol_FrameBgActive]         = ActiveBg.Vec4();
        c[ImGuiCol_TitleBg]               = HeaderBg.Vec4();
        c[ImGuiCol_TitleBgActive]         = HeaderBg.Vec4();
        c[ImGuiCol_TitleBgCollapsed]      = HeaderBg.Vec4();
        c[ImGuiCol_MenuBarBg]             = HeaderBg.Vec4();
        c[ImGuiCol_ScrollbarBg]           = Rgba{ 0, 0, 0, 0 }.Vec4();
        c[ImGuiCol_ScrollbarGrab]         = Border.Vec4();
        c[ImGuiCol_ScrollbarGrabHovered]  = BorderStrong.Vec4();
        c[ImGuiCol_ScrollbarGrabActive]   = TextMuted.Vec4();
        c[ImGuiCol_CheckMark]             = AccentBlue.Vec4();
        c[ImGuiCol_SliderGrab]            = TextSecondary.Vec4();
        c[ImGuiCol_SliderGrabActive]      = AccentBlue.Vec4();
        c[ImGuiCol_Button]                = Rgba{ 0, 0, 0, 0 }.Vec4();   // icon buttons are flat by default
        c[ImGuiCol_ButtonHovered]         = HoverBg.Vec4();
        c[ImGuiCol_ButtonActive]          = ActiveBg.Vec4();
        c[ImGuiCol_Header]                = SelectedBg.Vec4();           // Selectable / TreeNode selected
        c[ImGuiCol_HeaderHovered]         = HoverBg.Vec4();
        c[ImGuiCol_HeaderActive]          = ActiveBg.Vec4();
        c[ImGuiCol_Separator]             = BorderSubtle.Vec4();
        c[ImGuiCol_SeparatorHovered]      = BorderActive.Vec4();
        c[ImGuiCol_SeparatorActive]       = BorderActive.Vec4();
        c[ImGuiCol_ResizeGrip]            = Rgba{ 0, 0, 0, 0 }.Vec4();
        c[ImGuiCol_ResizeGripHovered]     = BorderActive.Alpha(120).Vec4();
        c[ImGuiCol_ResizeGripActive]      = BorderActive.Vec4();
        c[ImGuiCol_Tab]                   = Rgba{ 0, 0, 0, 0 }.Vec4();
        c[ImGuiCol_TabHovered]            = HoverBg.Vec4();
        c[ImGuiCol_PlotLines]             = AccentBlue.Vec4();
        c[ImGuiCol_PlotLinesHovered]      = AccentGold.Vec4();
        c[ImGuiCol_PlotHistogram]         = AccentBlue.Vec4();
        c[ImGuiCol_PlotHistogramHovered]  = AccentGold.Vec4();
        c[ImGuiCol_TableHeaderBg]         = HeaderBg.Vec4();
        c[ImGuiCol_TableBorderStrong]     = Border.Vec4();
        c[ImGuiCol_TableBorderLight]      = BorderSubtle.Vec4();
        c[ImGuiCol_TableRowBg]            = Rgba{ 0, 0, 0, 0 }.Vec4();
        c[ImGuiCol_TableRowBgAlt]         = TimelineRowAlt.Vec4();
        c[ImGuiCol_TextSelectedBg]        = AccentBlue.Alpha(90).Vec4();
        c[ImGuiCol_DragDropTarget]        = AccentGold.Vec4();
        c[ImGuiCol_NavWindowingHighlight] = TextPrimary.Alpha(180).Vec4();
        c[ImGuiCol_NavWindowingDimBg]     = ModalDim.Vec4();
        c[ImGuiCol_ModalWindowDimBg]      = ModalDim.Vec4();
        // ImGuiCol_TabActive / TabSelected and NavHighlight / NavCursor were renamed
        // across 1.90-1.91; set them in TheaterUI::Init() for your ImGui version.
    }

    // ------------------------------------------------------------------------
    // Font atlas. Bake once per UiScale change (ImGui < 1.92), or load once
    // and use PushFont(font, size) on 1.92+ dynamic fonts.
    // ------------------------------------------------------------------------
    struct FontPaths
    {
        const char* uiRegular  = "TheaterUI/Inter-Regular.ttf";
        const char* uiSemiBold = "TheaterUI/Inter-SemiBold.ttf";
        const char* mono       = "TheaterUI/JetBrainsMono-Regular.ttf";
        const char* monoMedium = "TheaterUI/JetBrainsMono-Medium.ttf";
        const char* uiMedium   = "TheaterUI/Inter-Medium.ttf";
        // Material Symbols Rounded (filled, weight 500), merged into Body/BodyStrong/PanelTitle.
        // Most glyphs sit in U+E000-U+F8FF. A few newer ones are above U+FFFF and need
        // IMGUI_USE_WCHAR32; the IconSet in TheaterUITypes.h only uses BMP glyphs.
        const char* icons      = "TheaterUI/MaterialSymbolsRounded-Filled.ttf";
        ImWchar iconMin = 0xE000, iconMax = 0xF8FF;
    };

    struct Fonts { ImFont* f[(int)Font::Count] = {}; ImFont* operator[](Font r) const { return f[(int)r]; } };

    inline Fonts LoadFonts(ImGuiIO& io, float uiScale, const FontPaths& p = {})
    {
        Fonts out;
        io.Fonts->Clear();

        ImFontConfig base;
        base.OversampleH = 2;
        base.OversampleV = 1;
        base.PixelSnapH  = true;

        static ImWchar iconRanges[3];
        iconRanges[0] = p.iconMin; iconRanges[1] = p.iconMax; iconRanges[2] = 0;

        auto addUi = [&](const char* path, float px, bool mergeIcons) -> ImFont*
        {
            ImFont* f = io.Fonts->AddFontFromFileTTF(path, Px(px, uiScale), &base);
            if (mergeIcons && p.icons)
            {
                ImFontConfig ic;
                ic.MergeMode = true;
                ic.PixelSnapH = true;
                ic.GlyphMinAdvanceX = Px(px + 4.0f, uiScale);
                ic.GlyphOffset = ImVec2(0, Px(3.0f, uiScale)); // Material Symbols sit high; centre on the text line
                io.Fonts->AddFontFromFileTTF(p.icons, Px(px + 4.0f, uiScale), &ic, iconRanges);
            }
            return f;
        };

        out.f[(int)Font::Body]       = addUi(p.uiRegular,  FontSize::Body,       true);
        out.f[(int)Font::BodyStrong] = addUi(p.uiSemiBold, FontSize::Body,       true);
        out.f[(int)Font::PanelTitle] = addUi(p.uiSemiBold, FontSize::PanelTitle, true);
        out.f[(int)Font::Meta]       = addUi(p.uiRegular,  FontSize::Meta,       false);
        out.f[(int)Font::RailLabel]  = addUi(p.uiMedium,   FontSize::RailLabel,  false);
        out.f[(int)Font::Mono]       = addUi(p.mono,       FontSize::Mono,       false);
        out.f[(int)Font::MonoSmall]  = addUi(p.mono,       FontSize::MonoSmall,  false);
        out.f[(int)Font::Timecode]   = addUi(p.monoMedium, FontSize::Timecode,   false);

        io.FontDefault = out.f[(int)Font::Body];
        // DX12 backend: call ImGui_ImplDX12_InvalidateDeviceObjects() +
        // ImGui_ImplDX12_CreateDeviceObjects() after rebuilding (or let the
        // backend pick it up on 1.92+ with ImGuiBackendFlags_RendererHasTextures).
        return out;
    }
}
