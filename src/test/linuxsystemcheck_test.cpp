// AI-generated (Claude Code). Start of AI-generated text.
// Tests for the Linux start checks (kernel version, sound card list).
#include "util/linuxsystemcheck.h"

#include <gtest/gtest.h>

namespace {

using mixxx::kernelOlderThan;
using mixxx::rtNoticeNeeded;
using mixxx::shouldShowNotice;
using mixxx::soundCardsListDdj800;

TEST(LinuxSystemCheckTest, OldKernelIsOlder) {
    EXPECT_TRUE(kernelOlderThan(QStringLiteral("6.3.9-generic"), 6, 4));
    EXPECT_TRUE(kernelOlderThan(QStringLiteral("5.15.0-91-generic"), 6, 4));
    EXPECT_TRUE(kernelOlderThan(QStringLiteral("6.3"), 6, 4));
    EXPECT_TRUE(kernelOlderThan(QStringLiteral("4.19.0\n"), 6, 4));
}

TEST(LinuxSystemCheckTest, NewKernelIsNotOlder) {
    EXPECT_FALSE(kernelOlderThan(QStringLiteral("6.4.0"), 6, 4));
    EXPECT_FALSE(kernelOlderThan(QStringLiteral("6.4"), 6, 4));
    EXPECT_FALSE(kernelOlderThan(QStringLiteral("7.0.1-mint"), 6, 4));
    EXPECT_FALSE(kernelOlderThan(QStringLiteral("6.10.2-arch1-1"), 6, 4));
    EXPECT_FALSE(kernelOlderThan(QStringLiteral("6.8.0-45-generic"), 6, 4));
}

TEST(LinuxSystemCheckTest, UnreadableReleaseIsNotOlder) {
    EXPECT_FALSE(kernelOlderThan(QString(), 6, 4));
    EXPECT_FALSE(kernelOlderThan(QStringLiteral("generic"), 6, 4));
    EXPECT_FALSE(kernelOlderThan(QStringLiteral("6"), 6, 4));
    EXPECT_FALSE(kernelOlderThan(QStringLiteral("6.x.1"), 6, 4));
}

TEST(LinuxSystemCheckTest, SoundCardListWithDdj800) {
    const QString cards = QStringLiteral(
            " 0 [PCH            ]: HDA-Intel - HDA Intel PCH\n"
            "                      HDA Intel PCH at 0xf7e10000 irq 130\n"
            " 1 [DDJ800         ]: USB-Audio - DDJ-800\n"
            "                      Pioneer DJ Corporation DDJ-800 at usb-0000:00:14.0-2\n");
    EXPECT_TRUE(soundCardsListDdj800(cards));
    EXPECT_TRUE(soundCardsListDdj800(QStringLiteral(" 2 [x]: USB-Audio - ddj-800\n")));
}

TEST(LinuxSystemCheckTest, SoundCardListWithoutDdj800) {
    EXPECT_FALSE(soundCardsListDdj800(QString()));
    EXPECT_FALSE(soundCardsListDdj800(
            QStringLiteral(" 0 [PCH]: HDA-Intel - HDA Intel PCH\n")));
    EXPECT_FALSE(soundCardsListDdj800(
            QStringLiteral(" 1 [DDJ1000]: USB-Audio - DDJ-1000\n")));
    EXPECT_FALSE(soundCardsListDdj800(
            QStringLiteral(" 1 [DDJ8000]: USB-Audio - DDJ-8000\n")));
}

TEST(LinuxSystemCheckTest, MidiPortAloneAllowsNotice) {
    const QStringList ports{QStringLiteral("Port   0 : \"DDJ-800 MIDI 1\" (RWeX)")};
    EXPECT_TRUE(shouldShowNotice(mixxx::isDdj800Connected(ports, QString()), false));
}

TEST(LinuxSystemCheckTest, SoundCardAloneAllowsNotice) {
    const QString cards = QStringLiteral(" 1 [DDJ800]: USB-Audio - DDJ-800\n");
    EXPECT_TRUE(shouldShowNotice(mixxx::isDdj800Connected({}, cards), false));
}

TEST(LinuxSystemCheckTest, Ddj800XDoesNotAllowNotice) {
    const QStringList ports{QStringLiteral("Port   0 : \"DDJ-800X MIDI 1\" (RWeX)")};
    const QString cards = QStringLiteral(" 1 [DDJ800X]: USB-Audio - DDJ-800X\n");
    EXPECT_FALSE(shouldShowNotice(mixxx::isDdj800Connected(ports, QString()), false));
    EXPECT_FALSE(shouldShowNotice(mixxx::isDdj800Connected({}, cards), false));
}

TEST(LinuxSystemCheckTest, Ddj8000DoesNotAllowNotice) {
    const QStringList ports{QStringLiteral("Port   0 : \"DDJ-8000 MIDI 1\" (RWeX)")};
    const QString cards = QStringLiteral(" 1 [DDJ8000]: USB-Audio - DDJ-8000\n");
    EXPECT_FALSE(shouldShowNotice(mixxx::isDdj800Connected(ports, QString()), false));
    EXPECT_FALSE(shouldShowNotice(mixxx::isDdj800Connected({}, cards), false));
}

TEST(LinuxSystemCheckTest, NoticeShowsOnceOnly) {
    EXPECT_TRUE(shouldShowNotice(true, false));
    EXPECT_FALSE(shouldShowNotice(true, true));
    EXPECT_FALSE(shouldShowNotice(false, false));
    EXPECT_FALSE(shouldShowNotice(false, true));
}

TEST(LinuxSystemCheckTest, RtNoticeNeededWhenLimitIsLow) {
    EXPECT_TRUE(rtNoticeNeeded(0, 0));
    EXPECT_TRUE(rtNoticeNeeded(0, 95));
    EXPECT_TRUE(rtNoticeNeeded(81, 81));
    EXPECT_TRUE(rtNoticeNeeded(81, 99));
}

TEST(LinuxSystemCheckTest, RtNoticeNotNeededWhenLimitIsEnough) {
    EXPECT_FALSE(rtNoticeNeeded(82, 82));
    EXPECT_FALSE(rtNoticeNeeded(95, 99));
    // RLimit::getCurRtPrio() gives 100 when the limit cannot be read.
    EXPECT_FALSE(rtNoticeNeeded(100, 0));
}

} // namespace

// End of AI-generated text.
