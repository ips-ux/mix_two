#pragma once

#include <QColor>

#include "waveform/overviewtype.h"
#include "waveform/waveform.h"

class QPainter;
class WaveformSignalColors;

namespace waveformOverviewRenderer {
/// This returns the normalized fullsize image
/// for the library's overview column.
QImage render(ConstWaveformPointer pWaveform,
        mixxx::OverviewType type,
        const WaveformSignalColors& signalColors,
        const QColor& colorSaturatedBase,
        bool mono = false);

/// These paint methods return the fullsize image
/// They allow "mono" rendering (mono-mixdown, bottom-aligned).
/// Note: Don't use mono = true with WOverview, it's not adjusted yet! It does some
/// additional scaling for normalization which will atm cut off the bottom part.
void drawWaveformPartRGB(
        QPainter* pPainter,
        ConstWaveformPointer pWaveform,
        int* start,
        int end,
        const WaveformSignalColors& signalColors,
        bool mono = false);
void drawWaveformPartLMH(
        QPainter* pPainter,
        ConstWaveformPointer pWaveform,
        int* start,
        int end,
        const WaveformSignalColors& signalColors,
        bool mono = false);
void drawWaveformPartHSV(
        QPainter* pPainter,
        ConstWaveformPointer pWaveform,
        int* start,
        int end,
        const WaveformSignalColors& signalColors,
        bool mono = false);
// <!-- AI-generated (Claude Code, 2026-11-02). Start of AI-generated text. -->
/// Color Saturated: the height is the loudest band, the colour goes from
/// `baseColor` to white as the mid band gets smaller (see bandcolor.h).
// <!-- End of AI-generated text. -->
void drawWaveformPartColorSaturated(
        QPainter* pPainter,
        ConstWaveformPointer pWaveform,
        int* start,
        int end,
        const QColor& baseColor,
        bool mono = false);
} // namespace waveformOverviewRenderer
