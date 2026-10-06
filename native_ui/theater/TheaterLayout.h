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
//   2. Spare HEIGHT  -> sequencer grows (more track rows become visible).
//   3. Spare WIDTH   -> side panel widens up to PanelMax, then a Selection
//                       column opens on the right (>= SelectionMin wide).
//   4. Anything left -> centred pillarbox / letterbox (AppBg).
//
// Output of this exact code (display px; compiled and run against Dear ImGui 1.93 WIP):
//   display     picture  UiScale  game        panel  selection  sequencer  pillar
//   2560x1440   16:9     1.00     2098x1180   398    -          260        0  
//   1920x1080   16:9     0.80     1550x 872   319    -          208        0  
//   3840x2160   16:9     1.50     3147x1770   597    -          390        0  
//   2560x1600   16:10    1.00     2144x1340   352    -          260        0  
//   2560x1100   21:9     0.80     2076x 892   352    -          208        41 
//   2560x1100   16:9 *   0.80     1586x 892   352    288        208        142
//   3440x1440   21:9     1.00     2819x1180   440    -          260        59 
//   3440x1440   16:9 *   1.00     2098x1180   440    360        260        239
//   1920x 810   21:9     0.80     1427x 602   352    -          208        45 
//   5120x1440   16:9 *   1.00     2098x1180   440    360        260        1079
//   1280x 720   16:9     0.80      910x 512   319    -          208        0  
//   Every row fits 6 track rows (TrackRowsThatFit) with SequencerMin 260.
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

        const float rail = Layout.RailWidth;
        float panel = pl.panelOpen ? std::clamp(pl.panelWidth, Layout.PanelMin, Layout.PanelMax) : 0.0f;
        float right = 0.0f;
        const float seqMin = std::max(Layout.SequencerMin, pl.sequencerMin);

        const float W0 = W - rail - panel;
        float gameH = std::min(H - seqMin, W0 / gameAspect);
        float gameW = gameH * gameAspect;
        float seq = H - gameH;
        float padY = 0.0f;
        if (seq > H * Layout.SequencerMaxFraction)      // very tall displays: cap the sequencer, letterbox the rest
        {
            padY = seq - H * Layout.SequencerMaxFraction;
            seq = H * Layout.SequencerMaxFraction;
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
        }

        const float areaX = rail + panel, areaW = W - rail - panel - right, areaH = H - seq;
        (void)padY; // the game is centred in area; padY is the resulting letterbox

        r.railMin      = ImVec2(0, 0);                         r.railMax      = ImVec2(toPx(rail), displaySize.y);
        r.panelMin     = ImVec2(toPx(rail), 0);                r.panelMax     = ImVec2(toPx(rail + panel), toPx(areaH));
        r.selectionMin = ImVec2(toPx(W - right), 0);           r.selectionMax = ImVec2(displaySize.x, toPx(areaH));
        r.sequencerMin = ImVec2(toPx(rail), toPx(areaH));      r.sequencerMax = displaySize;
        r.areaMin      = ImVec2(toPx(areaX), 0);               r.areaMax      = ImVec2(toPx(areaX + areaW), toPx(areaH));
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
}
