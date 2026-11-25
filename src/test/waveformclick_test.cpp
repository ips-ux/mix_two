// AI-generated (Claude Code). Start of AI-generated text.
#include "widget/waveformclick.h"

#include <gtest/gtest.h>

#include "test/mixxxtest.h"

namespace {

class WaveformClickTest : public MixxxTest {};

} // namespace

TEST_F(WaveformClickTest, PreferenceIsOffByDefault) {
    EXPECT_FALSE(waveformclick::isClickPlayCueEnabled(config()));
    EXPECT_FALSE(waveformclick::isClickPlayCueEnabled(UserSettingsPointer()));
}

TEST_F(WaveformClickTest, PreferenceReadsConfiguration) {
    config()->setValue(waveformclick::clickPlayCueConfigKey(), true);
    EXPECT_TRUE(waveformclick::isClickPlayCueEnabled(config()));
    config()->setValue(waveformclick::clickPlayCueConfigKey(), false);
    EXPECT_FALSE(waveformclick::isClickPlayCueEnabled(config()));
}

TEST_F(WaveformClickTest, PressAndReleaseOnSamePointIsClick) {
    EXPECT_TRUE(waveformclick::isClick(QPoint(100, 20), QPoint(100, 20)));
}

TEST_F(WaveformClickTest, SmallMoveIsStillClick) {
    EXPECT_TRUE(waveformclick::isClick(QPoint(100, 20), QPoint(102, 21)));
    EXPECT_TRUE(waveformclick::isClick(QPoint(100, 20), QPoint(96, 20)));
    EXPECT_TRUE(waveformclick::isClick(QPoint(100, 20), QPoint(100, 24)));
}

TEST_F(WaveformClickTest, MoveBeyondLimitIsDrag) {
    EXPECT_FALSE(waveformclick::isClick(QPoint(100, 20), QPoint(105, 20)));
    EXPECT_FALSE(waveformclick::isClick(QPoint(100, 20), QPoint(100, 15)));
    // Diagonal: 3 and 3 pixels are 4.2 pixels away.
    EXPECT_FALSE(waveformclick::isClick(QPoint(100, 20), QPoint(103, 23)));
    EXPECT_TRUE(waveformclick::isDrag(QPoint(0, 0), QPoint(40, 0)));
    EXPECT_FALSE(waveformclick::isDrag(QPoint(0, 0), QPoint(1, 1)));
}

TEST_F(WaveformClickTest, ButtonsMapToControls) {
    EXPECT_EQ(waveformclick::Action::TogglePlayOnClick,
            waveformclick::actionForButton(Qt::LeftButton));
    EXPECT_EQ(waveformclick::Action::CueDefaultWhileHeld,
            waveformclick::actionForButton(Qt::RightButton));
    EXPECT_EQ(waveformclick::Action::None,
            waveformclick::actionForButton(Qt::MiddleButton));
    EXPECT_EQ(waveformclick::Action::None,
            waveformclick::actionForButton(Qt::NoButton));
}

TEST_F(WaveformClickTest, HeldCueGoesUpOnEveryLossEvent) {
    using E = waveformclick::CueReleaseEvent;
    for (E e : {E::RightButtonReleased,
                 E::Hidden,
                 E::Destroyed,
                 E::WindowDeactivated,
                 E::MouseGrabLost}) {
        EXPECT_TRUE(waveformclick::mustReleaseCueNow(true, e, true));
        EXPECT_TRUE(waveformclick::mustReleaseCueNow(true, e, false));
    }
}

TEST_F(WaveformClickTest, HeldCueGoesUpWhenSettingSwitchedOff) {
    using E = waveformclick::CueReleaseEvent;
    EXPECT_TRUE(waveformclick::mustReleaseCueNow(true, E::Moved, false));
    EXPECT_FALSE(waveformclick::mustReleaseCueNow(true, E::Moved, true));
}

TEST_F(WaveformClickTest, NoHeldCueNeedsNoRelease) {
    using E = waveformclick::CueReleaseEvent;
    for (E e : {E::RightButtonReleased,
                 E::Hidden,
                 E::Destroyed,
                 E::WindowDeactivated,
                 E::MouseGrabLost,
                 E::Moved}) {
        EXPECT_FALSE(waveformclick::mustReleaseCueNow(false, e, false));
        EXPECT_FALSE(waveformclick::mustReleaseCueNow(false, e, true));
    }
}

// End of AI-generated text.
