// AI-generated (Claude Code). Start of AI-generated text.
// WHotCueList shows the hot cues of a track in the order of the pads, edits
// the comment and clears exactly the hot cue of the row through the same
// control as the CLEAR of the deck.
// End of AI-generated text.
#include "widget/whotcuelist.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <memory>
#include <vector>

#include "control/controlpushbutton.h"
#include "test/mixxxtest.h"
#include "track/track.h"
#include "util/defs.h"

namespace {

const QString kGroup = QStringLiteral("[Channel1]");

class HotCueListTest : public MixxxTest {
  protected:
    HotCueListTest() {
        for (int i = 1; i <= kMaxNumberOfHotcues; ++i) {
            m_gotoControls.push_back(makeRecorder(
                    QStringLiteral("hotcue_%1_goto").arg(i), i, &m_gone));
            m_clearControls.push_back(makeRecorder(
                    QStringLiteral("hotcue_%1_clear").arg(i), i, &m_cleared));
        }
        m_pTrack = Track::newTemporary();
        m_pTrack->setAudioProperties(mixxx::audio::ChannelCount(2),
                mixxx::audio::SampleRate(44100),
                mixxx::audio::Bitrate(),
                mixxx::Duration::fromSeconds(180));
    }

    std::unique_ptr<ControlPushButton> makeRecorder(
            const QString& item, int number, std::vector<int>* pLog) {
        auto pControl = std::make_unique<ControlPushButton>(ConfigKey(kGroup, item));
        QObject::connect(pControl.get(),
                &ControlObject::valueChanged,
                [number, pLog](double value) {
                    if (value > 0) {
                        pLog->push_back(number);
                    }
                });
        return pControl;
    }

    // index is 0-based (0 is pad A).
    CuePointer addHotCue(int index,
            double seconds,
            double loopSeconds = 0,
            const QString& label = QString()) {
        const auto start = mixxx::audio::FramePos(seconds * 44100);
        const auto end = loopSeconds > 0
                ? mixxx::audio::FramePos((seconds + loopSeconds) * 44100)
                : mixxx::audio::kInvalidFramePos;
        CuePointer pCue = m_pTrack->createAndAddCue(
                loopSeconds > 0 ? mixxx::CueType::Loop : mixxx::CueType::HotCue,
                index,
                start,
                end);
        if (!label.isEmpty()) {
            pCue->setLabel(label);
        }
        return pCue;
    }

    static void leftClick(WHotCueList* pList, int row, int column) {
        const QPointF pos = pList->visualRect(pList->model()->index(row, column)).center();
        const QPointF globalPos = pList->viewport()->mapToGlobal(pos);
        QMouseEvent press(QEvent::MouseButtonPress,
                pos,
                globalPos,
                Qt::LeftButton,
                Qt::LeftButton,
                Qt::NoModifier);
        QCoreApplication::sendEvent(pList->viewport(), &press);
        QMouseEvent release(QEvent::MouseButtonRelease,
                pos,
                globalPos,
                Qt::LeftButton,
                Qt::NoButton,
                Qt::NoModifier);
        QCoreApplication::sendEvent(pList->viewport(), &release);
    }

    static void processEvents() {
        QCoreApplication::processEvents();
    }

    std::vector<std::unique_ptr<ControlPushButton>> m_gotoControls;
    std::vector<std::unique_ptr<ControlPushButton>> m_clearControls;
    std::vector<int> m_gone;
    std::vector<int> m_cleared;
    TrackPointer m_pTrack;
};

TEST_F(HotCueListTest, FormatsName) {
    EXPECT_EQ(QStringLiteral("A"), mixxx::formatHotCueName(0));
    EXPECT_EQ(QStringLiteral("H"), mixxx::formatHotCueName(7));
    EXPECT_EQ(QStringLiteral("Z"), mixxx::formatHotCueName(25));
    EXPECT_EQ(QStringLiteral("27"), mixxx::formatHotCueName(26));
    EXPECT_TRUE(mixxx::formatHotCueName(-1).isEmpty());
}

TEST_F(HotCueListTest, RowsInPadOrderAndFollowTheTrack) {
    addHotCue(2, 30.0, 0, QStringLiteral("drop"));
    addHotCue(0, 10.5, 2.0);
    // Memory cues and the main cue are not in the list.
    m_pTrack->createAndAddCue(mixxx::CueType::MemoryCue,
            Cue::kNoHotCue,
            mixxx::audio::FramePos(44100),
            mixxx::audio::kInvalidFramePos);
    m_pTrack->createAndAddCue(mixxx::CueType::MainCue,
            Cue::kNoHotCue,
            mixxx::audio::FramePos(88200),
            mixxx::audio::kInvalidFramePos);

    WHotCueList list(kGroup);
    list.slotTrackLoaded(m_pTrack);
    ASSERT_EQ(2, list.rowCount());
    EXPECT_EQ(QStringLiteral("A"), list.item(0, 0)->text());
    EXPECT_EQ(QStringLiteral("00:10.500"), list.item(0, 1)->text());
    EXPECT_EQ(QStringLiteral("00:02.000"), list.item(0, 2)->text());
    EXPECT_EQ(QStringLiteral("C"), list.item(1, 0)->text());
    EXPECT_EQ(QStringLiteral("00:30.000"), list.item(1, 1)->text());
    EXPECT_TRUE(list.item(1, 2)->text().isEmpty());
    EXPECT_EQ(QStringLiteral("drop"), list.item(1, 3)->text());

    // A new hot cue on pad B comes between the two rows.
    addHotCue(1, 1.0);
    processEvents();
    ASSERT_EQ(3, list.rowCount());
    EXPECT_EQ(QStringLiteral("B"), list.item(1, 0)->text());
    EXPECT_EQ(QStringLiteral("00:01.000"), list.item(1, 1)->text());

    list.slotLoadingTrack(TrackPointer(), m_pTrack);
    EXPECT_EQ(0, list.rowCount());
}

TEST_F(HotCueListTest, ClickGoesToTheHotCueOfTheRow) {
    addHotCue(3, 20.0);
    addHotCue(1, 5.0);
    WHotCueList list(kGroup);
    list.slotTrackLoaded(m_pTrack);

    // Row 0 is pad B (hotcue_2), row 1 is pad D (hotcue_4).
    list.activateHotCue(1);
    list.activateHotCue(0);
    list.activateHotCue(2); // No third row: nothing.
    EXPECT_EQ((std::vector<int>{4, 2}), m_gone);
}

TEST_F(HotCueListTest, EditCommentSetsLabel) {
    CuePointer pCue = addHotCue(0, 5.0);
    WHotCueList list(kGroup);
    list.slotTrackLoaded(m_pTrack);

    list.item(0, 3)->setText(QStringLiteral("  break "));
    EXPECT_EQ(QStringLiteral("break"), pCue->getLabel());
    processEvents();
    EXPECT_EQ(QStringLiteral("break"), list.item(0, 3)->text());
}

TEST_F(HotCueListTest, DeleteClearsTheHotCueOfTheRow) {
    addHotCue(0, 5.0);
    addHotCue(4, 20.0);
    addHotCue(6, 40.0);
    WHotCueList list(kGroup);
    list.slotTrackLoaded(m_pTrack);

    // Row 1 is pad E: its clear control is hotcue_5_clear, and no other.
    list.deleteHotCue(1);
    list.deleteHotCue(3); // No fourth row: nothing.
    EXPECT_EQ((std::vector<int>{5}), m_cleared);
    // The deck does not go to the hot cue.
    EXPECT_TRUE(m_gone.empty());
}

TEST_F(HotCueListTest, RightClickClearsTheClickedRow) {
    addHotCue(0, 5.0);
    addHotCue(2, 20.0);
    WHotCueList list(kGroup);
    list.resize(300, 120);
    list.slotTrackLoaded(m_pTrack);
    list.show();

    const QRect rect = list.visualRect(list.model()->index(1, 3));
    const QPointF pos = rect.center();
    QMouseEvent press(QEvent::MouseButtonPress,
            pos,
            list.viewport()->mapToGlobal(pos),
            Qt::RightButton,
            Qt::RightButton,
            Qt::NoModifier);
    QCoreApplication::sendEvent(list.viewport(), &press);
    processEvents();

    EXPECT_TRUE(m_gone.empty());
    EXPECT_EQ((std::vector<int>{3}), m_cleared);
}

TEST_F(HotCueListTest, OnlyNameTimeAndLoopClicksGoToTheHotCue) {
    addHotCue(0, 5.0, 2.0);
    addHotCue(1, 20.0);
    WHotCueList list(kGroup);
    list.resize(300, 120);
    list.slotTrackLoaded(m_pTrack);
    list.show();

    // A click on the comment (also the first click of a double click to
    // edit) does not move the deck.
    leftClick(&list, 1, 3);
    EXPECT_TRUE(m_gone.empty());

    leftClick(&list, 1, 1);
    leftClick(&list, 0, 2);
    leftClick(&list, 0, 0);
    EXPECT_EQ((std::vector<int>{2, 1, 1}), m_gone);
}

TEST_F(HotCueListTest, EditWritesTheEditedCueWhenRowsMove) {
    CuePointer pFirst = addHotCue(1, 10.0);
    CuePointer pSecond = addHotCue(2, 20.0);
    WHotCueList list(kGroup);
    list.resize(300, 120);
    list.slotTrackLoaded(m_pTrack);
    list.show();

    list.editItem(list.item(1, 3));
    auto* pEditor = list.viewport()->findChild<QLineEdit*>();
    ASSERT_NE(nullptr, pEditor);

    // A new hot cue on pad A, before the edited row, while the editor is open.
    CuePointer pNew = addHotCue(0, 1.0);
    processEvents();

    pEditor->setText(QStringLiteral("verse"));
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QCoreApplication::sendEvent(pEditor, &enter);
    // The delegate commits and closes the editor with a queued call; the
    // deferred rebuild comes after it.
    processEvents();
    processEvents();

    EXPECT_EQ(QStringLiteral("verse"), pSecond->getLabel());
    EXPECT_TRUE(pFirst->getLabel().isEmpty());
    EXPECT_TRUE(pNew->getLabel().isEmpty());
    // The deferred rebuild has the new cue now.
    ASSERT_EQ(3, list.rowCount());
    EXPECT_EQ(QStringLiteral("A"), list.item(0, 0)->text());
    EXPECT_EQ(QStringLiteral("verse"), list.item(2, 3)->text());
}

} // namespace
