// AI-generated (Claude Code). Start of AI-generated text.
#include <gtest/gtest.h>

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QtEndian>
#include <tuple>

#include "coreservices.h"
#include "library/library.h"
#include "library/rekordbox/rekordboxfeature.h"
#include "library/rekordbox/stickwatcher.h"
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
#endif

} // namespace mixxx::rekordbox
// End of AI-generated text.
