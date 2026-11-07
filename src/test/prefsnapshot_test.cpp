// AI-generated (Claude Code). Start of AI-generated text.
#include "preferences/dialog/prefsnapshot.h"

#include <gtest/gtest.h>

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QScrollBar>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTableView>

#include "test/mixxxtest.h"

using namespace mixxx::preferences;

namespace {

class PrefSnapshotTest : public MixxxTest {};

TEST_F(PrefSnapshotTest, NoChangeAfterCapture) {
    QWidget page;
    new QCheckBox("Keylock", &page);
    auto* pCombo = new QComboBox(&page);
    pCombo->addItems({"ASIO", "WASAPI"});
    PrefSnapshot snapshot;
    snapshot.capture({&page});
    EXPECT_FALSE(snapshot.isEmpty());
    EXPECT_FALSE(snapshot.differsFromCurrent());
}

TEST_F(PrefSnapshotTest, ChangeAndChangeBack) {
    QWidget page;
    auto* pCheck = new QCheckBox("Keylock", &page);
    auto* pSpin = new QSpinBox(&page);
    auto* pEdit = new QLineEdit(&page);
    PrefSnapshot snapshot;
    snapshot.capture({&page});

    pCheck->setChecked(true);
    EXPECT_TRUE(snapshot.differsFromCurrent());
    pCheck->setChecked(false);
    EXPECT_FALSE(snapshot.differsFromCurrent());

    pSpin->setValue(5);
    EXPECT_TRUE(snapshot.differsFromCurrent());
    pSpin->setValue(0);

    pEdit->setText("Mix Two");
    EXPECT_TRUE(snapshot.differsFromCurrent());
}

TEST_F(PrefSnapshotTest, ComboBoxComparesTheText) {
    QWidget page;
    auto* pCombo = new QComboBox(&page);
    pCombo->addItems({"ASIO", "WASAPI"});
    pCombo->setCurrentIndex(1);
    PrefSnapshot snapshot;
    snapshot.capture({&page});
    // A new list with the same selected device is not a change.
    pCombo->clear();
    pCombo->addItems({"MME", "ASIO", "WASAPI"});
    pCombo->setCurrentText("WASAPI");
    EXPECT_FALSE(snapshot.differsFromCurrent());
    pCombo->setCurrentText("MME");
    EXPECT_TRUE(snapshot.differsFromCurrent());
}

TEST_F(PrefSnapshotTest, ScrollBarsAndReadOnlyTextAreIgnored) {
    QWidget page;
    auto* pScrollBar = new QScrollBar(&page);
    pScrollBar->setRange(0, 100);
    auto* pReadOnly = new QLineEdit(&page);
    pReadOnly->setReadOnly(true);
    PrefSnapshot snapshot;
    snapshot.capture({&page});
    pScrollBar->setValue(50);
    pReadOnly->setText("status");
    EXPECT_FALSE(snapshot.differsFromCurrent());
}

TEST_F(PrefSnapshotTest, ItemViewUsesOnlyUserCells) {
    QWidget page;
    auto* pModel = new QStandardItemModel(1, 2, &page);
    auto* pName = new QStandardItem("Connection 1");
    pName->setCheckable(true);
    auto* pStatus = new QStandardItem("Disconnected");
    pStatus->setEditable(false);
    pModel->setItem(0, 0, pName);
    pModel->setItem(0, 1, pStatus);
    auto* pView = new QTableView(&page);
    pView->setModel(pModel);
    PrefSnapshot snapshot;
    snapshot.capture({&page});

    // A status cell is not a setting.
    pStatus->setText("Connected");
    EXPECT_FALSE(snapshot.differsFromCurrent());

    pName->setCheckState(Qt::Checked);
    EXPECT_TRUE(snapshot.differsFromCurrent());
}

TEST_F(PrefSnapshotTest, DragOrderCounts) {
    QWidget page;
    auto* pList = new QListWidget(&page);
    pList->addItems({"Echo", "Filter"});
    for (int i = 0; i < pList->count(); ++i) {
        pList->item(i)->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    }
    pList->setDragDropMode(QAbstractItemView::InternalMove);
    PrefSnapshot snapshot;
    snapshot.capture({&page});
    pList->insertItem(0, pList->takeItem(1));
    EXPECT_TRUE(snapshot.differsFromCurrent());
}

TEST_F(PrefSnapshotTest, RemovedWidgetsAreIgnored) {
    QWidget page;
    auto* pCheck = new QCheckBox("Controller", &page);
    PrefSnapshot snapshot;
    snapshot.capture({&page});
    delete pCheck;
    EXPECT_FALSE(snapshot.differsFromCurrent());
}

} // namespace
// End of AI-generated text.
