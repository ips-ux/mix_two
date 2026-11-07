// AI-generated (Claude Code). Start of AI-generated text.
#include "preferences/dialog/prefsearch.h"

#include <gtest/gtest.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>

#include "test/mixxxtest.h"

using namespace mixxx::preferences;

namespace {

class PrefSearchTest : public MixxxTest {};

TEST_F(PrefSearchTest, NormalizeRemovesMnemonicsAndCase) {
    EXPECT_EQ(QStringLiteral("sample rate"), normalizePrefSearchText("&Sample Rate"));
    EXPECT_EQ(QStringLiteral("r&b"), normalizePrefSearchText("R&&B"));
}

TEST_F(PrefSearchTest, NormalizeRemovesRichText) {
    EXPECT_EQ(QStringLiteral("use the keylock"),
            normalizePrefSearchText("<html><body><b>Use</b> the keylock</body></html>"));
}

TEST_F(PrefSearchTest, QueryMatchesAllWordsInAnyOrder) {
    const QString text = normalizePrefSearchText("Audio buffer size");
    EXPECT_TRUE(prefSearchTextMatches(text, prefSearchQueryWords("Size audio")));
    EXPECT_FALSE(prefSearchTextMatches(text, prefSearchQueryWords("audio rate")));
    EXPECT_FALSE(prefSearchTextMatches(text, prefSearchQueryWords("   ")));
}

TEST_F(PrefSearchTest, CollectsTextOfAllInputWidgets) {
    QWidget page;
    auto* pLayout = new QVBoxLayout(&page);
    pLayout->addWidget(new QLabel("Sound API", &page));
    pLayout->addWidget(new QCheckBox("&Keylock", &page));
    auto* pGroupBox = new QGroupBox("Outputs", &page);
    pLayout->addWidget(pGroupBox);
    auto* pComboBox = new QComboBox(&page);
    pComboBox->addItem("ASIO");
    pLayout->addWidget(pComboBox);

    const QList<PrefSearchEntry> entries = collectPrefSearchEntries(&page);

    const PrefSearchEntry* pEntry =
            firstMatchingPrefSearchEntry(entries, prefSearchQueryWords("keylock"));
    ASSERT_NE(nullptr, pEntry);
    EXPECT_NE(nullptr, qobject_cast<QCheckBox*>(pEntry->pWidget.data()));
    pEntry = firstMatchingPrefSearchEntry(entries, prefSearchQueryWords("asio"));
    ASSERT_NE(nullptr, pEntry);
    EXPECT_EQ(pComboBox, pEntry->pWidget.data());
    EXPECT_NE(nullptr, firstMatchingPrefSearchEntry(entries, prefSearchQueryWords("outputs")));
    EXPECT_EQ(nullptr, firstMatchingPrefSearchEntry(entries, prefSearchQueryWords("vinyl")));
}

TEST_F(PrefSearchTest, SkipsHiddenWidgetsButKeepsOtherTabs) {
    QWidget page;
    auto* pHidden = new QLabel("Hidden option", &page);
    pHidden->hide();
    auto* pTabs = new QTabWidget(&page);
    auto* pFirstTab = new QWidget();
    auto* pSecondTab = new QWidget();
    pTabs->addTab(pFirstTab, "Output");
    pTabs->addTab(pSecondTab, "Input");
    auto* pOnSecondTab = new QLabel("Vinyl gain", pSecondTab);

    const QList<PrefSearchEntry> entries = collectPrefSearchEntries(&page);

    EXPECT_EQ(nullptr, firstMatchingPrefSearchEntry(entries, prefSearchQueryWords("hidden")));
    const PrefSearchEntry* pTabEntry =
            firstMatchingPrefSearchEntry(entries, prefSearchQueryWords("input"));
    ASSERT_NE(nullptr, pTabEntry);
    EXPECT_EQ(pSecondTab, pTabEntry->pWidget.data());
    const PrefSearchEntry* pLabelEntry =
            firstMatchingPrefSearchEntry(entries, prefSearchQueryWords("gain"));
    ASSERT_NE(nullptr, pLabelEntry);
    EXPECT_EQ(pOnSecondTab, pLabelEntry->pWidget.data());
}

} // namespace
// End of AI-generated text.
