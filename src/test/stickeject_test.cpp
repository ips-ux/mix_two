// AI-generated (Claude Code). Start of AI-generated text.
// Tests for the safe removal of the stick after an export. The tests use a
// fake ejector: no test ejects a real volume.
#include "library/export/stickeject.h"

#include <gtest/gtest.h>

#include <QStringList>

namespace {

using mixxx::ejectExportedStick;
using mixxx::ejectVolumeSteps;
using mixxx::isLinuxStickDrive;
using mixxx::isPathOnStick;
using mixxx::VolumeSteps;
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
    QString volumeIdentity(const QString&) override {
        return identity;
    }
    QStringList calls;
    StickEjectResult result{true, QString()};
    QString identity = QStringLiteral("id-1");
};

const QString kId = QStringLiteral("id-1");

/// Records the order of the device calls.
class FakeSteps : public VolumeSteps {
  public:
    bool isStickDevice() override {
        log << "device";
        return isStick;
    }
    void flush() override {
        log << "flush";
    }
    bool lock() override {
        log << "lock";
        return lockOk;
    }
    void pause() override {
        log << "pause";
    }
    bool unlock() override {
        log << "unlock";
        return true;
    }
    bool dismount() override {
        log << "dismount";
        return dismountOk;
    }
    void allowRemoval() override {
        log << "allow";
    }
    bool eject() override {
        log << "eject";
        return ejectOk;
    }
    void close() override {
        log << "close";
    }
    unsigned long lastError() override {
        return 5;
    }
    QStringList log;
    bool isStick = true;
    bool lockOk = true;
    bool dismountOk = true;
    bool ejectOk = true;
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
            kId,
            QStringLiteral("E:/"),
            {QStringLiteral("C:/Music/a.mp3")});
    EXPECT_TRUE(result.ok);
    ASSERT_EQ(1, fake.calls.size());
    EXPECT_EQ(QStringLiteral("E:/"), fake.calls.at(0));
}

TEST(StickEjectTest, RefusesAnotherVolume) {
    FakeEjector fake;
    const auto result = ejectExportedStick(
            fake, QStringLiteral("E:/"), kId, QStringLiteral("F:/"), {});
    EXPECT_FALSE(result.ok);
    EXPECT_TRUE(fake.calls.isEmpty());
    EXPECT_FALSE(result.message.isEmpty());
}

TEST(StickEjectTest, RefusesWithoutAStickRoot) {
    FakeEjector fake;
    EXPECT_FALSE(ejectExportedStick(fake, QString(), kId, QString(), {}).ok);
    EXPECT_FALSE(ejectExportedStick(fake, QStringLiteral("E:/"), kId, QString(), {}).ok);
    EXPECT_FALSE(ejectExportedStick(fake, QString(), kId, QStringLiteral("E:/"), {}).ok);
    EXPECT_TRUE(fake.calls.isEmpty());
}

TEST(StickEjectTest, RefusesWhenATrackOfTheStickIsLoaded) {
    FakeEjector fake;
    const auto result = ejectExportedStick(fake,
            QStringLiteral("E:/"),
            kId,
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
            fake, QStringLiteral("E:/"), kId, QStringLiteral("E:/"), {});
    EXPECT_FALSE(result.ok);
    ASSERT_EQ(1, fake.calls.size());
    EXPECT_TRUE(result.message.contains(QStringLiteral("Another program uses the stick.")));
    EXPECT_TRUE(result.message.contains(QStringLiteral("Do not remove the stick yet")));
}

TEST(StickEjectTest, RefusesASwappedStick) {
    FakeEjector fake;
    fake.identity = QStringLiteral("id-2");
    const auto result = ejectExportedStick(
            fake, QStringLiteral("E:/"), kId, QStringLiteral("E:/"), {});
    EXPECT_FALSE(result.ok);
    EXPECT_TRUE(fake.calls.isEmpty());
}

TEST(StickEjectTest, RefusesWhenNoIdentityCanBeRead) {
    FakeEjector fake;
    // None at the export.
    EXPECT_FALSE(ejectExportedStick(
            fake, QStringLiteral("E:/"), QString(), QStringLiteral("E:/"), {})
                         .ok);
    // None now.
    fake.identity.clear();
    EXPECT_FALSE(ejectExportedStick(
            fake, QStringLiteral("E:/"), kId, QStringLiteral("E:/"), {})
                         .ok);
    EXPECT_TRUE(fake.calls.isEmpty());
}

TEST(StickEjectTest, OnlyAToZIsADriveLetter) {
    // "e acute" is a letter, but it is not a drive letter: the compare keeps
    // the letter case, so a path with the other case is not on the stick.
    const QString lower = QString(QChar(0x00e9)) + QStringLiteral(":/a.mp3");
    const QString upperRoot = QString(QChar(0x00c9)) + QStringLiteral(":/");
    EXPECT_FALSE(isPathOnStick(lower, upperRoot));
    // A to Z still ignores the letter case.
    EXPECT_TRUE(isPathOnStick(QStringLiteral("e:/a.mp3"), QStringLiteral("E:/")));
}

TEST(StickEjectTest, LinuxInternalDiskUnderMediaIsNotAStick) {
    EXPECT_FALSE(isLinuxStickDrive(false,
            QStringLiteral("/sys/devices/pci0000:00/0000:00:1f.2/ata1/host0/target0:0:0/0:0:0:0/block/sda")));
    EXPECT_FALSE(isLinuxStickDrive(false,
            QStringLiteral("/sys/devices/pci0000:00/0000:00:1d.0/nvme/nvme0/nvme0n1")));
    EXPECT_FALSE(isLinuxStickDrive(false, QString()));
}

TEST(StickEjectTest, LinuxUsbAndCardAreSticks) {
    EXPECT_TRUE(isLinuxStickDrive(false,
            QStringLiteral("/sys/devices/pci0000:00/0000:00:14.0/usb2/2-1/2-1:1.0/host6/target6:0:0/6:0:0:0/block/sdb")));
    EXPECT_TRUE(isLinuxStickDrive(false,
            QStringLiteral("/sys/devices/pci0000:00/0000:00:1a.0/mmc_host/mmc0/mmc0:aaaa/block/mmcblk0")));
    EXPECT_TRUE(isLinuxStickDrive(true,
            QStringLiteral("/sys/devices/pci0000:00/0000:00:1f.2/ata1/host0/block/sdc")));
    // A name that only starts with "usb" is not a USB port.
    EXPECT_FALSE(isLinuxStickDrive(false, QStringLiteral("/sys/devices/usbfoo/block/sda")));
}

TEST(StickEjectTest, StepsRunInTheDocumentedOrder) {
    FakeSteps steps;
    const auto result = ejectVolumeSteps(steps);
    EXPECT_TRUE(result.ok);
    EXPECT_EQ(QStringList({"device", "flush", "lock", "dismount", "allow", "eject", "close"}),
            steps.log);
}

TEST(StickEjectTest, NoDismountAfterAFailedLock) {
    FakeSteps steps;
    steps.lockOk = false;
    const auto result = ejectVolumeSteps(steps);
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(4, steps.log.count("lock"));
    EXPECT_EQ(3, steps.log.count("pause"));
    EXPECT_EQ(0, steps.log.count("dismount"));
    EXPECT_EQ(0, steps.log.count("eject"));
    EXPECT_EQ(1, steps.log.count("close"));
    EXPECT_EQ(QStringLiteral("close"), steps.log.last());
}

TEST(StickEjectTest, EveryPathClosesTheHandleOnce) {
    {
        FakeSteps steps;
        steps.isStick = false;
        EXPECT_FALSE(ejectVolumeSteps(steps).ok);
        EXPECT_EQ(1, steps.log.count("close"));
        EXPECT_EQ(0, steps.log.count("lock"));
    }
    {
        FakeSteps steps;
        steps.dismountOk = false;
        EXPECT_FALSE(ejectVolumeSteps(steps).ok);
        EXPECT_EQ(1, steps.log.count("close"));
        EXPECT_EQ(1, steps.log.count("unlock"));
        EXPECT_EQ(0, steps.log.count("eject"));
    }
    {
        FakeSteps steps;
        steps.ejectOk = false;
        EXPECT_FALSE(ejectVolumeSteps(steps).ok);
        EXPECT_EQ(1, steps.log.count("close"));
        EXPECT_EQ(1, steps.log.count("unlock"));
    }
    {
        FakeSteps steps;
        EXPECT_TRUE(ejectVolumeSteps(steps).ok);
        EXPECT_EQ(1, steps.log.count("close"));
    }
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
