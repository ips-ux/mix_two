// Tests for tableview-related things
// Right now it's just testing the serialize-unserialize of the header state code.
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <QAbstractTableModel>
#include <QtDebug>

#include "library/trackmodel.h"
#include "test/mixxxtest.h"
#include "proto/headers.pb.h"
#include "widget/wtracktableviewheader.h"

class HeaderViewStateTest : public testing::Test {
};

TEST_F(HeaderViewStateTest, RoundTrip) {
    mixxx::library::HeaderViewState headerViewState_pb;
    mixxx::library::HeaderViewState::HeaderState* header_state_pb =
                headerViewState_pb.add_header_state();

    header_state_pb->set_hidden(true);
    header_state_pb->set_size(50);
    header_state_pb->set_logical_index(10);
    header_state_pb->set_visual_index(2);
    header_state_pb->set_column_name("MyCol");

    header_state_pb =
                headerViewState_pb.add_header_state();

    header_state_pb->set_hidden(false);
    header_state_pb->set_size(22);
    header_state_pb->set_logical_index(6);
    header_state_pb->set_visual_index(3);
    header_state_pb->set_column_name("MyOtherCol");

    headerViewState_pb.set_sort_indicator_shown(true);
    headerViewState_pb.set_sort_indicator_section(1);
    headerViewState_pb.set_sort_order(Qt::DescendingOrder);

    // Create a HeaderViewState based on the proto.
    HeaderViewState view_state(headerViewState_pb);

    // Get a serialized form of the state.
    QString saved_state = view_state.saveState();

    // Initialize a new state object with the saved state.
    HeaderViewState loaded_state(saved_state);

    // Compare the old saved state with the new one.
    ASSERT_EQ(saved_state, loaded_state.saveState());

    // Ensure that the serialization is not bullshit.
    ASSERT_NE("", saved_state);
}

TEST_F(HeaderViewStateTest, GoodHeaderState) {
    const QString kGoodSerializedProto("ChEIARAAGAEgACoHcHJldmlldwoICAAQSxgEIAE"
            "KEggBEAAYAiACKghjb3ZlcmFydAoICAEQABgDIAMKCAgBEAAYBSAECggIABBaGBYgB"
            "QoICAEQABgGIAYKCQgAEPwBGBcgBwoJCAAQvAEYByAICgkIABDmARgIIAkKCAgBEAA"
            "YCSAKCggIARAAGAogCwoJCAAQkwQYGSAMCggIARAAGAwgDQoICAEQABgNIA4KCAgBE"
            "AAYDiAPCggIARAAGBAgEAoICAAQVxgRIBEKCAgBEAAYEiASCggIABBGGBMgEwoICAA"
            "QMhgPIBQKCAgAEHUYCyAVCggIARAAGBQgFgoICAAQNxgVIBcKCAgBEAAYGCAYCggIA"
            "RAAGBogGQoICAEQABgbIBoKCAgBEAAYHCAbCggIARAAGAAgHAoICAEQABgdIB0KCAg"
            "BEAAYHiAeEAEYFiAB");

    HeaderViewState view_state(kGoodSerializedProto);
    ASSERT_TRUE(view_state.healthy());
    ASSERT_EQ(kGoodSerializedProto, view_state.saveState());
}

TEST_F(HeaderViewStateTest, BadHeaderState) {
    HeaderViewState view_state("BLAHBLAHBLAHBAD");
    ASSERT_FALSE(view_state.healthy());
}

// AI-generated (Claude Code). Start of AI-generated text.
namespace {

// A table model with three columns. The column "bit_depth" is hidden by
// default, as in the library.
class FakeTrackModel : public QAbstractTableModel, public TrackModel {
  public:
    FakeTrackModel()
            : QAbstractTableModel(nullptr),
              TrackModel(QSqlDatabase(), "mixxx.db.model.test_header") {
    }

    int rowCount(const QModelIndex& = QModelIndex()) const override {
        return 0;
    }
    int columnCount(const QModelIndex& = QModelIndex()) const override {
        return 3;
    }
    QVariant data(const QModelIndex&, int = Qt::DisplayRole) const override {
        return QVariant();
    }
    QVariant headerData(int section, Qt::Orientation, int role) const override {
        static const QStringList names = {"artist", "bit_depth", "title"};
        if (role == TrackModel::kHeaderNameRole || role == Qt::DisplayRole) {
            return names.value(section);
        }
        return QVariant();
    }

    TrackPointer getTrack(const QModelIndex&) const override {
        return nullptr;
    }
    TrackPointer getTrackByRef(const TrackRef&) const override {
        return nullptr;
    }
    QUrl getTrackUrl(const QModelIndex&) const override {
        return QUrl();
    }
    QString getTrackLocation(const QModelIndex&) const override {
        return QString();
    }
    TrackId getTrackId(const QModelIndex&) const override {
        return TrackId();
    }
    CoverInfo getCoverInfo(const QModelIndex&) const override {
        return CoverInfo();
    }
    const QVector<int> getTrackRows(TrackId) const override {
        return {};
    }
    void search(const QString&) override {
    }
    const QString currentSearch() const override {
        return QString();
    }
    bool isColumnInternal(int) override {
        return false;
    }
    bool isColumnHiddenByDefault(int column) override {
        return column == 1;
    }
    SortColumnId sortColumnIdFromColumnIndex(int) const override {
        return SortColumnId::Artist;
    }
    int columnIndexFromSortColumnId(SortColumnId) const override {
        return 0;
    }
    QString modelKey(bool) const override {
        return QString();
    }
    bool updateTrackGenre(Track*, const QString&) const override {
        return false;
    }
#if defined(__EXTRA_METADATA__)
    bool updateTrackMood(Track*, const QString&) const override {
        return false;
    }
#endif
};

mixxx::library::HeaderViewState makeState(bool withBitDepth, bool bitDepthHidden) {
    mixxx::library::HeaderViewState pb;
    const auto add = [&pb](const char* name, bool hidden, int visual) {
        auto* pState = pb.add_header_state();
        pState->set_column_name(name);
        pState->set_hidden(hidden);
        pState->set_size(80);
        pState->set_logical_index(0);
        pState->set_visual_index(visual);
    };
    add("artist", false, 0);
    if (withBitDepth) {
        add("bit_depth", bitDepthHidden, 1);
    }
    add("title", false, withBitDepth ? 2 : 1);
    return pb;
}

} // namespace

class HeaderRestoreHiddenDefaultTest : public MixxxTest {
};

TEST_F(HeaderRestoreHiddenDefaultTest, NewColumnHiddenByDefaultStaysHidden) {
    FakeTrackModel model;
    WTrackTableViewHeader header(Qt::Horizontal);
    header.QHeaderView::setModel(&model);
    HeaderViewState state(makeState(false, false));
    state.restoreState(&header);
    EXPECT_FALSE(header.isSectionHidden(0));
    EXPECT_TRUE(header.isSectionHidden(1));
    EXPECT_FALSE(header.isSectionHidden(2));
}

TEST_F(HeaderRestoreHiddenDefaultTest, ColumnInSavedStateKeepsItsState) {
    FakeTrackModel model;
    WTrackTableViewHeader header(Qt::Horizontal);
    header.QHeaderView::setModel(&model);

    HeaderViewState shown(makeState(true, false));
    shown.restoreState(&header);
    EXPECT_FALSE(header.isSectionHidden(1));

    HeaderViewState hidden(makeState(true, true));
    hidden.restoreState(&header);
    EXPECT_TRUE(header.isSectionHidden(1));
}
// End of AI-generated text.
