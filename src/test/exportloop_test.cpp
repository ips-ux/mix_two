// AI-generated (Claude Code). Start of AI-generated text.
// Tests for "Export Loop As WAV": the length, the header, the ends, the
// errors and the safety of the files.
#include "library/exportloop.h"

#include <gtest/gtest.h>
#include <sndfile.h>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <cmath>
#include <vector>

#include "test/mixxxtest.h"
#include "test/soundsourceproviderregistration.h"

namespace {

constexpr double kPi = 3.14159265358979323846;

using mixxx::library::ExportLoopStatus;

struct WavData {
    SF_INFO info = {};
    std::vector<short> samples; // interleaved
    bool ok = false;
};

WavData readWav(const QString& path) {
    WavData data;
    SNDFILE* pFile = sf_open(QFile::encodeName(path).constData(), SFM_READ, &data.info);
    if (!pFile) {
        return data;
    }
    data.samples.resize(static_cast<size_t>(data.info.frames * data.info.channels));
    const sf_count_t frames = sf_readf_short(pFile, data.samples.data(), data.info.frames);
    sf_close(pFile);
    data.ok = frames == data.info.frames;
    return data;
}

QByteArray fileHash(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha1);
}

class ExportLoopTest : public MixxxTest, SoundSourceProviderRegistration {
  protected:
    /// A stereo 16 bit WAV with a sine of `frequency` (left) and a ramp
    /// pattern (right), so each frame is different.
    QString makeSource(const QString& name, int rate, int frames, double frequency = 441.0) {
        const QString path = m_dir.filePath(name);
        SF_INFO info = {};
        info.samplerate = rate;
        info.channels = 2;
        info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
        SNDFILE* pFile = sf_open(QFile::encodeName(path).constData(), SFM_WRITE, &info);
        EXPECT_TRUE(pFile != nullptr);
        m_source.assign(static_cast<size_t>(frames) * 2, 0);
        for (int i = 0; i < frames; ++i) {
            m_source[i * 2] = static_cast<short>(
                    std::lround(0.5 * 32767.0 * std::sin(2 * kPi * frequency * i / rate)));
            m_source[i * 2 + 1] = static_cast<short>((i % 20000) - 10000);
        }
        sf_writef_short(pFile, m_source.data(), frames);
        sf_close(pFile);
        return path;
    }

    QStringList filesInDir() {
        return QDir(m_dir.path()).entryList(QDir::Files | QDir::Hidden, QDir::Name);
    }

    QTemporaryDir m_dir;
    std::vector<short> m_source;
};

TEST_F(ExportLoopTest, FrameCount) {
    EXPECT_EQ(48000, mixxx::library::exportLoopFrameCount(44100, 44100));
    EXPECT_EQ(48000, mixxx::library::exportLoopFrameCount(48000, 48000));
    EXPECT_EQ(96000, mixxx::library::exportLoopFrameCount(88200, 44100));
    EXPECT_EQ(24000, mixxx::library::exportLoopFrameCount(48000, 96000));
    EXPECT_EQ(0, mixxx::library::exportLoopFrameCount(0, 44100));
    EXPECT_EQ(0, mixxx::library::exportLoopFrameCount(100, 0));
}

TEST_F(ExportLoopTest, OneSecondLoopOfA44100FileGives48000Frames16BitStereo) {
    ASSERT_TRUE(m_dir.isValid());
    const QString source = makeSource("sine44.wav", 44100, 3 * 44100);
    const QString target = m_dir.filePath("loop.wav");
    const auto result = mixxx::library::exportLoopAsWav(source, 44100, 2 * 44100, target);
    ASSERT_EQ(ExportLoopStatus::Exported, result.status);
    EXPECT_EQ(48000, result.framesWritten);

    const WavData wav = readWav(target);
    ASSERT_TRUE(wav.ok);
    EXPECT_EQ(48000, wav.info.samplerate);
    EXPECT_EQ(2, wav.info.channels);
    EXPECT_EQ(48000, wav.info.frames);
    EXPECT_EQ(SF_FORMAT_WAV, wav.info.format & SF_FORMAT_TYPEMASK);
    EXPECT_EQ(SF_FORMAT_PCM_16, wav.info.format & SF_FORMAT_SUBMASK);
    // No temporary file stays.
    EXPECT_EQ((QStringList{"loop.wav", "sine44.wav"}), filesInDir());
}

TEST_F(ExportLoopTest, ResampledSineKeepsPitchAndSpeed) {
    ASSERT_TRUE(m_dir.isValid());
    const QString source = makeSource("sine44b.wav", 44100, 3 * 44100, 1000.0);
    const QString target = m_dir.filePath("loop.wav");
    const SINT start = 44100;
    const auto result = mixxx::library::exportLoopAsWav(source, start, start + 44100, target);
    ASSERT_EQ(ExportLoopStatus::Exported, result.status);
    const WavData wav = readWav(target);
    ASSERT_TRUE(wav.ok);
    ASSERT_EQ(48000, wav.info.frames);
    // The output is the 1 kHz sine of the source from the loop in point on,
    // with the same phase: out[k] = sine at source frame start + k * 44100/48000.
    // Skip the first and last frames, where the filter meets the loop ends.
    double sum = 0;
    int count = 0;
    for (int k = 64; k < 48000 - 64; ++k) {
        const double ideal = 0.5 * 32767.0 *
                std::sin(2 * kPi * 1000.0 * (start + k * 44100.0 / 48000.0) / 44100.0);
        const double diff = wav.samples[k * 2] - ideal;
        sum += diff * diff;
        ++count;
    }
    EXPECT_LT(std::sqrt(sum / count), 40.0); // about -58 dB below full scale
}

TEST_F(ExportLoopTest, At48kHzTheLoopIsTheSourceFramesExactly) {
    ASSERT_TRUE(m_dir.isValid());
    const QString source = makeSource("src48.wav", 48000, 20000);
    const QString target = m_dir.filePath("loop.wav");
    const SINT start = 1234;
    const SINT end = 5678;
    const auto result = mixxx::library::exportLoopAsWav(source, start, end, target);
    ASSERT_EQ(ExportLoopStatus::Exported, result.status);
    const WavData wav = readWav(target);
    ASSERT_TRUE(wav.ok);
    ASSERT_EQ(end - start, wav.info.frames);
    for (SINT k = 0; k < wav.info.frames; ++k) {
        ASSERT_NEAR(m_source[(start + k) * 2], wav.samples[k * 2], 1) << k;
        ASSERT_NEAR(m_source[(start + k) * 2 + 1], wav.samples[k * 2 + 1], 1) << k;
    }
    // The first and the last sample are the in point and the frame before the
    // out point.
    EXPECT_NEAR(m_source[start * 2 + 1], wav.samples[1], 1);
    EXPECT_NEAR(m_source[(end - 1) * 2 + 1], wav.samples[(wav.info.frames - 1) * 2 + 1], 1);
}

TEST_F(ExportLoopTest, LoopAtTheStartOfTheFile) {
    ASSERT_TRUE(m_dir.isValid());
    const QString source = makeSource("src48.wav", 48000, 20000);
    const QString target = m_dir.filePath("loop.wav");
    const auto result = mixxx::library::exportLoopAsWav(source, 0, 3000, target);
    ASSERT_EQ(ExportLoopStatus::Exported, result.status);
    const WavData wav = readWav(target);
    ASSERT_TRUE(wav.ok);
    ASSERT_EQ(3000, wav.info.frames);
    EXPECT_NEAR(m_source[1], wav.samples[1], 1); // first frame, right channel: -10000
    for (SINT k = 0; k < 3000; ++k) {
        ASSERT_NEAR(m_source[k * 2], wav.samples[k * 2], 1) << k;
        ASSERT_NEAR(m_source[k * 2 + 1], wav.samples[k * 2 + 1], 1) << k;
    }
}

TEST_F(ExportLoopTest, LoopAtTheEndOfTheFile) {
    ASSERT_TRUE(m_dir.isValid());
    const QString source = makeSource("src48.wav", 48000, 20000);
    const QString target = m_dir.filePath("loop.wav");
    // The loop out point is exactly the end of the audio.
    auto result = mixxx::library::exportLoopAsWav(source, 17000, 20000, target);
    ASSERT_EQ(ExportLoopStatus::Exported, result.status);
    WavData wav = readWav(target);
    ASSERT_TRUE(wav.ok);
    ASSERT_EQ(3000, wav.info.frames);
    for (SINT k = 0; k < 3000; ++k) {
        ASSERT_NEAR(m_source[(17000 + k) * 2], wav.samples[k * 2], 1) << k;
        ASSERT_NEAR(m_source[(17000 + k) * 2 + 1], wav.samples[k * 2 + 1], 1) << k;
    }
    // An out point behind the end is cut to the end of the audio.
    result = mixxx::library::exportLoopAsWav(source, 17000, 25000, target);
    ASSERT_EQ(ExportLoopStatus::Exported, result.status);
    EXPECT_EQ(3000, result.framesWritten);
    wav = readWav(target);
    ASSERT_TRUE(wav.ok);
    EXPECT_EQ(3000, wav.info.frames);
}

TEST_F(ExportLoopTest, InvalidLoopGivesAnErrorAndNoFile) {
    ASSERT_TRUE(m_dir.isValid());
    const QString source = makeSource("src48.wav", 48000, 20000);
    const QString target = m_dir.filePath("loop.wav");
    // No length.
    EXPECT_EQ(ExportLoopStatus::InvalidLoop,
            mixxx::library::exportLoopAsWav(source, 5000, 5000, target).status);
    // The end is before the start.
    EXPECT_EQ(ExportLoopStatus::InvalidLoop,
            mixxx::library::exportLoopAsWav(source, 6000, 5000, target).status);
    // No loop (invalid positions).
    EXPECT_EQ(ExportLoopStatus::InvalidLoop,
            mixxx::library::exportLoopAsWav(source, -1, -1, target).status);
    EXPECT_EQ(ExportLoopStatus::InvalidLoop,
            mixxx::library::exportLoopAsWav(source, -1, 5000, target).status);
    // The loop starts behind the end of the audio.
    EXPECT_EQ(ExportLoopStatus::InvalidLoop,
            mixxx::library::exportLoopAsWav(source, 20000, 30000, target).status);
    EXPECT_FALSE(QFile::exists(target));
    EXPECT_EQ(QStringList{"src48.wav"}, filesInDir());
}

TEST_F(ExportLoopTest, MissingSourceGivesAnErrorAndNoFile) {
    ASSERT_TRUE(m_dir.isValid());
    const QString target = m_dir.filePath("loop.wav");
    EXPECT_EQ(ExportLoopStatus::SourceMissing,
            mixxx::library::exportLoopAsWav(m_dir.filePath("nothing.wav"), 0, 100, target).status);
    EXPECT_TRUE(filesInDir().isEmpty());
}

TEST_F(ExportLoopTest, MissingTargetFolderGivesAnError) {
    ASSERT_TRUE(m_dir.isValid());
    const QString source = makeSource("src48.wav", 48000, 20000);
    EXPECT_EQ(ExportLoopStatus::TargetFolderMissing,
            mixxx::library::exportLoopAsWav(
                    source, 0, 100, m_dir.filePath("nodir/loop.wav"))
                    .status);
}

TEST_F(ExportLoopTest, TheSourceFileIsNeverChanged) {
    ASSERT_TRUE(m_dir.isValid());
    const QString source = makeSource("src48.wav", 48000, 20000);
    const QByteArray before = fileHash(source);
    ASSERT_FALSE(before.isEmpty());
    // The target is the source: refused, and the source stays as it is.
    EXPECT_EQ(ExportLoopStatus::TargetIsSource,
            mixxx::library::exportLoopAsWav(source, 0, 1000, source).status);
    EXPECT_EQ(ExportLoopStatus::TargetIsSource,
            mixxx::library::exportLoopAsWav(
                    source, 0, 1000, m_dir.path() + "/./src48.wav")
                    .status);
    EXPECT_EQ(before, fileHash(source));
    // A normal export leaves it as it was, too.
    EXPECT_EQ(ExportLoopStatus::Exported,
            mixxx::library::exportLoopAsWav(source, 0, 1000, m_dir.filePath("loop.wav")).status);
    EXPECT_EQ(before, fileHash(source));
}

TEST_F(ExportLoopTest, AnExistingTargetIsReplacedOnlyByACompleteFile) {
    ASSERT_TRUE(m_dir.isValid());
    const QString source = makeSource("src48.wav", 48000, 20000);
    const QString target = m_dir.filePath("loop.wav");
    {
        QFile old(target);
        ASSERT_TRUE(old.open(QIODevice::WriteOnly));
        old.write("old content");
    }
    const QByteArray oldHash = fileHash(target);
    // A failed export keeps the old file.
    EXPECT_EQ(ExportLoopStatus::InvalidLoop,
            mixxx::library::exportLoopAsWav(source, 100, 100, target).status);
    EXPECT_EQ(oldHash, fileHash(target));
    // A good export replaces it.
    EXPECT_EQ(ExportLoopStatus::Exported,
            mixxx::library::exportLoopAsWav(source, 0, 1000, target).status);
    const WavData wav = readWav(target);
    ASSERT_TRUE(wav.ok);
    EXPECT_EQ(1000, wav.info.frames);
    EXPECT_EQ((QStringList{"loop.wav", "src48.wav"}), filesInDir());
}

TEST_F(ExportLoopTest, ErrorTextsAreSetForEveryError) {
    using mixxx::library::exportLoopErrorText;
    EXPECT_TRUE(exportLoopErrorText(ExportLoopStatus::Exported).isEmpty());
    for (auto status : {ExportLoopStatus::InvalidLoop,
                 ExportLoopStatus::SourceMissing,
                 ExportLoopStatus::SourceOpenFailed,
                 ExportLoopStatus::SourceReadFailed,
                 ExportLoopStatus::TargetIsSource,
                 ExportLoopStatus::TargetFolderMissing,
                 ExportLoopStatus::WriteFailed}) {
        EXPECT_FALSE(exportLoopErrorText(status).isEmpty());
    }
}

} // namespace
// End of AI-generated text.
