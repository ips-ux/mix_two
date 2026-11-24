// AI-generated (Claude Code). Start of AI-generated text.
#include <gtest/gtest.h>

#include <QStandardItemModel>
#include <memory>

#include "library/library_prefs.h"
#include "preferences/usersettings.h"
#include "widget/holdtomenu.h"
#include "widget/prefixsearch.h"

namespace {

QStandardItemModel* makeModel() {
    auto* pModel = new QStandardItemModel(5, 2);
    const QStringList artists = {"Adele", "Bjork", "Boards of Canada", "Cher", "the Cure"};
    const QStringList titles = {"Zebra", "Yes", "Xylophone", "Wow", "Violet"};
    for (int row = 0; row < 5; ++row) {
        pModel->setItem(row, 0, new QStandardItem(artists[row]));
        pModel->setItem(row, 1, new QStandardItem(titles[row]));
    }
    return pModel;
}

TEST(PrefixSearchTest, FindsTheFirstRowInTheGivenColumn) {
    std::unique_ptr<QStandardItemModel> pModel(makeModel());
    EXPECT_EQ(1, findRowByPrefix(pModel.get(), 0, "B"));
    EXPECT_EQ(2, findRowByPrefix(pModel.get(), 0, "Bo"));
    // The sorted column decides, not another column.
    EXPECT_EQ(4, findRowByPrefix(pModel.get(), 1, "V"));
    EXPECT_EQ(-1, findRowByPrefix(pModel.get(), 0, "V"));
}

TEST(PrefixSearchTest, IgnoresCase) {
    std::unique_ptr<QStandardItemModel> pModel(makeModel());
    EXPECT_EQ(3, findRowByPrefix(pModel.get(), 0, "cH"));
    EXPECT_EQ(4, findRowByPrefix(pModel.get(), 0, "THE C"));
}

TEST(PrefixSearchTest, NoMatchForEmptyTextOrBadColumn) {
    std::unique_ptr<QStandardItemModel> pModel(makeModel());
    EXPECT_EQ(-1, findRowByPrefix(pModel.get(), 0, ""));
    EXPECT_EQ(-1, findRowByPrefix(pModel.get(), 7, "A"));
    EXPECT_EQ(-1, findRowByPrefix(pModel.get(), -1, "A"));
    EXPECT_EQ(-1, findRowByPrefix(nullptr, 0, "A"));
}

TEST(PrefixSearchBufferTest, KeysWithinOneSecondExtendTheText) {
    PrefixSearchBuffer buffer;
    EXPECT_EQ("b", buffer.add("b", 10000));
    EXPECT_EQ("bo", buffer.add("o", 10900));
    EXPECT_EQ("boa", buffer.add("a", 11800));
}

TEST(PrefixSearchBufferTest, ALaterKeyStartsANewText) {
    PrefixSearchBuffer buffer;
    EXPECT_EQ("b", buffer.add("b", 10000));
    EXPECT_EQ("c", buffer.add("c", 11001));
}

TEST(PrefixSearchBufferTest, ClearEndsTheSearch) {
    PrefixSearchBuffer buffer;
    buffer.add("b", 10000);
    EXPECT_TRUE(buffer.isActive(10500));
    buffer.clear();
    EXPECT_FALSE(buffer.isActive(10500));
    EXPECT_EQ("c", buffer.add("c", 10600));
}

TEST(PrefixSearchBufferTest, SpaceCountsOnlyInsideASearch) {
    EXPECT_TRUE(isPrefixSearchText("a", false));
    EXPECT_TRUE(isPrefixSearchText("7", false));
    EXPECT_FALSE(isPrefixSearchText(" ", false));
    EXPECT_TRUE(isPrefixSearchText(" ", true));
    EXPECT_FALSE(isPrefixSearchText("", false));
    EXPECT_FALSE(isPrefixSearchText("\r", false));
    EXPECT_FALSE(isPrefixSearchText("\x1b", false));
    EXPECT_FALSE(isPrefixSearchText("ab", false));
}

TEST(HoldToMenuTest, DefaultIsOffAndHoldIsOneSecond) {
    EXPECT_FALSE(holdtomenu::kEnabledDefault);
    EXPECT_EQ(1000, holdtomenu::kHoldMillis);
    // A fresh configuration gives the default.
    UserSettingsPointer pConfig(new UserSettings(QStringLiteral("nonexistent_trackviewkeys.cfg")));
    EXPECT_EQ(holdtomenu::kEnabledDefault,
            pConfig->getValue(mixxx::library::prefs::kHoldToOpenMenuConfigKey,
                    holdtomenu::kEnabledDefault));
}

TEST(HoldToMenuTest, MovingOutOfTheDragDistanceCancelsTheHold) {
    EXPECT_FALSE(holdtomenu::movedTooFar(QPoint(10, 10), QPoint(10, 10), 10));
    EXPECT_FALSE(holdtomenu::movedTooFar(QPoint(10, 10), QPoint(14, 16), 10));
    EXPECT_TRUE(holdtomenu::movedTooFar(QPoint(10, 10), QPoint(30, 10), 10));
}

} // namespace

// End of AI-generated text.
