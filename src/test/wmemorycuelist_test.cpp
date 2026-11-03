// AI-generated (Claude Code). Start of AI-generated text.
// WMemoryCueList shows the memory cues of a track in time order, edits the
// comment and deletes exactly the memory cue of the row.
// End of AI-generated text.
#include "widget/wmemorycuelist.h"

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

class MemoryCueListTest : public MixxxTest {
  protected:
    MemoryCueListTest() {
        for (int i = 1; i <= kMaxNumberOfMemoryCueMarks; ++i) {
            auto pControl = std::make_unique<ControlPushButton>(
                    ConfigKey(kGroup, QStringLiteral("memory_cue_%1_activate").arg(i)));
            const int index = i;
            QObject::connect(pControl.get(),
                    &ControlObject::valueChanged,
                    [this, index](double value) {
                        if (value > 0) {
                            m_activated.push_back(index);
                        }
                    });
            m_activateControls.push_back(std::move(pControl));
        }
        m_pTrack = Track::newTemporary();
        m_pTrack->setAudioProperties(mixxx::audio::ChannelCount(2),
                mixxx::audio::SampleRate(44100),
                mixxx::audio::Bitrate(),
                mixxx::Duration::fromSeconds(180));
    }

    CuePointer addMemoryCue(double seconds,
            double loopSeconds = 0,
            const QString& label = QString()) {
        const auto start = mixxx::audio::FramePos(seconds * 44100);
        const auto end = loopSeconds > 0
                ? mixxx::audio::FramePos((seconds + loopSeconds) * 44100)
                : mixxx::audio::kInvalidFramePos;
        CuePointer pCue = m_pTrack->createAndAddCue(
                mixxx::CueType::MemoryCue, Cue::kNoHotCue, start, end);
        if (!label.isEmpty()) {
            pCue->setLabel(label);
        }
        return pCue;
    }

    static void leftClick(WMemoryCueList* pList, int row, int column) {
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

    std::vector<std::unique_ptr<ControlPushButton>> m_activateControls;
    std::vector<int> m_activated;
    TrackPointer m_pTrack;
};

TEST_F(MemoryCueListTest, FormatsTime) {
    EXPECT_EQ(QStringLiteral("00:00.000"), mixxx::formatMemoryCueTime(0.0));
    EXPECT_EQ(QStringLiteral("01:23.456"), mixxx::formatMemoryCueTime(83.456));
    EXPECT_EQ(QStringLiteral("00:59.999"), mixxx::formatMemoryCueTime(59.9994));
    EXPECT_EQ(QStringLiteral("01:00.000"), mixxx::formatMemoryCueTime(59.9996));
    EXPECT_EQ(QStringLiteral("75:00.000"), mixxx::formatMemoryCueTime(4500.0));
    EXPECT_TRUE(mixxx::formatMemoryCueTime(-1.0).isEmpty());
}

TEST_F(MemoryCueListTest, RowsInTimeOrderAndFollowTheTrack) {
    addMemoryCue(30.0, 0, QStringLiteral("drop"));
    addMemoryCue(10.5, 2.0);
    // Hot cues are not in the list.
    m_pTrack->createAndAddCue(mixxx::CueType::HotCue,
            0,
            mixxx::audio::FramePos(44100),
            mixxx::audio::kInvalidFramePos);

    WMemoryCueList list(kGroup);
    list.slotTrackLoaded(m_pTrack);
    ASSERT_EQ(2, list.rowCount());
    EXPECT_EQ(QStringLiteral("00:10.500"), list.item(0, 0)->text());
    EXPECT_EQ(QStringLiteral("00:02.000"), list.item(0, 1)->text());
    EXPECT_EQ(QStringLiteral("00:30.000"), list.item(1, 0)->text());
    EXPECT_TRUE(list.item(1, 1)->text().isEmpty());
    EXPECT_EQ(QStringLiteral("drop"), list.item(1, 2)->text());

    addMemoryCue(1.0);
    processEvents();
    ASSERT_EQ(3, list.rowCount());
    EXPECT_EQ(QStringLiteral("00:01.000"), list.item(0, 0)->text());

    list.slotLoadingTrack(TrackPointer(), m_pTrack);
    EXPECT_EQ(0, list.rowCount());
}

TEST_F(MemoryCueListTest, ClickActivatesTheRow) {
    addMemoryCue(20.0);
    addMemoryCue(5.0);
    WMemoryCueList list(kGroup);
    list.slotTrackLoaded(m_pTrack);

    list.activateMemoryCue(1);
    list.activateMemoryCue(0);
    list.activateMemoryCue(2); // No third row: nothing.
    EXPECT_EQ((std::vector<int>{2, 1}), m_activated);
}

TEST_F(MemoryCueListTest, EditCommentSetsLabel) {
    CuePointer pCue = addMemoryCue(5.0);
    WMemoryCueList list(kGroup);
    list.slotTrackLoaded(m_pTrack);

    list.item(0, 2)->setText(QStringLiteral("  break "));
    EXPECT_EQ(QStringLiteral("break"), pCue->getLabel());
    processEvents();
    EXPECT_EQ(QStringLiteral("break"), list.item(0, 2)->text());
}

TEST_F(MemoryCueListTest, DeleteRemovesTheCueOfTheRow) {
    CuePointer pFirst = addMemoryCue(5.0);
    CuePointer pSecond = addMemoryCue(20.0);
    CuePointer pThird = addMemoryCue(40.0);
    WMemoryCueList list(kGroup);
    list.slotTrackLoaded(m_pTrack);

    list.deleteMemoryCue(1);
    processEvents();

    // Exactly its cue is removed, and the deck does not go to it.
    EXPECT_TRUE(m_activated.empty());
    const QList<CuePointer> cues = m_pTrack->getCuePoints();
    EXPECT_TRUE(cues.contains(pFirst));
    EXPECT_FALSE(cues.contains(pSecond));
    EXPECT_TRUE(cues.contains(pThird));
    ASSERT_EQ(2, list.rowCount());
    EXPECT_EQ(QStringLiteral("00:40.000"), list.item(1, 0)->text());
}

TEST_F(MemoryCueListTest, RightClickDeletesTheClickedRow) {
    CuePointer pFirst = addMemoryCue(5.0);
    CuePointer pSecond = addMemoryCue(20.0);
    WMemoryCueList list(kGroup);
    list.resize(300, 120);
    list.slotTrackLoaded(m_pTrack);
    list.show();

    const QRect rect = list.visualRect(list.model()->index(1, 2));
    const QPointF pos = rect.center();
    QMouseEvent press(QEvent::MouseButtonPress,
            pos,
            list.viewport()->mapToGlobal(pos),
            Qt::RightButton,
            Qt::RightButton,
            Qt::NoModifier);
    QCoreApplication::sendEvent(list.viewport(), &press);
    processEvents();

    EXPECT_TRUE(m_activated.empty());
    const QList<CuePointer> cues = m_pTrack->getCuePoints();
    EXPECT_TRUE(cues.contains(pFirst));
    EXPECT_FALSE(cues.contains(pSecond));
}

TEST_F(MemoryCueListTest, OnlyTimeAndLoopClicksActivate) {
    addMemoryCue(5.0, 2.0);
    addMemoryCue(20.0);
    WMemoryCueList list(kGroup);
    list.resize(300, 120);
    list.slotTrackLoaded(m_pTrack);
    list.show();

    // A click on the comment (also the first click of a double click to
    // edit) does not move the deck.
    leftClick(&list, 1, 2);
    EXPECT_TRUE(m_activated.empty());

    leftClick(&list, 1, 0);
    leftClick(&list, 0, 1);
    EXPECT_EQ((std::vector<int>{2, 1}), m_activated);
}

TEST_F(MemoryCueListTest, EditWritesTheEditedCueWhenRowsMove) {
    CuePointer pFirst = addMemoryCue(10.0);
    CuePointer pSecond = addMemoryCue(20.0);
    WMemoryCueList list(kGroup);
    list.resize(300, 120);
    list.slotTrackLoaded(m_pTrack);
    list.show();

    list.editItem(list.item(1, 2));
    auto* pEditor = list.viewport()->findChild<QLineEdit*>();
    ASSERT_NE(nullptr, pEditor);

    // A new memory cue before the edited row while the editor is open.
    CuePointer pNew = addMemoryCue(1.0);
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
    EXPECT_EQ(QStringLiteral("00:01.000"), list.item(0, 0)->text());
    EXPECT_EQ(QStringLiteral("verse"), list.item(2, 2)->text());
}

} // namespace
