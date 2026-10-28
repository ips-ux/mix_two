#include <gtest/gtest.h>

#include <QFileInfo>
#include <QTemporaryDir>

#include "controllers/controllermappinginfo.h"
#include "controllers/legacycontrollermappingfilehandler.h"
#include "controllers/midi/midicontroller.h"
#include "controllers/midi/midiutils.h"
#include "test/mixxxtest.h"

namespace {

ProductInfo midiProduct(const QString& deviceName, const QString& deviceNameRegex = QString()) {
    ProductInfo product;
    product.protocol = QStringLiteral("midi");
    product.device_name = deviceName;
    product.device_name_regex = deviceNameRegex;
    return product;
}

} // namespace

class MidiProductMatchTest : public MixxxTest {};

TEST_F(MidiProductMatchTest, CanonicalDeviceName) {
    EXPECT_EQ(QStringLiteral("DDJ-800"), MidiUtils::canonicalDeviceName("DDJ-800"));
    EXPECT_EQ(QStringLiteral("DDJ-800"), MidiUtils::canonicalDeviceName("  DDJ-800 "));
    // Windows numbers additional instances of the same device.
    EXPECT_EQ(QStringLiteral("DDJ-800"), MidiUtils::canonicalDeviceName("2- DDJ-800"));
    // ALSA
    EXPECT_EQ(QStringLiteral("DDJ-800"), MidiUtils::canonicalDeviceName("DDJ-800 MIDI 1"));
    EXPECT_EQ(QStringLiteral("DDJ-800"),
            MidiUtils::canonicalDeviceName("DDJ-800:DDJ-800 MIDI 1 20:0"));
    EXPECT_EQ(QStringLiteral("DDJ-800"), MidiUtils::canonicalDeviceName("DDJ-800 20:0"));
    EXPECT_EQ(QStringLiteral("DDJ-800"), MidiUtils::canonicalDeviceName("From: DDJ-800"));
    // Numbers that belong to the device name are kept.
    EXPECT_EQ(QStringLiteral("Hercules RMX 2"),
            MidiUtils::canonicalDeviceName("Hercules RMX 2"));
    EXPECT_EQ(QStringLiteral("DDJ-1000"), MidiUtils::canonicalDeviceName("DDJ-1000"));
}

TEST_F(MidiProductMatchTest, MatchByDeviceName) {
    const ProductInfo product = midiProduct("DDJ-800");
    EXPECT_TRUE(MidiController::matchProductInfo("DDJ-800", product));
    EXPECT_TRUE(MidiController::matchProductInfo("ddj-800", product));
    EXPECT_TRUE(MidiController::matchProductInfo("2- DDJ-800", product));
    EXPECT_TRUE(MidiController::matchProductInfo("DDJ-800 MIDI 1", product));
    EXPECT_FALSE(MidiController::matchProductInfo("DDJ-8000", product));
    EXPECT_FALSE(MidiController::matchProductInfo("DDJ-1000", product));
    EXPECT_FALSE(MidiController::matchProductInfo("", product));
}

TEST_F(MidiProductMatchTest, MatchByRegex) {
    const ProductInfo product = midiProduct(QString(), "^DDJ-(400|FLX4)\\b");
    EXPECT_TRUE(MidiController::matchProductInfo("DDJ-400", product));
    EXPECT_TRUE(MidiController::matchProductInfo("DDJ-FLX4 MIDI 1", product));
    EXPECT_FALSE(MidiController::matchProductInfo("DDJ-800", product));
}

TEST_F(MidiProductMatchTest, OtherProtocolsNeverMatch) {
    ProductInfo product = midiProduct("DDJ-800");
    product.protocol = QStringLiteral("hid");
    EXPECT_FALSE(MidiController::matchProductInfo("DDJ-800", product));
}

TEST_F(MidiProductMatchTest, InvalidRegexNeverMatches) {
    const ProductInfo product = midiProduct(QString(), "DDJ-(800");
    EXPECT_FALSE(MidiController::matchProductInfo("DDJ-800", product));
}

TEST_F(MidiProductMatchTest, Ddj800MappingDeclaresDevice) {
    const QFileInfo mappingFile(getTestDir().filePath(
            QStringLiteral("../../res/controllers/Pioneer-DDJ-800.midi.xml")));
    ASSERT_TRUE(mappingFile.exists());
    const MappingInfo mapping(mappingFile);
    ASSERT_TRUE(mapping.isValid());
    ASSERT_EQ(1, mapping.getProducts().size());
    const ProductInfo& product = mapping.getProducts().first();
    EXPECT_EQ(QStringLiteral("midi"), product.protocol);
    EXPECT_EQ(QStringLiteral("DDJ-800"), product.device_name);
    EXPECT_TRUE(MidiController::matchProductInfo("DDJ-800", product));
}

TEST_F(MidiProductMatchTest, Ddj800MappingKeepsDeviceWhenSaved) {
    const QFileInfo mappingFile(getTestDir().filePath(
            QStringLiteral("../../res/controllers/Pioneer-DDJ-800.midi.xml")));
    const auto pMapping = LegacyControllerMappingFileHandler::loadMapping(
            mappingFile, mappingFile.absolutePath());
    ASSERT_TRUE(pMapping);
    ASSERT_EQ(1, pMapping->productMatch().size());
    EXPECT_EQ(QStringLiteral("DDJ-800"), pMapping->productMatch().begin()->device_name);
    // Loading must not mark the mapping as modified.
    EXPECT_FALSE(pMapping->isDirty());

    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString savedPath = tempDir.filePath(QStringLiteral("saved.midi.xml"));
    ASSERT_TRUE(pMapping->saveMapping(savedPath));
    const MappingInfo savedInfo{QFileInfo(savedPath)};
    ASSERT_EQ(1, savedInfo.getProducts().size());
    EXPECT_EQ(QStringLiteral("DDJ-800"), savedInfo.getProducts().first().device_name);
}
