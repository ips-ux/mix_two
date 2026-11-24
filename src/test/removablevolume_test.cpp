// AI-generated (Claude Code). Start of AI-generated text.
// Tests for the removable volume check and the safe move to the trash.
#include "util/removablevolume.h"

#include <gtest/gtest.h>

#include <QFile>
#include <QTemporaryDir>

namespace {

using mixxx::isRemovableVolume;
using mixxx::isTrashSafeVolume;
using mixxx::moveToTrashChecked;
using mixxx::TrashResult;
using mixxx::VolumeInfo;

const QStringList kParents = {QStringLiteral("/media"), QStringLiteral("/run/media")};

QList<VolumeInfo> fakeVolumes() {
    return {{QStringLiteral("C:/"), false},
            {QStringLiteral("E:/"), true},
            {QStringLiteral("/"), false}};
}

TEST(RemovableVolumeTest, MediaMountsAreRemovable) {
    EXPECT_TRUE(isRemovableVolume(QStringLiteral("/media/u/X/music/a.mp3"), {}, kParents));
    EXPECT_TRUE(isRemovableVolume(QStringLiteral("/run/media/u/Y/a.mp3"), {}, kParents));
    EXPECT_TRUE(isRemovableVolume(QStringLiteral("/media/u/X"), {}, kParents));
}

TEST(RemovableVolumeTest, OtherLinuxPathsAreNot) {
    EXPECT_FALSE(isRemovableVolume(QStringLiteral("/home/u/music/a.mp3"),
            fakeVolumes(),
            kParents));
    // A folder that only starts with the same letters is not below /media.
    EXPECT_FALSE(isRemovableVolume(QStringLiteral("/media2/a.mp3"), {}, kParents));
    EXPECT_FALSE(isRemovableVolume(QStringLiteral("/mediafoo/u/a.mp3"), fakeVolumes(), kParents));
}

TEST(RemovableVolumeTest, DriveLettersFollowTheDriveType) {
    EXPECT_FALSE(isRemovableVolume(QStringLiteral("C:/Music/a.mp3"), fakeVolumes(), {}));
    EXPECT_TRUE(isRemovableVolume(QStringLiteral("E:/Music/a.mp3"), fakeVolumes(), {}));
    EXPECT_TRUE(isRemovableVolume(QStringLiteral("E:\\Music\\a.mp3"), fakeVolumes(), {}));
    EXPECT_TRUE(isRemovableVolume(QStringLiteral("e:/Music/a.mp3"), fakeVolumes(), {}));
    EXPECT_TRUE(isRemovableVolume(QStringLiteral("E:/"), fakeVolumes(), {}));
}

TEST(RemovableVolumeTest, LongestRootDecides) {
    // A fixed disk mounted below a removable one.
    const QList<VolumeInfo> volumes = {{QStringLiteral("/mnt/usb"), true},
            {QStringLiteral("/mnt/usb/disk"), false},
            {QStringLiteral("/"), false}};
    EXPECT_TRUE(isRemovableVolume(QStringLiteral("/mnt/usb/a.mp3"), volumes, {}));
    EXPECT_FALSE(isRemovableVolume(QStringLiteral("/mnt/usb/disk/a.mp3"), volumes, {}));
    EXPECT_FALSE(isRemovableVolume(QStringLiteral("/mnt/usbx/a.mp3"), volumes, {}));
}

TEST(RemovableVolumeTest, EmptyPathIsNotRemovable) {
    EXPECT_FALSE(isRemovableVolume(QString(), fakeVolumes(), kParents));
}

TEST(RemovableVolumeTest, RealCheckOfATempFolder) {
    // The temporary folder is on the system drive in a normal set-up.
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("a.txt"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.close();
    EXPECT_FALSE(isRemovableVolume(path));
    EXPECT_TRUE(isTrashSafeVolume(path));
}

QList<VolumeInfo> volumesWithShare() {
    return {{QStringLiteral("C:/"), false, false},
            {QStringLiteral("E:/"), true, false},
            {QStringLiteral("Z:/"), false, true}};
}

TEST(TrashSafeVolumeTest, FixedDriveIsSafe) {
    EXPECT_TRUE(isTrashSafeVolume(QStringLiteral("C:/Music/a.mp3"), volumesWithShare(), {}));
    EXPECT_TRUE(isTrashSafeVolume(QStringLiteral("/home/u/a.mp3"), volumesWithShare(), kParents));
}

TEST(TrashSafeVolumeTest, RemovableAndNetworkDrivesAreNot) {
    EXPECT_FALSE(isTrashSafeVolume(QStringLiteral("E:/Music/a.mp3"), volumesWithShare(), {}));
    EXPECT_FALSE(isTrashSafeVolume(QStringLiteral("Z:/Music/a.mp3"), volumesWithShare(), {}));
    EXPECT_FALSE(isTrashSafeVolume(QStringLiteral("/media/u/X/a.mp3"), {}, kParents));
    // A network drive is not a removable drive.
    EXPECT_FALSE(isRemovableVolume(QStringLiteral("Z:/Music/a.mp3"), volumesWithShare(), {}));
}

TEST(TrashSafeVolumeTest, NetworkPathsAreNot) {
    EXPECT_FALSE(isTrashSafeVolume(QStringLiteral("//server/share/a.mp3"), volumesWithShare(), {}));
    EXPECT_FALSE(isTrashSafeVolume(
            QStringLiteral("\\\\server\\share\\a.mp3"), volumesWithShare(), {}));
}

TEST(TrashSafeVolumeTest, LinkIsJudgedByItsTarget) {
    // The resolver stands for a junction or a substitute drive on a fixed
    // disk that points to a stick, or to a share.
    const auto toStick = [](const QString&) { return QStringLiteral("E:/a.mp3"); };
    const auto toShare = [](const QString&) { return QStringLiteral("//server/share/a.mp3"); };
    const auto toFixed = [](const QString&) { return QStringLiteral("C:/other/a.mp3"); };
    const auto noTarget = [](const QString&) { return QString(); };
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("link.mp3"));
    EXPECT_FALSE(isTrashSafeVolume(path, toStick, volumesWithShare(), {}));
    EXPECT_FALSE(isTrashSafeVolume(path, toShare, volumesWithShare(), {}));
    EXPECT_TRUE(isTrashSafeVolume(path, toFixed, volumesWithShare(), {}));
    EXPECT_TRUE(isTrashSafeVolume(path, noTarget, volumesWithShare(), {}));
}

TEST(MoveToTrashCheckedTest, RemovableFileStays) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("a.txt"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("x");
    file.close();

    int trashCalls = 0;
    const auto result = moveToTrashChecked(
            path,
            [](const QString&) { return false; },
            [&](const QString&) {
                ++trashCalls;
                return true;
            });
    EXPECT_EQ(TrashResult::RefusedUnsafe, result);
    EXPECT_EQ(0, trashCalls);
    EXPECT_TRUE(QFile::exists(path));
}

TEST(MoveToTrashCheckedTest, UnsafeFileOfTheRealCheckStaysWhenTheTargetIsUnsafe) {
    // The real overload refuses a network path and does not touch it.
    EXPECT_EQ(TrashResult::RefusedUnsafe,
            moveToTrashChecked(QStringLiteral("//server/share/none.mp3")));
}

TEST(MoveToTrashCheckedTest, NormalFileGoesToTheTrash) {
    QString trashed;
    const auto result = moveToTrashChecked(
            QStringLiteral("/home/u/a.mp3"),
            [](const QString&) { return true; },
            [&](const QString& path) {
                trashed = path;
                return true;
            });
    EXPECT_EQ(TrashResult::Moved, result);
    EXPECT_EQ(QStringLiteral("/home/u/a.mp3"), trashed);
}

TEST(MoveToTrashCheckedTest, FailureOfTheSystemIsReported) {
    const auto result = moveToTrashChecked(
            QStringLiteral("/home/u/a.mp3"),
            [](const QString&) { return true; },
            [](const QString&) { return false; });
    EXPECT_EQ(TrashResult::Failed, result);
}

} // namespace
// End of AI-generated text.
