// AI-generated (Claude Code). Start of AI-generated text.
// Tests for the library display preferences: sidebar labels, library font,
// artwork crop and "centre the selected track".
// End of AI-generated text.
#include <gtest/gtest.h>

#include <QApplication>
#include <cstdlib>
#include <QStandardItemModel>
#include <QTableView>

#include "library/library_prefs.h"
#include "library/libraryviewprefs.h"
#include "test/mixxxtest.h"

namespace {

using mixxx::library::coverArtPaintRects;
using mixxx::library::libraryFontFromConfig;
using mixxx::library::scrollRowToCenter;
using mixxx::library::sidebarLabel;

TEST(SidebarLabelTest, OffGivesThePlainName) {
    EXPECT_EQ(QStringLiteral("House"),
            sidebarLabel(QStringLiteral("House"), 12, QStringLiteral("1:02:03"), false));
    EXPECT_EQ(QStringLiteral("Empty"),
            sidebarLabel(QStringLiteral("Empty"), 0, QString(), false));
}

TEST(SidebarLabelTest, OnAddsCountAndDuration) {
    EXPECT_EQ(QStringLiteral("House (12) 1:02:03"),
            sidebarLabel(QStringLiteral("House"), 12, QStringLiteral("1:02:03"), true));
}

TEST(SidebarLabelTest, OnWithoutDurationAddsOnlyTheCount) {
    EXPECT_EQ(QStringLiteral("Empty (0)"),
            sidebarLabel(QStringLiteral("Empty"), 0, QString(), true));
}

TEST(SidebarLabelTest, DefaultIsOff) {
    EXPECT_FALSE(mixxx::library::prefs::kShowTrackCountInSidebarDefault);
}

TEST(LibraryFontTest, BoldSetsTheBoldWeight) {
    QFont base(QStringLiteral("Arial"), 11);
    const QFont font = libraryFontFromConfig(base.toString(), true, QFont());
    EXPECT_TRUE(font.bold());
    EXPECT_EQ(base.family(), font.family());
    EXPECT_EQ(base.pointSizeF(), font.pointSizeF());
}

TEST(LibraryFontTest, NotBoldClearsTheBoldWeight) {
    QFont base(QStringLiteral("Arial"), 11);
    base.setBold(true);
    const QFont font = libraryFontFromConfig(base.toString(), false, QFont());
    EXPECT_FALSE(font.bold());
}

TEST(LibraryFontTest, EmptyTextUsesTheFallback) {
    QFont fallback(QStringLiteral("Arial"), 13);
    const QFont font = libraryFontFromConfig(QString(), true, fallback);
    EXPECT_TRUE(font.bold());
    EXPECT_EQ(13.0, font.pointSizeF());
    const QFont plain = libraryFontFromConfig(QString(), false, fallback);
    EXPECT_FALSE(plain.bold());
}

TEST(LibraryFontTest, DefaultIsNotBold) {
    EXPECT_FALSE(mixxx::library::prefs::kLibraryFontBoldDefault);
}

TEST(CoverArtRectTest, UpperPartCutsTheBottom) {
    // The picture is 100 x 100 after scaling to the cell width; the cell is 100 x 40.
    const QRect cell(10, 20, 100, 40);
    const auto rects = coverArtPaintRects(false, QSize(100, 100), cell);
    EXPECT_EQ(cell, rects.target);
    EXPECT_EQ(QRect(0, 0, 100, 40), rects.source);
}

TEST(CoverArtRectTest, UpperPartKeepsASmallPicture) {
    // The picture is lower than the cell: nothing is cut off.
    const QRect cell(0, 0, 100, 80);
    const auto rects = coverArtPaintRects(false, QSize(100, 60), cell);
    EXPECT_EQ(QRect(0, 0, 100, 60), rects.target);
    EXPECT_EQ(QRect(0, 0, 100, 60), rects.source);
}

TEST(CoverArtRectTest, WholeScalesDownToTheCellHeight) {
    const QRect cell(10, 20, 100, 40);
    const auto rects = coverArtPaintRects(true, QSize(100, 100), cell);
    EXPECT_EQ(QRect(10, 20, 40, 40), rects.target);
    EXPECT_EQ(QRect(0, 0, 100, 100), rects.source);
}

TEST(CoverArtRectTest, WholeKeepsASmallPicture) {
    const QRect cell(0, 0, 100, 80);
    const auto rects = coverArtPaintRects(true, QSize(100, 60), cell);
    EXPECT_EQ(QRect(0, 0, 100, 60), rects.target);
    EXPECT_EQ(QRect(0, 0, 100, 60), rects.source);
}

TEST(CoverArtRectTest, EmptyInputGivesEmptyRects) {
    const auto rects = coverArtPaintRects(true, QSize(), QRect(0, 0, 100, 40));
    EXPECT_TRUE(rects.target.isEmpty());
    EXPECT_TRUE(rects.source.isEmpty());
}

TEST(CoverArtRectTest, DefaultIsUpperPart) {
    EXPECT_FALSE(mixxx::library::prefs::kCoverArtShowWholeDefault);
}

class CenterRowTest : public MixxxTest {};

TEST_F(CenterRowTest, SelectedRowIsInTheMiddleOfTheViewport) {
    QStandardItemModel model(200, 1);
    for (int row = 0; row < 200; ++row) {
        model.setItem(row, 0, new QStandardItem(QString::number(row)));
    }
    QTableView view;
    view.setModel(&model);
    view.resize(300, 400);
    view.show();
    QApplication::processEvents();

    const QModelIndex index = model.index(100, 0);
    view.setCurrentIndex(index);
    scrollRowToCenter(&view, index);
    QApplication::processEvents();

    const QRect rowRect = view.visualRect(index);
    ASSERT_TRUE(rowRect.isValid());
    const int rowMiddle = rowRect.center().y();
    const int viewportMiddle = view.viewport()->height() / 2;
    EXPECT_LE(std::abs(rowMiddle - viewportMiddle), rowRect.height());
}

TEST_F(CenterRowTest, DefaultIsOff) {
    EXPECT_FALSE(mixxx::library::prefs::kCenterSelectedTrackDefault);
}

} // namespace
