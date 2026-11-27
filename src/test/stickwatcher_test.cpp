// AI-generated (Claude Code). Start of AI-generated text.
#include <gtest/gtest.h>
#include <sqlite3.h>

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QMap>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QtEndian>
#include <tuple>

#include "coreservices.h"
#include "library/library.h"
#include "library/rekordbox/devicelibraryplusdb.h"
#include "library/rekordbox/rekordboxfeature.h"
#include "library/rekordbox/stickwatcher.h"
#include "library/trackcollection.h"
#include "library/treeitem.h"
#include "library/trackcollectionmanager.h"
#include "test/mixxxtest.h"
#include "util/cmdlineargs.h"
#include "util/versionstore.h"

namespace mixxx::rekordbox {
namespace {

class StickWatcherFormats : public testing::TestWithParam<std::tuple<QStringList, bool, bool>> {};

TEST_P(StickWatcherFormats, DetectsExportFiles) {
    const auto& [paths, legacy, plus] = GetParam();
    QTemporaryDir stick;
    ASSERT_TRUE(stick.isValid());
    for (const auto& path : paths) {
        const auto filePath = QDir(stick.path()).filePath(path);
        ASSERT_TRUE(QDir().mkpath(QFileInfo(filePath).path()));
        QFile file(filePath);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    }
    const auto formats = detectStickFormats(stick.path());
    EXPECT_EQ(formats.hasLegacy(), legacy);
    EXPECT_EQ(formats.hasDeviceLibraryPlus(), plus);
}

INSTANTIATE_TEST_SUITE_P(Layouts,
        StickWatcherFormats,
        testing::Values(
                std::make_tuple(QStringList{"PIONEER/rekordbox/export.pdb"}, true, false),
                std::make_tuple(QStringList{"PIONEER/rekordbox/exportLibrary.db"}, false, true),
                std::make_tuple(QStringList{"PIONEER/rekordbox/export.pdb",
                                        "PIONEER/rekordbox/exportLibrary.db"},
                        true,
                        true),
                std::make_tuple(QStringList{}, false, false),
                std::make_tuple(QStringList{"pIoNeEr/REKORDBOX/EXPORT.PDB",
                                        "pIoNeEr/REKORDBOX/EXPORTLIBRARY.DB"},
                        true,
                        true),
                std::make_tuple(QStringList{".PIONEER/rekordbox/export.pdb"}, true, false)));

TEST(StickWatcher, EmitsOneSignalPerVolumeTransition) {
    StickWatcher watcher;
    QStringList mounted;
    QStringList unmounted;
    QObject::connect(&watcher, &StickWatcher::volumeAdded, &watcher,
            [&](const QString& root) { mounted.append(root); });
    QObject::connect(&watcher, &StickWatcher::volumeRemoved, &watcher,
            [&](const QString& root) { unmounted.append(root); });
    watcher.volumeArrived("X:/");
    watcher.volumeArrived("X:/");
    watcher.volumeArrived("Y:/");
    watcher.volumeDeparted("X:/");
    watcher.volumeDeparted("X:/");
    watcher.volumeArrived("X:/");
    EXPECT_EQ(mounted, (QStringList{"X:/", "Y:/", "X:/"}));
    EXPECT_EQ(unmounted, QStringList{"X:/"});
}

TEST(StickWatcher, DecodesDriveLetterMask) {
    EXPECT_TRUE(driveRootsFromMask(0).isEmpty());
    EXPECT_EQ(driveRootsFromMask(0x82000005u), (QStringList{"A:/", "C:/", "Z:/"}));
}

TEST(StickWatcher, ReplacesDeviceAtSameMountPoint) {
    StickWatcher watcher;
    QStringList events;
    QObject::connect(&watcher, &StickWatcher::volumeAdded, &watcher,
            [&](const QString& root) { events.append("+" + root); });
    QObject::connect(&watcher, &StickWatcher::volumeRemoved, &watcher,
            [&](const QString& root) { events.append("-" + root); });
    watcher.applySnapshot({{"/mnt/usb", "/dev/sdb1"}, {"/mnt/other", "/dev/sdb1"}}, false);
    events.clear();
    watcher.applySnapshot({{"/mnt/usb", "/dev/sdc1"}, {"/mnt/other", "/dev/sdb1"}}, false);
    watcher.applySnapshot({{"/mnt/usb", "/dev/sdc1"}, {"/mnt/other", "/dev/sdb1"}}, false);
    EXPECT_EQ(events, (QStringList{"-/mnt/usb", "+/mnt/usb"}));
}

bool writeOneTrackPdb(const QDir& root, const QByteArray& title) {
    constexpr int kPageSize = 4096;
    constexpr int kTrack = kPageSize + 40;
    constexpr quint16 kStrings = 136;
    QByteArray bytes(2 * kPageSize, '\0');
    const auto put16 = [&bytes](int offset, quint16 value) {
        qToLittleEndian(value, bytes.data() + offset);
    };
    const auto put32 = [&bytes](int offset, quint32 value) {
        qToLittleEndian(value, bytes.data() + offset);
    };
    put32(4, kPageSize);
    put32(8, 1); // One table: tracks (type zero), on page one.
    put32(36, 1);
    put32(40, 1);
    put32(kPageSize + 4, 1);
    put32(kPageSize + 24, 1 | (1 << 13)); // One row, one valid row.
    put16(2 * kPageSize - 4, 1); // First row present, at heap offset zero.
    put32(kTrack + 72, 1); // Track ID.
    for (int i = 0; i < 21; ++i) {
        put16(kTrack + 94 + 2 * i, kStrings);
    }
    const auto putString = [&bytes, kTrack](int offset, const QByteArray& text) {
        bytes[kTrack + offset] = static_cast<char>(2 * (text.size() + 1) + 1);
        bytes.replace(kTrack + offset + 1, text.size(), text);
    };
    putString(kStrings, {});
    put16(kTrack + 94 + 2 * 17, kStrings + 1);
    putString(kStrings + 1, title);
    const auto pathOffset = static_cast<quint16>(kStrings + 2 + title.size());
    put16(kTrack + 94 + 2 * 20, pathOffset);
    putString(pathOffset, "/track.wav");
    QFile pdb(root.filePath("PIONEER/rekordbox/export.pdb"));
    return root.mkpath("PIONEER/rekordbox") && pdb.open(QIODevice::WriteOnly) &&
            pdb.write(bytes) == bytes.size();
}


/// Gives the tests the SQLite handle of a library, to set columns that
/// DlpContent does not hold.
struct DlpHandle : DeviceLibraryPlusDb {
    static sqlite3* of(DeviceLibraryPlusDb& db) {
        return (db.*&DlpHandle::handle)();
    }
};

struct PlusTrack {
    QString title;
    QString artist;
    QString album;
    QString genre;
    QString key;
    int year = 0;
    int lengthSeconds = 0;
    int rating = 0;
    int bpmx100 = 0;
    int bitrate = 0;
    int colorId = 0;
    int trackNumber = 0;
    QString comment;
    QString path;
    QString anlzPath;
};

QList<PlusTrack> threePlusTracks() {
    return {
            {"Plus One",
                    "Artist A",
                    "Album A",
                    "House",
                    "8A",
                    2021,
                    245,
                    4,
                    12450,
                    320,
                    5,
                    7,
                    "first",
                    "/Contents/Artist A/one.mp3",
                    "/PIONEER/USBANLZ/P001/00000001/ANLZ0000.DAT"},
            {"Plus Two",
                    "Artist B",
                    "Album B",
                    "Techno",
                    "11B",
                    1999,
                    360,
                    2,
                    13000,
                    256,
                    6,
                    2,
                    "second",
                    "/Contents/Artist B/two.mp3",
                    "/PIONEER/USBANLZ/P002/00000002/ANLZ0000.DAT"},
            {"Plus Three",
                    "Artist A",
                    "Album A",
                    "House",
                    "8A",
                    2021,
                    180,
                    0,
                    12000,
                    1411,
                    0,
                    3,
                    QString(),
                    "/Contents/Artist A/three.wav",
                    "/PIONEER/USBANLZ/P003/00000003/ANLZ0000.DAT"},
    };
}

/// Writes PIONEER/rekordbox/exportLibrary.db with these tracks. When
/// withPlaylists is true, it adds the playlist "Top list" (tracks 3 and 1)
/// and the folder "Folder A" with the playlist "Inner list" (track 2), in
/// this order.
bool writePlusLibrary(const QDir& root, const QList<PlusTrack>& tracks, bool withPlaylists) {
    QString error;
    auto pDb = DeviceLibraryPlusDb::create(root.path(), QStringLiteral("TEST STICK"), 1234, &error);
    if (!pDb) {
        ADD_FAILURE() << qPrintable(error);
        return false;
    }
    QList<qint64> ids;
    for (const auto& track : tracks) {
        DlpContent content;
        content.title = track.title;
        content.artistId = pDb->findOrAddArtist(track.artist);
        content.albumId = pDb->findOrAddAlbum(track.album, content.artistId);
        content.genreId = pDb->findOrAddGenre(track.genre);
        content.keyId = pDb->findOrAddKey(track.key);
        content.releaseYear = track.year;
        content.lengthSeconds = track.lengthSeconds;
        content.rating = track.rating;
        content.bpmx100 = track.bpmx100;
        content.bitrate = track.bitrate;
        content.colorId = track.colorId;
        content.djComment = track.comment;
        content.path = track.path;
        content.fileName = track.path.section(QLatin1Char('/'), -1);
        content.fileType = track.path.endsWith(QStringLiteral(".wav")) ? 11 : 1;
        content.dateAdded = QStringLiteral("2026-11-25");
        content.dateCreated = QStringLiteral("2026-11-25");
        content.analysisDataFilePath = track.anlzPath;
        const qint64 id = pDb->addContent(content);
        const QByteArray sql = QStringLiteral("UPDATE content SET trackNo = %1 WHERE content_id = %2")
                                       .arg(track.trackNumber)
                                       .arg(id)
                                       .toUtf8();
        if (id < 0 ||
                sqlite3_exec(DlpHandle::of(*pDb), sql.constData(), nullptr, nullptr, nullptr) !=
                        SQLITE_OK) {
            ADD_FAILURE() << "cannot add a track";
            return false;
        }
        ids.append(id);
    }
    if (withPlaylists && ids.size() == 3) {
        const qint64 top = pDb->addPlaylist(QStringLiteral("Top list"), 0, false);
        const qint64 folder = pDb->addPlaylist(QStringLiteral("Folder A"), 0, true);
        const qint64 inner = pDb->addPlaylist(QStringLiteral("Inner list"), folder, false);
        pDb->setPlaylistContents(top, {ids[2], ids[0]});
        pDb->setPlaylistContents(inner, {ids[1]});
    }
    pDb->updateContentCount();
    if (!pDb->lastErrorMessage().isEmpty() || !pDb->save(&error)) {
        ADD_FAILURE() << qPrintable(pDb->lastErrorMessage()) << qPrintable(error);
        return false;
    }
    return true;
}

/// Each file and folder below the root, with its size, the time of its last
/// change and the SHA-256 of its bytes.
QMap<QString, QString> stickSnapshot(const QDir& root) {
    QMap<QString, QString> result;
    QDirIterator it(root.path(),
            QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
            QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo info = it.fileInfo();
        QString value = QString::number(info.size()) + QLatin1Char(' ') +
                QString::number(info.lastModified().toMSecsSinceEpoch());
        if (info.isFile()) {
            QFile file(info.filePath());
            if (file.open(QIODevice::ReadOnly)) {
                value += QLatin1Char(' ') +
                        QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256)
                                .toHex();
            }
        }
        result.insert(root.relativeFilePath(info.filePath()), value);
    }
    return result;
}

} // namespace

class StickWatcherFeature : public MixxxTest {
  protected:
    void SetUp() override {
        m_previousArgs = CmdlineArgs::Instance();
        ASSERT_TRUE(m_profile.isValid());
        const QString settingsPath = m_profile.path() + QLatin1Char('/');
        UserSettings settings(settingsPath + "mixxx.cfg");
        settings.setValue(ConfigKey("[Config]", "Version"), VersionStore::FUTURE_UNSTABLE);
        settings.save();
        QFile soundConfig(settingsPath + "soundconfig.xml");
        ASSERT_TRUE(soundConfig.open(QIODevice::WriteOnly));
        ASSERT_GT(soundConfig.write("<SoundManagerConfig api=\"None\"/>\n"), 0);
        soundConfig.close();

        CmdlineArgs::Instance().setSettingsPath(settingsPath);
        CmdlineArgs args;
        args.setSettingsPath(settingsPath);
        application()->installEventFilter(&m_dialogFilter);
        m_services = std::make_shared<CoreServices>(args, application());
        m_services->initialize(application());
        application()->removeEventFilter(&m_dialogFilter);
    }

    void TearDown() override {
        m_services.reset();
        restoreTestLogging();
        CmdlineArgs::Instance() = m_previousArgs;
    }

    template<typename Predicate>
    bool waitUntil(Predicate ready) {
        QElapsedTimer timer;
        timer.start();
        while (!ready() && timer.elapsed() < 5000) {
            QCoreApplication::processEvents();
            QThread::msleep(1);
        }
        return ready();
    }

    RekordboxFeature* feature() const {
        return m_services->getLibrary()->findChild<RekordboxFeature*>();
    }

    static QModelIndex deviceIndex(RekordboxFeature* feature, const QDir& root) {
        auto* sidebar = feature->sidebarModel();
        for (int row = 0; row < sidebar->rowCount(); ++row) {
            const auto index = sidebar->index(row, 0);
            if (index.data().toString() == root.dirName()) {
                return index;
            }
        }
        return {};
    }

    static bool probePending(RekordboxFeature* feature, const QString& root) {
        return feature->m_probes.contains(root);
    }

    static void deliverProbeOnly(RekordboxFeature* feature, const QString& root) {
        auto* probe = feature->m_probes.value(root);
        ASSERT_NE(probe, nullptr);
        probe->waitForFinished();
        QCoreApplication::sendPostedEvents(probe, QEvent::FutureCallOut);
    }

    static bool databaseJobPending(RekordboxFeature* feature) {
        return feature->m_databaseBusy;
    }

    static QString plusReadError(RekordboxFeature* feature, const QDir& root) {
        for (auto it = feature->m_plusReadErrors.cbegin();
                it != feature->m_plusReadErrors.cend();
                ++it) {
            if (QDir(it.key()) == root) {
                return it.value();
            }
        }
        return {};
    }

    static RekordboxFeature::DeleteFromStickMenuState deleteMenuState(
            RekordboxFeature* feature, const QModelIndex& index) {
        return feature->deleteFromStickMenuState(index);
    }

    /// The child of `parent` with this label.
    static QModelIndex childIndex(
            RekordboxFeature* feature, const QModelIndex& parent, const QString& label) {
        auto* sidebar = feature->sidebarModel();
        for (int row = 0; row < sidebar->rowCount(parent); ++row) {
            const auto index = sidebar->index(row, 0, parent);
            if (index.data().toString() == label) {
                return index;
            }
        }
        return {};
    }

    /// Adds a playlist item below the stick item, with the data that the
    /// reader gives a playlist. The legacy test stick has no playlist.
    static void addPlaylistItem(RekordboxFeature* feature, const QDir& root, const QString& name) {
        const auto device = deviceIndex(feature, root);
        ASSERT_TRUE(device.isValid());
        const auto data = device.data(TreeItemModel::kDataRole).toList();
        ASSERT_EQ(data.size(), 2);
        std::vector<std::unique_ptr<TreeItem>> items;
        items.push_back(std::make_unique<TreeItem>(name,
                QVariant(QList<QString>{
                        data[0].toString() + QStringLiteral("-->") + name,
                        data[1].toString()})));
        feature->m_pSidebarModel->insertTreeItemRows(std::move(items), 0, device);
    }

    QSqlDatabase appDatabase() const {
        return m_services->getTrackCollectionManager()->internalCollection()->database();
    }

    /// Opens the stick in the feature and waits for its track view.
    RekordboxPlaylistModel* openStick(RekordboxFeature* feature, const QDir& root) {
        auto* watcher = feature->findChild<StickWatcher*>();
        if (!watcher) {
            ADD_FAILURE() << "no stick watcher";
            return nullptr;
        }
        RekordboxPlaylistModel* shown = nullptr;
        QObject context;
        QObject::connect(feature, &LibraryFeature::showTrackModel, &context,
                [&](QAbstractItemModel* model) {
                    shown = qobject_cast<RekordboxPlaylistModel*>(model);
                    if (shown) {
                        shown->select();
                    }
                });
        watcher->volumeArrived(root.path());
        if (!waitUntil([&] { return deviceIndex(feature, root).isValid(); })) {
            ADD_FAILURE() << "the stick is not in the device list";
            return nullptr;
        }
        feature->activateChild(deviceIndex(feature, root));
        if (!waitUntil([&] { return shown != nullptr; })) {
            ADD_FAILURE() << "the stick does not open";
        }
        return shown;
    }

    std::shared_ptr<CoreServices> m_services;

  private:
    class RejectFileDialogs final : public QObject {
        bool eventFilter(QObject* object, QEvent* event) override {
            if (event->type() == QEvent::Show) {
                if (auto* dialog = qobject_cast<QFileDialog*>(object)) {
                    QTimer::singleShot(0, dialog, &QDialog::reject);
                }
            }
            return false;
        }
    } m_dialogFilter;
    QTemporaryDir m_profile;
    CmdlineArgs m_previousArgs;
};

TEST_F(StickWatcherFeature, RemovingOpenStickClearsPlaylistModel) {
    QTemporaryDir stick;
    ASSERT_TRUE(stick.isValid());
    QDir root(stick.path());
    ASSERT_TRUE(writeOneTrackPdb(root, "One track"));

    auto* feature = this->feature();
    ASSERT_NE(feature, nullptr);
    auto* watcher = feature->findChild<StickWatcher*>();
    ASSERT_NE(watcher, nullptr);
    QObject context;
    RekordboxPlaylistModel* shown = nullptr;
    QString view;
    QObject::connect(feature, &LibraryFeature::showTrackModel, &context,
            [&](QAbstractItemModel* model) {
                shown = qobject_cast<RekordboxPlaylistModel*>(model);
                if (shown) {
                    shown->select();
                }
            });
    QObject::connect(feature, &LibraryFeature::switchToView, &context,
            [&](const QString& name) { view = name; });

    const auto findDevice = [&] { return deviceIndex(feature, root); };
    watcher->volumeArrived(stick.path());
    ASSERT_TRUE(waitUntil([&] { return findDevice().isValid(); }));
    feature->activateChild(findDevice());
    ASSERT_TRUE(waitUntil([&] { return shown != nullptr; }));
    ASSERT_EQ(shown->rowCount(), 1);
    const QString playlistKey = shown->modelKey(false);

    watcher->volumeDeparted(stick.path());
    EXPECT_FALSE(findDevice().isValid());
    EXPECT_EQ(view, QStringLiteral("REKORDBOXHOME"));
    EXPECT_EQ(shown->rowCount(), 0);
    EXPECT_NE(shown->modelKey(false), playlistKey);
}

TEST_F(StickWatcherFeature, OpensReplacementRequestedBeforeCanceledImportCompletes) {
    QTemporaryDir stick;
    ASSERT_TRUE(stick.isValid());
    const QDir root(stick.path());
    ASSERT_TRUE(writeOneTrackPdb(root, "First stick"));
    auto* feature = this->feature();
    ASSERT_NE(feature, nullptr);
    auto* watcher = feature->findChild<StickWatcher*>();
    ASSERT_NE(watcher, nullptr);
    QObject context;
    RekordboxPlaylistModel* shown = nullptr;
    QObject::connect(feature, &LibraryFeature::showTrackModel, &context,
            [&](QAbstractItemModel* model) {
                shown = qobject_cast<RekordboxPlaylistModel*>(model);
                if (shown) {
                    shown->select();
                }
            });

    watcher->volumeArrived(root.path());
    ASSERT_TRUE(waitUntil([&] { return deviceIndex(feature, root).isValid(); }));
    feature->activateChild(deviceIndex(feature, root));
    ASSERT_TRUE(databaseJobPending(feature));
    watcher->volumeDeparted(root.path());
    watcher->volumeArrived(root.path());
    // Deliver the new probe without delivering the canceled import's completion.
    ASSERT_NO_FATAL_FAILURE(deliverProbeOnly(feature, root.path()));
    ASSERT_TRUE(deviceIndex(feature, root).isValid());
    feature->activateChild(deviceIndex(feature, root));
    ASSERT_EQ(shown, nullptr);
    ASSERT_TRUE(waitUntil([&] { return !databaseJobPending(feature); }));
    ASSERT_NE(shown, nullptr);
    EXPECT_EQ(shown->rowCount(), 1);
}

TEST_F(StickWatcherFeature, ListsAStickWithOnlyDeviceLibraryPlus) {
    QTemporaryDir stick;
    ASSERT_TRUE(stick.isValid());
    const QDir root(stick.path());
    ASSERT_TRUE(writePlusLibrary(root, threePlusTracks(), false));
    auto* feature = this->feature();
    ASSERT_NE(feature, nullptr);
    auto* watcher = feature->findChild<StickWatcher*>();
    ASSERT_NE(watcher, nullptr);
    watcher->volumeArrived(root.path());
    EXPECT_TRUE(waitUntil([&] { return deviceIndex(feature, root).isValid(); }));
}

TEST_F(StickWatcherFeature, ReadsTracksAndPlaylistsFromDeviceLibraryPlus) {
    QTemporaryDir stick;
    ASSERT_TRUE(stick.isValid());
    const QDir root(stick.path());
    const auto tracks = threePlusTracks();
    ASSERT_TRUE(writePlusLibrary(root, tracks, true));
    auto* feature = this->feature();
    ASSERT_NE(feature, nullptr);
    auto* shown = openStick(feature, root);
    ASSERT_NE(shown, nullptr);
    EXPECT_EQ(shown->rowCount(), 3);

    QSqlQuery query(appDatabase());
    query.prepare(QStringLiteral(
            "SELECT title, artist, album, genre, year, duration, location, rating, "
            "comment, tracknumber, key, bpm, bitrate, analyze_path, color "
            "FROM rekordbox_library WHERE device = :device ORDER BY rb_id"));
    query.bindValue(":device", root.path());
    ASSERT_TRUE(query.exec());
    const QList<int> colors{0x1EE000, 0x16C0F8, 0};
    int row = 0;
    while (query.next()) {
        ASSERT_LT(row, tracks.size());
        const auto& track = tracks[row];
        EXPECT_EQ(query.value(0).toString(), track.title);
        EXPECT_EQ(query.value(1).toString(), track.artist);
        EXPECT_EQ(query.value(2).toString(), track.album);
        EXPECT_EQ(query.value(3).toString(), track.genre);
        EXPECT_EQ(query.value(4).toInt(), track.year);
        EXPECT_EQ(query.value(5).toInt(), track.lengthSeconds);
        EXPECT_EQ(query.value(6).toString(), root.path() + track.path);
        EXPECT_EQ(query.value(7).toInt(), track.rating);
        EXPECT_EQ(query.value(8).toString(), track.comment);
        EXPECT_EQ(query.value(9).toInt(), track.trackNumber);
        EXPECT_EQ(query.value(10).toString(), track.key);
        EXPECT_DOUBLE_EQ(query.value(11).toDouble(), track.bpmx100 / 100.0);
        EXPECT_EQ(query.value(12).toInt(), track.bitrate);
        EXPECT_EQ(query.value(13).toString(), root.path() + track.anlzPath);
        EXPECT_EQ(query.value(14).toInt(), colors[row]);
        ++row;
    }
    EXPECT_EQ(row, 3);

    // The sidebar shows the playlists and folders in the order of the stick.
    const auto device = deviceIndex(feature, root);
    auto* sidebar = feature->sidebarModel();
    ASSERT_EQ(sidebar->rowCount(device), 2);
    EXPECT_EQ(sidebar->index(0, 0, device).data().toString(), QStringLiteral("Top list"));
    const auto folder = sidebar->index(1, 0, device);
    EXPECT_EQ(folder.data().toString(), QStringLiteral("Folder A"));
    ASSERT_EQ(sidebar->rowCount(folder), 1);
    EXPECT_EQ(sidebar->index(0, 0, folder).data().toString(), QStringLiteral("Inner list"));

    const auto titlesOf = [&](const QString& playlist) {
        QSqlQuery entries(appDatabase());
        entries.prepare(QStringLiteral(
                "SELECT library.title FROM rekordbox_playlist_tracks AS entry "
                "JOIN rekordbox_playlists AS list ON list.id = entry.playlist_id "
                "JOIN rekordbox_library AS library ON library.id = entry.track_id "
                "WHERE list.name = :name ORDER BY entry.position"));
        entries.bindValue(":name", playlist);
        QStringList titles;
        if (entries.exec()) {
            while (entries.next()) {
                titles.append(entries.value(0).toString());
            }
        }
        return titles;
    };
    const QString delimiter = QStringLiteral("-->");
    EXPECT_EQ(titlesOf(root.path()), (QStringList{"Plus One", "Plus Two", "Plus Three"}));
    EXPECT_EQ(titlesOf(root.path() + delimiter + "Top list"),
            (QStringList{"Plus Three", "Plus One"}));
    EXPECT_EQ(titlesOf(root.path() + delimiter + "Folder A" + delimiter + "Inner list"),
            QStringList{"Plus Two"});
}

TEST_F(StickWatcherFeature, PrefersDeviceLibraryPlusWhenBothFormatsExist) {
    QTemporaryDir stick;
    ASSERT_TRUE(stick.isValid());
    const QDir root(stick.path());
    ASSERT_TRUE(writeOneTrackPdb(root, "Legacy title"));
    ASSERT_TRUE(writePlusLibrary(root, {threePlusTracks().first()}, false));
    auto* feature = this->feature();
    ASSERT_NE(feature, nullptr);
    auto* shown = openStick(feature, root);
    ASSERT_NE(shown, nullptr);
    ASSERT_EQ(shown->rowCount(), 1);
    EXPECT_EQ(shown->data(shown->index(0, shown->fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_TITLE)))
                      .toString(),
            QStringLiteral("Plus One"));
}

TEST_F(StickWatcherFeature, ReadingADeviceLibraryPlusStickChangesNothingOnIt) {
    QTemporaryDir stick;
    ASSERT_TRUE(stick.isValid());
    const QDir root(stick.path());
    ASSERT_TRUE(writePlusLibrary(root, threePlusTracks(), true));
    ASSERT_TRUE(writeOneTrackPdb(root, "Legacy title"));
    // Some file systems set the time of a folder a moment after a change in it.
    QThread::msleep(2000);
    const auto before = stickSnapshot(root);
    ASSERT_FALSE(before.isEmpty());
    auto* feature = this->feature();
    ASSERT_NE(feature, nullptr);
    auto* shown = openStick(feature, root);
    ASSERT_NE(shown, nullptr);
    EXPECT_EQ(shown->rowCount(), 3);
    auto* watcher = feature->findChild<StickWatcher*>();
    ASSERT_NE(watcher, nullptr);
    watcher->volumeDeparted(root.path());
    ASSERT_TRUE(waitUntil([&] { return !databaseJobPending(feature); }));
    const auto after = stickSnapshot(root);
    QStringList changes;
    for (auto it = before.cbegin(); it != before.cend(); ++it) {
        if (after.value(it.key()) != it.value()) {
            changes.append(it.key() + QStringLiteral(": ") + it.value() + QStringLiteral(" -> ") +
                    after.value(it.key(), QStringLiteral("gone")));
        }
    }
    for (auto it = after.cbegin(); it != after.cend(); ++it) {
        if (!before.contains(it.key())) {
            changes.append(it.key() + QStringLiteral(": new"));
        }
    }
    EXPECT_TRUE(changes.isEmpty()) << qPrintable(changes.join(QLatin1Char('\n')));
}

TEST_F(StickWatcherFeature, FallsBackToTheLegacyLibraryWhenDeviceLibraryPlusCannotBeRead) {
    QTemporaryDir stick;
    ASSERT_TRUE(stick.isValid());
    const QDir root(stick.path());
    ASSERT_TRUE(writeOneTrackPdb(root, "Legacy title"));
    QFile broken(root.filePath(QStringLiteral("PIONEER/rekordbox/exportLibrary.db")));
    ASSERT_TRUE(broken.open(QIODevice::WriteOnly));
    ASSERT_EQ(broken.write(QByteArray(8192, 'x')), 8192);
    broken.close();
    auto* feature = this->feature();
    ASSERT_NE(feature, nullptr);
    auto* shown = openStick(feature, root);
    ASSERT_NE(shown, nullptr);
    ASSERT_EQ(shown->rowCount(), 1);
    EXPECT_EQ(shown->data(shown->index(0, shown->fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_TITLE)))
                      .toString(),
            QStringLiteral("Legacy title"));
    EXPECT_FALSE(plusReadError(feature, root).isEmpty());
}

#if defined(Q_OS_WIN)
TEST_F(StickWatcherFeature, DetectsAnExportAfterSlowWindowsMount) {
    QTemporaryDir stick;
    ASSERT_TRUE(stick.isValid());
    const QDir root(stick.path());
    auto* feature = this->feature();
    ASSERT_NE(feature, nullptr);
    auto* watcher = feature->findChild<StickWatcher*>();
    ASSERT_NE(watcher, nullptr);
    QTimer readiness;
    readiness.setSingleShot(true);
    QObject::connect(&readiness, &QTimer::timeout, &readiness,
            [&] { EXPECT_TRUE(writeOneTrackPdb(root, "Slow mount")); });
    watcher->volumeArrived(root.path());
    readiness.start(1500);
    ASSERT_TRUE(waitUntil([&] { return deviceIndex(feature, root).isValid(); }));
}

TEST_F(StickWatcherFeature, ReprobesWindowsRootAfterAnExhaustedProbe) {
    QTemporaryDir stick;
    ASSERT_TRUE(stick.isValid());
    const QDir root(stick.path());
    auto* feature = this->feature();
    ASSERT_NE(feature, nullptr);
    auto* watcher = feature->findChild<StickWatcher*>();
    ASSERT_NE(watcher, nullptr);
    watcher->volumeArrived(root.path());
    ASSERT_TRUE(probePending(feature, root.path()));
    ASSERT_TRUE(waitUntil([&] { return !probePending(feature, root.path()); }));
    ASSERT_FALSE(deviceIndex(feature, root).isValid());
    ASSERT_TRUE(writeOneTrackPdb(root, "Late mount"));
    watcher->volumeArrived(root.path());
    ASSERT_TRUE(waitUntil([&] { return deviceIndex(feature, root).isValid(); }));
    watcher->volumeArrived(root.path());
    EXPECT_FALSE(probePending(feature, root.path()));
}

TEST_F(StickWatcherFeature, ProbesAnEmptyRootFromTheStartupScanOnce) {
    QTemporaryDir stick;
    ASSERT_TRUE(stick.isValid());
    const QDir root(stick.path());
    auto* feature = this->feature();
    ASSERT_NE(feature, nullptr);
    auto* watcher = feature->findChild<StickWatcher*>();
    ASSERT_NE(watcher, nullptr);
    watcher->applySnapshot({{root.path(), {}}}, false);
    ASSERT_TRUE(probePending(feature, root.path()));
    deliverProbeOnly(feature, root.path());
    // A retry keeps the probe pending while its timer waits.
    EXPECT_FALSE(probePending(feature, root.path()));
    EXPECT_FALSE(deviceIndex(feature, root).isValid());
}
#endif

TEST_F(StickWatcherFeature, DeleteFromStickIsOnTheListsOfADeviceLibraryPlusStick) {
    QTemporaryDir stick;
    ASSERT_TRUE(stick.isValid());
    const QDir root(stick.path());
    ASSERT_TRUE(writePlusLibrary(root, threePlusTracks(), true));
    auto* feature = this->feature();
    ASSERT_NE(feature, nullptr);
    ASSERT_NE(openStick(feature, root), nullptr);
    const auto device = deviceIndex(feature, root);
    ASSERT_TRUE(device.isValid());

    EXPECT_FALSE(deleteMenuState(feature, device).visible);
    const auto playlist = childIndex(feature, device, QStringLiteral("Top list"));
    ASSERT_TRUE(playlist.isValid());
    const auto playlistState = deleteMenuState(feature, playlist);
    EXPECT_TRUE(playlistState.visible);
    EXPECT_TRUE(playlistState.enabled);
    const auto inner = childIndex(feature,
            childIndex(feature, device, QStringLiteral("Folder A")),
            QStringLiteral("Inner list"));
    ASSERT_TRUE(inner.isValid());
    EXPECT_TRUE(deleteMenuState(feature, inner).enabled);
    // A folder is refused: the item shows, disabled, with the reason.
    const auto folderState =
            deleteMenuState(feature, childIndex(feature, device, QStringLiteral("Folder A")));
    EXPECT_TRUE(folderState.visible);
    EXPECT_FALSE(folderState.enabled);
    EXPECT_FALSE(folderState.toolTip.isEmpty());
}

TEST_F(StickWatcherFeature, DeleteFromStickIsDisabledOnALegacyOnlyStick) {
    QTemporaryDir stick;
    ASSERT_TRUE(stick.isValid());
    const QDir root(stick.path());
    ASSERT_TRUE(writeOneTrackPdb(root, "Legacy only"));
    auto* feature = this->feature();
    ASSERT_NE(feature, nullptr);
    ASSERT_NE(openStick(feature, root), nullptr);
    ASSERT_NO_FATAL_FAILURE(addPlaylistItem(feature, root, QStringLiteral("Legacy list")));
    const auto playlist =
            childIndex(feature, deviceIndex(feature, root), QStringLiteral("Legacy list"));
    ASSERT_TRUE(playlist.isValid());
    const auto state = deleteMenuState(feature, playlist);
    EXPECT_TRUE(state.visible);
    EXPECT_FALSE(state.enabled);
    EXPECT_TRUE(state.toolTip.contains(QStringLiteral("Device Library Plus"))) << qPrintable(state.toolTip);
}

} // namespace mixxx::rekordbox
// End of AI-generated text.
