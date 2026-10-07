#pragma once
// ============================================================================
// TheaterLayout.h
// Elden Ring Theater Mode - resolution-independent layout solver.
//
// One function turns (display size, game picture aspect, panel state) into
// rects. It runs every frame, costs nothing, and is the same algorithm the
// mockups use, so every PNG in screens/ is a real output of this code.
//
// Priority for space the game picture cannot use:
//   1. Game picture as large as possible at its own aspect ratio.
//   0. A 28 px menu bar (Layout menu) runs across the top; everything else is below it.
//   2. Spare HEIGHT  -> sequencer grows (more track rows become visible).
//   3. Spare WIDTH   -> side panel widens up to PanelMax, then a Selection
//                       column opens on the right (>= SelectionMin wide),
//                       then the panel takes the rest up to PanelHardMax.
//   4. Anything left -> centred pillarbox / letterbox (AppBg).
//
// Output of this exact code (display px; compiled and run against Dear ImGui 1.93 WIP):
//   display     picture  UiScale  game        panel  selection  sequencer  pillar
//   2560x1440   16:9     1.00     1977x1112   519    -          300        0  
//   1920x1080   16:9     0.80     1454x 818   415    -          240        0  
//   3840x2160   16:9     1.50     2965x1668   779    -          450        0  
//   2560x1600   16:10    1.00     2035x1272   461    -          300        0  
//   2560x1100   21:9     0.80     1950x 838   512    -          240        23 
//   2560x1100   16:9 *   0.80     1490x 838   512    288        240        110
//   3440x1440   21:9     1.00     2656x1112   640    -          300        40 
//   3440x1440   16:9 *   1.00     1977x1112   640    360        300        200
//   1920x 810   21:9     0.80     1299x 548   512    -          240        29 
//   5120x1440   16:9 *   1.00     1977x1112   640    360        300        1040
//   1280x 720   16:9     0.80      814x 458   415    -          240        0  
//   Every row fits 7 track rows (TrackRowsThatFit) with SequencerMin 300 and the 28 px menu bar.
//   * vanilla Elden Ring picture on an ultrawide display (no ultrawide fix)
// ============================================================================

#include <algorithm>
#include <cmath>

#include "TheaterTheme.h"
#include "TheaterUITypes.h"

namespace TheaterUI
{
    // Fit a rect of aspect `a` inside (x, y, w, h), centred.
    inline void FitAspect(float x, float y, float w, float h, float a, ImVec2& outMin, ImVec2& outMax)
    {
        float gw = w, gh = w / a;
        if (gh > h) { gh = h; gw = h * a; }
        outMin = ImVec2(x + (w - gw) * 0.5f, y + (h - gh) * 0.5f);
        outMax = ImVec2(outMin.x + gw, outMin.y + gh);
    }

    // displaySize : swap-chain size in pixels
    // gameAspect  : vm.gameFrame.contentAspect (16/9 on vanilla, display aspect with an ultrawide fix)
    inline LayoutRects SolveLayout(ImVec2 displaySize, float gameAspect, const PanelLayout& pl, UiVisibility vis)
    {
        using namespace Theme;
        LayoutRects r;
        const float s = ComputeUiScale(displaySize.x, displaySize.y, pl.uiScaleUser);
        r.uiScale = s;

        // Work in logical px (UiScale 1), convert to display px at the end.
        const float W = displaySize.x / s, H = displaySize.y / s;
        auto toPx = [s](float v) { return std::floor(v * s + 0.5f); };

        if (vis != UiVisibility::Shown)
        {
            // Hidden: the game owns the whole back buffer. Rects describe where
            // its picture sits (for the REC pill), nothing else is drawn.
            r.areaMin = ImVec2(0, 0); r.areaMax = displaySize;
            FitAspect(0, 0, displaySize.x, displaySize.y, gameAspect, r.gameMin, r.gameMax);
            return r;
        }

        const float MB = Layout.MenuBarHeight;  // everything below the menu bar
        const float Hb = H - MB;
        const float rail = Layout.RailWidth;
        float panel = pl.panelOpen ? std::clamp(pl.panelWidth, Layout.PanelMin, Layout.PanelMax) : 0.0f;
        float right = 0.0f;
        const float seqMin = std::max(Layout.SequencerMin, pl.sequencerMin);

        const float W0 = W - rail - panel;
        float gameH = std::min(Hb - seqMin, W0 / gameAspect);
        float gameW = gameH * gameAspect;
        float seq = Hb - gameH;
        float padY = 0.0f;
        if (seq > Hb * Layout.SequencerMaxFraction)     // very tall displays: cap the sequencer, letterbox the rest
        {
            padY = seq - Hb * Layout.SequencerMaxFraction;
            seq = Hb * Layout.SequencerMaxFraction;
        }

        float spareW = W0 - gameW;
        if (spareW > 1.0f)
        {
            if (pl.panelOpen)
            {
                const float add = std::min(spareW, Layout.PanelMax - panel);
                panel += add; spareW -= add;
            }
            if (pl.allowSelectionColumn && spareW >= Layout.SelectionMin)
            {
                right = std::min(spareW, Layout.SelectionWidth);
                spareW -= right;
            }
            if (pl.panelOpen && spareW > 1.0f)        // no thin pillarbox: the panel takes the rest
            {
                const float add = std::min(spareW, Layout.PanelHardMax - panel);
                panel += add; spareW -= add;
            }
        }

        const float areaX = rail + panel, areaW = W - rail - panel - right, areaH = Hb - seq;
        (void)padY; // the game is centred in area; padY is the resulting letterbox

        r.menuMin      = ImVec2(0, 0);                         r.menuMax      = ImVec2(displaySize.x, toPx(MB));
        r.railMin      = ImVec2(0, toPx(MB));                  r.railMax      = ImVec2(toPx(rail), displaySize.y);
        r.panelMin     = ImVec2(toPx(rail), toPx(MB));         r.panelMax     = ImVec2(toPx(rail + panel), toPx(MB + areaH));
        r.selectionMin = ImVec2(toPx(W - right), toPx(MB));    r.selectionMax = ImVec2(displaySize.x, toPx(MB + areaH));
        r.sequencerMin = ImVec2(toPx(rail), toPx(MB + areaH)); r.sequencerMax = displaySize;
        r.areaMin      = ImVec2(toPx(areaX), toPx(MB));        r.areaMax      = ImVec2(toPx(areaX + areaW), toPx(MB + areaH));
        r.hasSelectionColumn = right > 0.0f;

        FitAspect(r.areaMin.x, r.areaMin.y, r.areaMax.x - r.areaMin.x, r.areaMax.y - r.areaMin.y,
                  gameAspect, r.gameMin, r.gameMax);
        return r;
    }

    // Map a point on the scaled game picture back to back-buffer pixels
    // (for viewport picking and for drawing world-space overlays).
    inline ImVec2 GameViewToBackBuffer(const LayoutRects& r, ImVec2 p, ImVec2 backBuffer, float gameAspect)
    {
        ImVec2 srcMin, srcMax; // where the picture lives inside the full back buffer
        FitAspect(0, 0, backBuffer.x, backBuffer.y, gameAspect, srcMin, srcMax);
        const float u = (p.x - r.gameMin.x) / (r.gameMax.x - r.gameMin.x);
        const float v = (p.y - r.gameMin.y) / (r.gameMax.y - r.gameMin.y);
        return ImVec2(srcMin.x + u * (srcMax.x - srcMin.x), srcMin.y + v * (srcMax.y - srcMin.y));
    }

    inline ImVec2 BackBufferToGameView(const LayoutRects& r, ImVec2 p, ImVec2 backBuffer, float gameAspect)
    {
        ImVec2 srcMin, srcMax;
        FitAspect(0, 0, backBuffer.x, backBuffer.y, gameAspect, srcMin, srcMax);
        const float u = (p.x - srcMin.x) / (srcMax.x - srcMin.x);
        const float v = (p.y - srcMin.y) / (srcMax.y - srcMin.y);
        return ImVec2(r.gameMin.x + u * (r.gameMax.x - r.gameMin.x), r.gameMin.y + v * (r.gameMax.y - r.gameMin.y));
    }

    // Track rows that fit: top-level rows always show; child rows of the
    // selected actor auto-expand until the sequencer height is used up.
    inline int TrackRowsThatFit(const LayoutRects& r)
    {
        const float h = (r.sequencerMax.y - r.sequencerMin.y) / r.uiScale
                      - Theme::Layout.SequencerToolbar - Theme::Layout.RulerHeight - Theme::Layout.NavigatorHeight;
        return std::max(1, (int)(h / Theme::Metric::TrackRowHeight));
    }

    // Floating (undocked) panels: keep at least a 48 px grab strip of the
    // header on the display so a panel can never be lost off-screen, e.g.
    // after a resolution change or a layout.ini from a bigger monitor.
    inline void ClampToDisplay(ImVec2& pos, ImVec2& size, ImVec2 display, float uiScale)
    {
        const float grab = 48.0f * uiScale, header = Theme::Metric::PanelHeaderHeight * uiScale;
        size.x = std::min(size.x, display.x); size.y = std::min(size.y, display.y);
        pos.x = std::clamp(pos.x, grab - size.x, display.x - grab);
        pos.y = std::clamp(pos.y, 0.0f, display.y - header);
    }
}
