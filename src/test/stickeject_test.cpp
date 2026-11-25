// AI-generated (Claude Code). Start of AI-generated text.
// Tests for the safe removal of the stick after an export. The tests use a
// fake ejector: no test ejects a real volume.
#include "library/export/stickeject.h"

#include <gtest/gtest.h>

#include <QStringList>

namespace {

using mixxx::ejectExportedStick;
using mixxx::isPathOnStick;
using mixxx::linuxEjectCommands;
using mixxx::StickEjectResult;
using mixxx::StickEjector;

/// Counts the calls and gives the result that the test sets.
class FakeEjector : public StickEjector {
  public:
    StickEjectResult eject(const QString& stickRoot) override {
        calls.append(stickRoot);
        return result;
    }
    QStringList calls;
    StickEjectResult result{true, QString()};
};

TEST(StickEjectTest, PathOnStickWindowsDriveIgnoresCaseAndSeparator) {
    EXPECT_TRUE(isPathOnStick(QStringLiteral("e:/Music/a.mp3"), QStringLiteral("E:/")));
    EXPECT_TRUE(isPathOnStick(QStringLiteral("E:\\Music\\a.mp3"), QStringLiteral("E:\\")));
    EXPECT_TRUE(isPathOnStick(QStringLiteral("E:/"), QStringLiteral("E:/")));
    EXPECT_FALSE(isPathOnStick(QStringLiteral("F:/Music/a.mp3"), QStringLiteral("E:/")));
    EXPECT_FALSE(isPathOnStick(QStringLiteral("C:/Users/a.mp3"), QStringLiteral("E:/")));
}

TEST(StickEjectTest, PathOnStickLinuxMountIsNotAPrefixMatch) {
    const QString root = QStringLiteral("/media/u/STICK");
    EXPECT_TRUE(isPathOnStick(QStringLiteral("/media/u/STICK/a.mp3"), root));
    EXPECT_TRUE(isPathOnStick(QStringLiteral("/media/u/STICK"), root));
    EXPECT_FALSE(isPathOnStick(QStringLiteral("/media/u/STICK2/a.mp3"), root));
    EXPECT_FALSE(isPathOnStick(QStringLiteral("/media/u/stick/a.mp3"), root));
    EXPECT_FALSE(isPathOnStick(QStringLiteral("/home/u/a.mp3"), root));
}

TEST(StickEjectTest, EmptyPathOrRootIsNeverOnStick) {
    EXPECT_FALSE(isPathOnStick(QString(), QStringLiteral("E:/")));
    EXPECT_FALSE(isPathOnStick(QStringLiteral("E:/a.mp3"), QString()));
    EXPECT_FALSE(isPathOnStick(QStringLiteral("E:/a.mp3"), QStringLiteral("  ")));
}

TEST(StickEjectTest, EjectsTheStickOfTheExport) {
    FakeEjector fake;
    const auto result = ejectExportedStick(fake,
            QStringLiteral("E:/"),
            QStringLiteral("E:/"),
            {QStringLiteral("C:/Music/a.mp3")});
    EXPECT_TRUE(result.ok);
    ASSERT_EQ(1, fake.calls.size());
    EXPECT_EQ(QStringLiteral("E:/"), fake.calls.at(0));
}

TEST(StickEjectTest, RefusesAnotherVolume) {
    FakeEjector fake;
    const auto result = ejectExportedStick(
            fake, QStringLiteral("E:/"), QStringLiteral("F:/"), {});
    EXPECT_FALSE(result.ok);
    EXPECT_TRUE(fake.calls.isEmpty());
    EXPECT_FALSE(result.message.isEmpty());
}

TEST(StickEjectTest, RefusesWithoutAStickRoot) {
    FakeEjector fake;
    EXPECT_FALSE(ejectExportedStick(fake, QString(), QString(), {}).ok);
    EXPECT_FALSE(ejectExportedStick(fake, QStringLiteral("E:/"), QString(), {}).ok);
    EXPECT_FALSE(ejectExportedStick(fake, QString(), QStringLiteral("E:/"), {}).ok);
    EXPECT_TRUE(fake.calls.isEmpty());
}

TEST(StickEjectTest, RefusesWhenATrackOfTheStickIsLoaded) {
    FakeEjector fake;
    const auto result = ejectExportedStick(fake,
            QStringLiteral("E:/"),
            QStringLiteral("E:/"),
            {QStringLiteral("C:/Music/a.mp3"), QStringLiteral("e:/Music/b.mp3")});
    EXPECT_FALSE(result.ok);
    EXPECT_TRUE(fake.calls.isEmpty());
    // The message tells the user what to do.
    EXPECT_TRUE(result.message.contains(QStringLiteral("deck")));
}

TEST(StickEjectTest, AFailedEjectTellsTheUserToKeepTheStick) {
    FakeEjector fake;
    fake.result = {false, QStringLiteral("Another program uses the stick.")};
    const auto result = ejectExportedStick(
            fake, QStringLiteral("E:/"), QStringLiteral("E:/"), {});
    EXPECT_FALSE(result.ok);
    ASSERT_EQ(1, fake.calls.size());
    EXPECT_TRUE(result.message.contains(QStringLiteral("Another program uses the stick.")));
    EXPECT_TRUE(result.message.contains(QStringLiteral("Do not remove the stick yet")));
}

TEST(StickEjectTest, LinuxCommandsUnmountThenPowerOff) {
    const auto commands = linuxEjectCommands(QStringLiteral("/dev/sdb1"));
    ASSERT_EQ(2, commands.size());
    EXPECT_EQ(QStringLiteral("udisksctl"), commands.at(0).program);
    EXPECT_EQ(QStringList({QStringLiteral("unmount"), QStringLiteral("-b"), QStringLiteral("/dev/sdb1")}),
            commands.at(0).arguments);
    EXPECT_EQ(QStringLiteral("power-off"), commands.at(1).arguments.value(0));
    EXPECT_EQ(QStringLiteral("/dev/sdb1"), commands.at(1).arguments.value(2));
}

TEST(StickEjectTest, LinuxCommandsRefuseADeviceOutsideDev) {
    EXPECT_TRUE(linuxEjectCommands(QString()).isEmpty());
    EXPECT_TRUE(linuxEjectCommands(QStringLiteral("//server/share")).isEmpty());
    EXPECT_TRUE(linuxEjectCommands(QStringLiteral("-b")).isEmpty());
    EXPECT_TRUE(linuxEjectCommands(QStringLiteral("/dev/../etc/passwd")).isEmpty());
}

} // namespace

// End of AI-generated text.
