#include <gtest/gtest.h>

#include <cmath>

#include "library/rekordbox/rekordboxbeats.h"

namespace {

using mixxx::audio::FramePos;
using mixxx::rekordbox::GridBeat;

const auto kSampleRate = mixxx::audio::SampleRate(44100);
const QString kSubVersion = QStringLiteral("test");

/// Beats as rekordbox stores them: times rounded to whole milliseconds, plus
/// the tempo it shows (BPM x 100).
void appendRekordboxBeats(QVector<GridBeat>* pGrid, double startMs, double bpm, int count) {
    const double beatMs = 60000.0 / bpm;
    for (int i = 0; i < count; ++i) {
        const double timeMs = std::round(startMs + i * beatMs);
        pGrid->append(GridBeat{FramePos(kSampleRate / 1000.0 * timeMs),
                static_cast<int>(std::lround(bpm * 100))});
    }
}

double bpmAt(const mixxx::BeatsPointer& pBeats, double timeMs) {
    return pBeats->getBpmAroundPosition(FramePos(kSampleRate / 1000.0 * timeMs), 4).value();
}

TEST(RekordboxBeatsTest, ConstantTempoDoesNotFlicker) {
    QVector<GridBeat> grid;
    appendRekordboxBeats(&grid, 120, 132.0, 600);
    const auto pBeats = mixxx::rekordbox::beatsFromGrid(kSampleRate, grid, kSubVersion);
    ASSERT_TRUE(pBeats);
    for (double ms = 2000; ms < 250000; ms += 777) {
        EXPECT_NEAR(132.0, bpmAt(pBeats, ms), 0.005) << "at " << ms << " ms";
    }
}

TEST(RekordboxBeatsTest, RoundedBeatPositionsAloneDoFlicker) {
    // What the import did before: the shown BPM moved by a few hundredths.
    QVector<GridBeat> grid;
    appendRekordboxBeats(&grid, 120, 132.0, 600);
    QVector<FramePos> positions;
    for (const auto& beat : grid) {
        positions.append(beat.position);
    }
    const auto pBeats = mixxx::Beats::fromBeatPositions(kSampleRate, positions, kSubVersion);
    double maxError = 0;
    for (double ms = 2000; ms < 250000; ms += 777) {
        maxError = std::max(maxError, std::fabs(bpmAt(pBeats, ms) - 132.0));
    }
    EXPECT_GT(maxError, 0.02);
}

TEST(RekordboxBeatsTest, TempoChangesStartNewSegments) {
    QVector<GridBeat> grid;
    appendRekordboxBeats(&grid, 50, 128.0, 64);
    const double changeMs = 50 + 64 * 60000.0 / 128.0;
    appendRekordboxBeats(&grid, changeMs, 130.0, 200);
    const auto pBeats = mixxx::rekordbox::beatsFromGrid(kSampleRate, grid, kSubVersion);
    ASSERT_TRUE(pBeats);
    EXPECT_NEAR(128.0, bpmAt(pBeats, 10000), 0.005);
    EXPECT_NEAR(130.0, bpmAt(pBeats, changeMs + 20000), 0.005);
}

TEST(RekordboxBeatsTest, BeatsStayOnRekordboxsGrid) {
    QVector<GridBeat> grid;
    appendRekordboxBeats(&grid, 120, 132.0, 600);
    const auto pBeats = mixxx::rekordbox::beatsFromGrid(kSampleRate, grid, kSubVersion);
    ASSERT_TRUE(pBeats);
    // Every beat within a millisecond of where rekordbox has it.
    auto it = pBeats->iteratorFrom(grid.front().position);
    for (const auto& beat : grid) {
        EXPECT_NEAR(beat.position.value(), (*it).value(), kSampleRate / 1000.0);
        ++it;
    }
}

TEST(RekordboxBeatsTest, TooFewBeats) {
    QVector<GridBeat> grid;
    appendRekordboxBeats(&grid, 120, 132.0, 1);
    EXPECT_FALSE(mixxx::rekordbox::beatsFromGrid(kSampleRate, grid, kSubVersion));
}

} // namespace
