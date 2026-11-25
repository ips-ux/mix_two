// AI-generated (Claude Code). Start of AI-generated text.
#include "track/bitdepth.h"

#include <gtest/gtest.h>

#include <QTemporaryDir>

#include "test/bitdepthtestfiles.h"
#include "test/mixxxtest.h"

namespace {

class BitDepthTest : public MixxxTest {
  protected:
    QString file(const QString& name, const QByteArray& bytes) {
        return bitdepthtest::writeFile(QDir(m_dir.path()), name, bytes);
    }

    QTemporaryDir m_dir;
};

} // namespace

TEST_F(BitDepthTest, WavAndAiffGiveTheBitsPerSample) {
    EXPECT_EQ(24, mixxx::bitDepthFromFile(file("a24.wav", bitdepthtest::wavBytes(24))));
    EXPECT_EQ(16, mixxx::bitDepthFromFile(file("a16.wav", bitdepthtest::wavBytes(16))));
    EXPECT_EQ(24, mixxx::bitDepthFromFile(file("a24.aiff", bitdepthtest::aiffBytes(24))));
    EXPECT_EQ(16, mixxx::bitDepthFromFile(file("a16.aif", bitdepthtest::aiffBytes(16))));
    // The suffix decides, and it does not depend on the letter case.
    EXPECT_EQ(24, mixxx::bitDepthFromFile(file("b24.WAV", bitdepthtest::wavBytes(24))));
}

TEST_F(BitDepthTest, FlacTestFileGivesSixteen) {
    EXPECT_EQ(16, mixxx::bitDepthFromFile(getTestFile(u".flac")));
    EXPECT_EQ(16, mixxx::bitDepthFromFile(getTestFile(u".wav")));
    EXPECT_EQ(16, mixxx::bitDepthFromFile(getTestFile(u".aiff")));
}

TEST_F(BitDepthTest, LossyFormatsAndFailuresGiveUnknown) {
    // Lossy formats have no bit depth.
    EXPECT_EQ(0, mixxx::bitDepthFromFile(getTestFile(u"-vbr.mp3")));
    EXPECT_EQ(0, mixxx::bitDepthFromFile(getTestFile(u".ogg")));
    EXPECT_EQ(0, mixxx::bitDepthFromFile(getTestFile(u".opus")));
    // A file that cannot be read, a file without audio, a file without suffix.
    EXPECT_EQ(0, mixxx::bitDepthFromFile(m_dir.filePath("missing.wav")));
    EXPECT_EQ(0, mixxx::bitDepthFromFile(file("text.wav", QByteArray("no"))));
    EXPECT_EQ(0, mixxx::bitDepthFromFile(file("noending", bitdepthtest::wavBytes(24))));
    EXPECT_EQ(0, mixxx::bitDepthFromFile(QString()));
}

TEST_F(BitDepthTest, HeaderReaderSelectsByKind) {
    const QString wav = file("k.wav", bitdepthtest::wavBytes(24));
    EXPECT_EQ(24, mixxx::bitDepthFromHeader(wav, mixxx::BitDepthFileKind::Wav));
    // A WAV file is not a FLAC file.
    EXPECT_EQ(0, mixxx::bitDepthFromHeader(wav, mixxx::BitDepthFileKind::Flac));
}

TEST(BitDepthDisplayTest, UnknownShowsNothing) {
    EXPECT_FALSE(mixxx::bitDepthDisplayValue(QVariant()).isValid());
    EXPECT_FALSE(mixxx::bitDepthDisplayValue(QVariant(0)).isValid());
    EXPECT_FALSE(mixxx::bitDepthDisplayValue(QVariant(-8)).isValid());
    EXPECT_FALSE(mixxx::bitDepthDisplayValue(QVariant(QStringLiteral("x"))).isValid());
    EXPECT_EQ(16, mixxx::bitDepthDisplayValue(QVariant(16)).toInt());
    EXPECT_EQ(24, mixxx::bitDepthDisplayValue(QVariant(24)).toInt());
    EXPECT_EQ(24, mixxx::bitDepthDisplayValue(QVariant(QStringLiteral("24"))).toInt());
}
// End of AI-generated text.
