// AI-generated (Claude Code). Start of AI-generated text.

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QTemporaryDir>

#include "library/export/legacystickflow.h"
#include "library/rekordbox/legacylibrary.h"

namespace mixxx {

namespace {

using rekordbox::LegacyToDlpResult;

/// Gives scripted answers and records the questions.
class ScriptedUi : public LegacyStickUi {
  public:
    QList<bool> updateAnswers;
    QList<bool> stopAskingAnswers;
    QList<bool> keepAnswers;
    QList<bool> backAnswers;
    QList<bool> keepInitials;
    int updateQuestions = 0;
    int summaries = 0;

    bool askUpdate(bool* pStopAsking) override {
        EXPECT_FALSE(*pStopAsking);
        ++updateQuestions;
        *pStopAsking = stopAskingAnswers.isEmpty() ? false : stopAskingAnswers.takeFirst();
        return updateAnswers.isEmpty() ? false : updateAnswers.takeFirst();
    }
    bool askKeepLegacy(bool initial) override {
        keepInitials.append(initial);
        return keepAnswers.isEmpty() ? initial : keepAnswers.takeFirst();
    }
    bool showSummary(const LegacyToDlpResult& result) override {
        Q_UNUSED(result);
        ++summaries;
        return backAnswers.isEmpty() ? false : backAnswers.takeFirst();
    }
};

/// Records each apply call and gives a result.
struct RecordingApply {
    QList<LegacyStickChoices> calls;
    bool succeed = true;

    LegacyStickApply function() {
        return [this](const LegacyStickChoices& choices) {
            calls.append(choices);
            LegacyToDlpResult result;
            result.ok = succeed;
            result.legacyKept = choices.keepLegacy || !choices.update;
            if (!succeed) {
                result.errorMessage = QStringLiteral("The stick is full.");
            }
            return result;
        };
    }
};

} // namespace

TEST(LegacyStickFlow, OnlyAStickWithOnlyTheLegacyLibraryNeedsTheUpdate) {
    EXPECT_TRUE(isLegacyOnlyStick(true, false));
    EXPECT_FALSE(isLegacyOnlyStick(true, true));
    EXPECT_FALSE(isLegacyOnlyStick(false, true));
    EXPECT_FALSE(isLegacyOnlyStick(false, false));
}

TEST(LegacyStickFlow, NotNowChangesNothing) {
    ScriptedUi ui;
    ui.updateAnswers = {false};
    RecordingApply apply;
    const auto outcome = runLegacyStickFlow({}, ui, apply.function());
    EXPECT_EQ(1, ui.updateQuestions);
    EXPECT_TRUE(ui.keepInitials.isEmpty());
    EXPECT_EQ(0, ui.summaries);
    EXPECT_TRUE(apply.calls.isEmpty());
    EXPECT_EQ(0, outcome.applyCount);
    EXPECT_FALSE(outcome.choices.update);
    EXPECT_FALSE(outcome.prefsToStore);
}

TEST(LegacyStickFlow, UpdateAndKeepLegacyAppliesOnceAndShowsTheSummary) {
    ScriptedUi ui;
    ui.updateAnswers = {true};
    ui.keepAnswers = {true};
    RecordingApply apply;
    const auto outcome = runLegacyStickFlow({}, ui, apply.function());
    ASSERT_EQ(1, apply.calls.size());
    EXPECT_TRUE(apply.calls[0].update);
    EXPECT_TRUE(apply.calls[0].keepLegacy);
    EXPECT_EQ(1, ui.summaries);
    EXPECT_EQ(QList<bool>{true}, ui.keepInitials);
    EXPECT_EQ(1, outcome.applyCount);
    EXPECT_TRUE(outcome.lastResult.ok);
    EXPECT_TRUE(outcome.choices.update);
    EXPECT_TRUE(outcome.choices.keepLegacy);
    EXPECT_FALSE(outcome.prefsToStore);
}

TEST(LegacyStickFlow, BackInTheSummaryAsksAgainWithTheLastChoices) {
    ScriptedUi ui;
    ui.updateAnswers = {true, true};
    ui.keepAnswers = {true, false};
    ui.backAnswers = {true, false};
    RecordingApply apply;
    const auto outcome = runLegacyStickFlow({}, ui, apply.function());
    EXPECT_EQ(2, ui.updateQuestions);
    // The second question starts with the first answer.
    EXPECT_EQ((QList<bool>{true, true}), ui.keepInitials);
    ASSERT_EQ(2, apply.calls.size());
    EXPECT_TRUE(apply.calls[1].update);
    EXPECT_FALSE(apply.calls[1].keepLegacy);
    EXPECT_EQ(2, ui.summaries);
    EXPECT_FALSE(outcome.choices.keepLegacy);
    EXPECT_FALSE(outcome.lastResult.legacyKept);
}

TEST(LegacyStickFlow, BackThenNotNowUndoesTheUpdate) {
    ScriptedUi ui;
    ui.updateAnswers = {true, false};
    ui.keepAnswers = {false};
    ui.backAnswers = {true};
    RecordingApply apply;
    const auto outcome = runLegacyStickFlow({}, ui, apply.function());
    ASSERT_EQ(2, apply.calls.size());
    EXPECT_TRUE(apply.calls[0].update);
    EXPECT_FALSE(apply.calls[1].update);
    EXPECT_FALSE(outcome.choices.update);
    EXPECT_EQ(1, ui.summaries);
}

TEST(LegacyStickFlow, StopAskingIsStoredWithTheChoiceAfterASuccessfulUpdate) {
    ScriptedUi ui;
    ui.updateAnswers = {true};
    ui.stopAskingAnswers = {true};
    ui.keepAnswers = {false};
    RecordingApply apply;
    const auto outcome = runLegacyStickFlow({}, ui, apply.function());
    ASSERT_TRUE(outcome.prefsToStore);
    EXPECT_TRUE(outcome.prefsToStore->stopAsking);
    EXPECT_FALSE(outcome.prefsToStore->keepLegacy);
}

TEST(LegacyStickFlow, StopAskingIsNotStoredWhenTheUpdateFails) {
    ScriptedUi ui;
    ui.updateAnswers = {true};
    ui.stopAskingAnswers = {true};
    ui.keepAnswers = {true};
    RecordingApply apply;
    apply.succeed = false;
    const auto outcome = runLegacyStickFlow({}, ui, apply.function());
    EXPECT_EQ(1, apply.calls.size());
    EXPECT_EQ(1, ui.summaries);
    EXPECT_FALSE(outcome.lastResult.ok);
    EXPECT_FALSE(outcome.prefsToStore);
}

TEST(LegacyStickFlow, StopAskingUpdatesWithTheStoredChoiceAndAsksNothing) {
    ScriptedUi ui;
    RecordingApply apply;
    LegacyStickPrefs prefs;
    prefs.stopAsking = true;
    prefs.keepLegacy = false;
    const auto outcome = runLegacyStickFlow(prefs, ui, apply.function());
    EXPECT_EQ(0, ui.updateQuestions);
    EXPECT_TRUE(ui.keepInitials.isEmpty());
    EXPECT_EQ(0, ui.summaries);
    ASSERT_EQ(1, apply.calls.size());
    EXPECT_TRUE(apply.calls[0].update);
    EXPECT_FALSE(apply.calls[0].keepLegacy);
    EXPECT_EQ(1, outcome.applyCount);
    EXPECT_FALSE(outcome.prefsToStore);
}

TEST(LegacyStickFlow, StopAskingShowsTheSummaryWhenTheUpdateFails) {
    ScriptedUi ui;
    RecordingApply apply;
    apply.succeed = false;
    LegacyStickPrefs prefs;
    prefs.stopAsking = true;
    const auto outcome = runLegacyStickFlow(prefs, ui, apply.function());
    EXPECT_EQ(1, ui.summaries);
    EXPECT_FALSE(outcome.lastResult.ok);
}

/// A real stick: the apply function changes the stick on each Back.
class LegacyStickFlowStick : public testing::Test {
  protected:
    void SetUp() override {
        ASSERT_TRUE(m_stick.isValid());
        ASSERT_TRUE(m_backups.isValid());
        rekordbox::LegacyLibrary library;
        rekordbox::LegacyTrack track;
        track.id = 1;
        track.title = QStringLiteral("Title");
        track.filePath = QStringLiteral("/Contents/a.mp3");
        track.fileName = QStringLiteral("a.mp3");
        library.tracks.append(track);
        m_pdb = rekordbox::legacyPdbBytes(library);
        ASSERT_FALSE(m_pdb.isEmpty());
        ASSERT_TRUE(write(kPdb, m_pdb));
        ASSERT_TRUE(write(kExt, QByteArray("ext")));
    }

    bool write(const QString& relative, const QByteArray& bytes) {
        const QString path = QDir(m_stick.path()).filePath(relative);
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
    }

    bool exists(const QString& relative) const {
        return QFileInfo::exists(QDir(m_stick.path()).filePath(relative));
    }

    LegacyStickApply apply() const {
        rekordbox::LegacyToDlpOptions options;
        options.backupBaseDir = m_backups.path();
        options.deviceName = QStringLiteral("STICK");
        options.now = QDateTime(QDate(2026, 11, 26), QTime(10, 0, 0));
        options.masterDbId = 42;
        return makeLegacyStickApply(m_stick.path(), options);
    }

    const QString kPdb = QStringLiteral("PIONEER/rekordbox/export.pdb");
    const QString kExt = QStringLiteral("PIONEER/rekordbox/exportExt.pdb");
    const QString kPlus = QStringLiteral("PIONEER/rekordbox/exportLibrary.db");
    QTemporaryDir m_stick;
    QTemporaryDir m_backups;
    QByteArray m_pdb;
};

TEST_F(LegacyStickFlowStick, RemoveThenKeepThenNotNowGivesTheStickBack) {
    const LegacyStickApply change = apply();
    auto result = change({true, false});
    ASSERT_TRUE(result.ok) << qPrintable(result.errorMessage);
    EXPECT_TRUE(exists(kPlus));
    EXPECT_FALSE(exists(kPdb));
    EXPECT_FALSE(exists(kExt));
    EXPECT_FALSE(result.legacyKept);
    EXPECT_EQ(1, result.trackCount);

    result = change({true, true});
    ASSERT_TRUE(result.ok) << qPrintable(result.errorMessage);
    EXPECT_TRUE(exists(kPlus));
    EXPECT_TRUE(exists(kPdb));
    EXPECT_TRUE(exists(kExt));
    EXPECT_TRUE(result.legacyKept);
    EXPECT_EQ(1, result.trackCount);

    result = change({true, false});
    ASSERT_TRUE(result.ok) << qPrintable(result.errorMessage);
    EXPECT_FALSE(exists(kPdb));

    result = change({false, true});
    ASSERT_TRUE(result.ok) << qPrintable(result.errorMessage);
    EXPECT_FALSE(exists(kPlus));
    EXPECT_TRUE(exists(kExt));
    QFile pdb(QDir(m_stick.path()).filePath(kPdb));
    ASSERT_TRUE(pdb.open(QIODevice::ReadOnly));
    EXPECT_EQ(m_pdb, pdb.readAll());

    // The stick is legacy-only again, so a new update works.
    result = change({true, true});
    EXPECT_TRUE(result.ok) << qPrintable(result.errorMessage);
    EXPECT_TRUE(exists(kPlus));
}

} // namespace mixxx

// End of AI-generated text.
