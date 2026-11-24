// AI-generated (Claude Code). Start of AI-generated text.
// Tests for the removable volume check and the safe move to the trash.
#include "util/removablevolume.h"

#include <gtest/gtest.h>

#include <QFile>
#include <QTemporaryDir>

namespace {

using mixxx::isRemovableVolume;
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
    // The temporary folder is on the system disk in a normal set-up. The
    // test only proves that the real check runs for a path that exists.
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    (void)isRemovableVolume(dir.path());
    SUCCEED();
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
            [](const QString&) { return true; },
            [&](const QString&) {
                ++trashCalls;
                return true;
            });
    EXPECT_EQ(TrashResult::RefusedRemovable, result);
    EXPECT_EQ(0, trashCalls);
    EXPECT_TRUE(QFile::exists(path));
}

TEST(MoveToTrashCheckedTest, NormalFileGoesToTheTrash) {
    QString trashed;
    const auto result = moveToTrashChecked(
            QStringLiteral("/home/u/a.mp3"),
            [](const QString&) { return false; },
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
            [](const QString&) { return false; },
            [](const QString&) { return false; });
    EXPECT_EQ(TrashResult::Failed, result);
}

} // namespace
// End of AI-generated text.
