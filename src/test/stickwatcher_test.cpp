// AI-generated (Claude Code). Start of AI-generated text.
#include <gtest/gtest.h>

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
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
    ASSERT_TRUE(root.mkpath("PIONEER/rekordbox"));
    QFile pdb(root.filePath("PIONEER/rekordbox/export.pdb"));
    ASSERT_TRUE(pdb.open(QIODevice::WriteOnly));
    // A PDB header with zero tables produces an empty device playlist.
    ASSERT_EQ(pdb.write(QByteArray(28, '\0')), 28);
    pdb.close();

    auto* feature = m_services->getLibrary()->findChild<RekordboxFeature*>();
    ASSERT_NE(feature, nullptr);
    auto* watcher = feature->findChild<StickWatcher*>();
    ASSERT_NE(watcher, nullptr);
    QObject context;
    RekordboxPlaylistModel* shown = nullptr;
    QString view;
    QObject::connect(feature, &LibraryFeature::showTrackModel, &context,
            [&](QAbstractItemModel* model) { shown = qobject_cast<RekordboxPlaylistModel*>(model); });
    QObject::connect(feature, &LibraryFeature::switchToView, &context,
            [&](const QString& name) { view = name; });

    auto* sidebar = feature->sidebarModel();
    const auto deviceIndex = [&] {
        for (int row = 0; row < sidebar->rowCount(); ++row) {
            const auto index = sidebar->index(row, 0);
            if (index.data().toString() == root.dirName()) {
                return index;
            }
        }
        return QModelIndex{};
    };
    watcher->volumeArrived(stick.path());
    ASSERT_TRUE(waitUntil([&] { return deviceIndex().isValid(); }));
    feature->activateChild(deviceIndex());
    ASSERT_TRUE(waitUntil([&] { return shown != nullptr; }));
    const QString playlistKey = shown->modelKey(false);

    watcher->volumeDeparted(stick.path());
    EXPECT_FALSE(deviceIndex().isValid());
    EXPECT_EQ(view, QStringLiteral("REKORDBOXHOME"));
    EXPECT_NE(shown->modelKey(false), playlistKey);
}

} // namespace
} // namespace mixxx::rekordbox
// End of AI-generated text.
