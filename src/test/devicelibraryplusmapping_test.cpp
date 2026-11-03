#include "library/rekordbox/devicelibraryplusmapping.h"

#include <gtest/gtest.h>

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QTemporaryDir>
#include <initializer_list>
#include <optional>

#include "mixxxtest.h"
#include "proto/keys.pb.h"

namespace {

using mixxx::rekordbox::dlpAnlzFolder;
using mixxx::rekordbox::dlpAudioStickPath;
using mixxx::rekordbox::dlpBitDepth;
using mixxx::rekordbox::dlpBpmx100;
using mixxx::rekordbox::dlpColorId;
using mixxx::rekordbox::dlpDate;
using mixxx::rekordbox::dlpFileType;
using mixxx::rekordbox::dlpIsSafeStickPath;
using mixxx::rekordbox::dlpKeyName;
using mixxx::rekordbox::dlpMpegPcmFrames;
using mixxx::rekordbox::dlpPathKey;
using mixxx::rekordbox::fatSafeName;

/// A Layer III frame: the header, then zero bytes up to `length`.
QByteArray mpegFrame(quint8 byte1, quint8 byte2, quint8 byte3, int length) {
    QByteArray frame;
    frame.append(static_cast<char>(0xff));
    frame.append(static_cast<char>(byte1));
    frame.append(static_cast<char>(byte2));
    frame.append(static_cast<char>(byte3));
    frame.append(QByteArray(length - frame.size(), '\0'));
    return frame;
}

// MPEG-1 Layer III, no CRC, 44.1 kHz, stereo. 128 kbit/s: 417 bytes, 418 with padding.
QByteArray mpeg1Frame(bool padding = false) {
    return mpegFrame(0xfb, padding ? 0x92 : 0x90, 0x00, padding ? 418 : 417);
}

// MPEG-2 Layer III, no CRC, 22.05 kHz, stereo. 64 kbit/s: 208 bytes.
QByteArray mpeg2Frame() {
    return mpegFrame(0xf3, 0x80, 0x00, 208);
}

QByteArray id3v2Tag(int bodySize) {
    QByteArray tag("ID3\x04\x00\x00", 6);
    tag.append(static_cast<char>((bodySize >> 21) & 0x7f));
    tag.append(static_cast<char>((bodySize >> 14) & 0x7f));
    tag.append(static_cast<char>((bodySize >> 7) & 0x7f));
    tag.append(static_cast<char>(bodySize & 0x7f));
    tag.append(QByteArray(bodySize, '\0'));
    return tag;
}

QByteArray id3v1Tag() {
    QByteArray tag("TAG");
    tag.append(QByteArray(125, '\0'));
    return tag;
}

class DeviceLibraryPlusMappingFiles : public ::testing::Test {
  protected:
    QString writeFile(const QString& name, const QByteArray& bytes) {
        const QString path = m_dir.filePath(name);
        QFile file(path);
        EXPECT_TRUE(file.open(QIODevice::WriteOnly));
        file.write(bytes);
        return path;
    }

    QTemporaryDir m_dir;
};

QByteArray bytesOf(std::initializer_list<int> values) {
    QByteArray bytes;
    for (const int value : values) {
        bytes.append(static_cast<char>(value));
    }
    return bytes;
}
QByteArray u16le(quint16 v) {
    return bytesOf({v & 0xff, v >> 8});
}
QByteArray u32le(quint32 v) {
    return u16le(v & 0xffff) + u16le(v >> 16);
}
QByteArray u16be(quint16 v) {
    return bytesOf({v >> 8, v & 0xff});
}
QByteArray u32be(quint32 v) {
    return u16be(v >> 16) + u16be(v & 0xffff);
}

} // namespace

TEST(DeviceLibraryPlusMappingTest, FileTypeCodes) {
    EXPECT_EQ(1, dlpFileType(QStringLiteral("mp3")));
    EXPECT_EQ(1, dlpFileType(QStringLiteral("MP3")));
    EXPECT_EQ(3, dlpFileType(QStringLiteral("mp4")));
    EXPECT_EQ(4, dlpFileType(QStringLiteral("m4a")));
    EXPECT_EQ(4, dlpFileType(QStringLiteral("aac")));
    EXPECT_EQ(5, dlpFileType(QStringLiteral("flac")));
    EXPECT_EQ(11, dlpFileType(QStringLiteral("wav")));
    EXPECT_EQ(12, dlpFileType(QStringLiteral("aif")));
    EXPECT_EQ(12, dlpFileType(QStringLiteral("aiff")));
    EXPECT_EQ(0, dlpFileType(QStringLiteral("ogg")));
    EXPECT_EQ(0, dlpFileType(QString()));
}

TEST(DeviceLibraryPlusMappingTest, ColorIdNearest) {
    EXPECT_EQ(1, dlpColorId(qRgb(0xF8, 0x70, 0xF8)));
    EXPECT_EQ(2, dlpColorId(qRgb(0xF8, 0x09, 0x00)));
    EXPECT_EQ(3, dlpColorId(qRgb(0xF8, 0xA0, 0x30)));
    EXPECT_EQ(4, dlpColorId(qRgb(0xF8, 0xE3, 0x31)));
    EXPECT_EQ(5, dlpColorId(qRgb(0x1E, 0xE0, 0x00)));
    EXPECT_EQ(6, dlpColorId(qRgb(0x16, 0xC0, 0xF8)));
    EXPECT_EQ(7, dlpColorId(qRgb(0x01, 0x50, 0xF8)));
    EXPECT_EQ(8, dlpColorId(qRgb(0x98, 0x08, 0xF8)));
    EXPECT_EQ(2, dlpColorId(QRgb(0xFF0000)));
    EXPECT_EQ(0, dlpColorId(std::nullopt));
    // Black gives no colour, as the ANLZ writer wrote it before this function.
    EXPECT_EQ(0, dlpColorId(QRgb(0x000000)));
}

TEST(DeviceLibraryPlusMappingTest, KeyNames) {
    EXPECT_QSTRING_EQ(QStringLiteral("8A"), dlpKeyName(mixxx::track::io::key::A_MINOR));
    EXPECT_QSTRING_EQ(QStringLiteral("8B"), dlpKeyName(mixxx::track::io::key::C_MAJOR));
    EXPECT_TRUE(dlpKeyName(mixxx::track::io::key::INVALID).isEmpty());
}

TEST(DeviceLibraryPlusMappingTest, BpmRounding) {
    EXPECT_EQ(12800, dlpBpmx100(128.004));
    EXPECT_EQ(12800, dlpBpmx100(127.995));
    EXPECT_EQ(0, dlpBpmx100(std::numeric_limits<double>::quiet_NaN()));
    EXPECT_EQ(0, dlpBpmx100(std::numeric_limits<double>::infinity()));
    EXPECT_EQ(0, dlpBpmx100(0.0));
    EXPECT_EQ(0, dlpBpmx100(-5.0));
}

TEST(DeviceLibraryPlusMappingTest, DateFormat) {
    const QDateTime dateTime(QDate(2026, 3, 9), QTime(12, 0));
    EXPECT_QSTRING_EQ(QStringLiteral("2026-03-09"), dlpDate(dateTime));
    // Not valid: today.
    EXPECT_QSTRING_EQ(QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")),
            dlpDate(QDateTime()));
}

TEST(DeviceLibraryPlusMappingTest, FatSafeNames) {
    EXPECT_QSTRING_EQ(QStringLiteral("AC_DC"), fatSafeName(QStringLiteral("AC/DC")));
    EXPECT_QSTRING_EQ(QStringLiteral("What_"), fatSafeName(QStringLiteral("What?")));
    EXPECT_QSTRING_EQ(QStringLiteral("a_b_c_d_e_f_g_h_i"),
            fatSafeName(QStringLiteral("a\\b/c:d*e?f\"g<h>i")));
    EXPECT_QSTRING_EQ(QStringLiteral("a_b_c"),
            fatSafeName(QStringLiteral("a|b\x01"
                                       "c")));
    EXPECT_QSTRING_EQ(QStringLiteral("Track"), fatSafeName(QStringLiteral("Track.")));
    EXPECT_QSTRING_EQ(QStringLiteral("Track"), fatSafeName(QStringLiteral("Track. . ")));
    EXPECT_QSTRING_EQ(QStringLiteral("_CON.mp3"), fatSafeName(QStringLiteral("CON.mp3")));
    EXPECT_QSTRING_EQ(QStringLiteral("_con"), fatSafeName(QStringLiteral("con")));
    EXPECT_QSTRING_EQ(QStringLiteral("_Lpt9.wav"), fatSafeName(QStringLiteral("Lpt9.wav")));
    EXPECT_QSTRING_EQ(QStringLiteral("_COM1"), fatSafeName(QStringLiteral("COM1")));
    EXPECT_QSTRING_EQ(QStringLiteral("_NUL"), fatSafeName(QStringLiteral("NUL")));
    EXPECT_QSTRING_EQ(QStringLiteral("COM0"), fatSafeName(QStringLiteral("COM0")));
    EXPECT_QSTRING_EQ(QStringLiteral("CONSOLE.mp3"), fatSafeName(QStringLiteral("CONSOLE.mp3")));
    EXPECT_QSTRING_EQ(QStringLiteral("_"), fatSafeName(QString()));
    EXPECT_QSTRING_EQ(QStringLiteral("_"), fatSafeName(QStringLiteral("...")));
    EXPECT_QSTRING_EQ(QStringLiteral("Zoë - Tëst.mp3"), fatSafeName(QStringLiteral("Zoë - Tëst.mp3")));

    // 300 characters and an extension: 255 units, ends in .mp3.
    const QString longName = QString(300, QLatin1Char('a')) + QStringLiteral(".mp3");
    const QString cutName = fatSafeName(longName);
    EXPECT_EQ(255, cutName.size());
    EXPECT_TRUE(cutName.endsWith(QStringLiteral(".mp3")));

    // A surrogate pair at the cut is not split.
    const QString pair = QString::fromUtf8("\xF0\x9F\x98\x80");
    ASSERT_EQ(2, pair.size());
    const QString noExtension = QString(254, QLatin1Char('a')) + pair + QStringLiteral("bbb");
    EXPECT_EQ(QString(254, QLatin1Char('a')), fatSafeName(noExtension));
    const QString withExtension = QString(250, QLatin1Char('a')) + pair +
            QString(20, QLatin1Char('b')) + QStringLiteral(".mp3");
    EXPECT_EQ(QString(250, QLatin1Char('a')) + QStringLiteral(".mp3"),
            fatSafeName(withExtension));
    // A pair that fits is kept.
    const QString fits = QString(253, QLatin1Char('a')) + pair + QStringLiteral("bbb");
    EXPECT_EQ(QString(253, QLatin1Char('a')) + pair, fatSafeName(fits));
}

TEST(DeviceLibraryPlusMappingTest, AudioStickPath) {
    EXPECT_QSTRING_EQ(QStringLiteral("/Contents/UnknownArtist/UnknownAlbum/x.mp3"),
            dlpAudioStickPath(QString(), QString(), QStringLiteral("x.mp3")));
    EXPECT_QSTRING_EQ(QStringLiteral("/Contents/AC_DC/Back In Black/Hells Bells.mp3"),
            dlpAudioStickPath(QStringLiteral("AC/DC"),
                    QStringLiteral("Back In Black"),
                    QStringLiteral("Hells Bells.mp3")));
}

TEST(DeviceLibraryPlusMappingTest, PathKeyIgnoresCase) {
    EXPECT_QSTRING_EQ(dlpPathKey(QStringLiteral("/Contents/A/b.MP3")),
            dlpPathKey(QStringLiteral("/CONTENTS/a/B.mp3")));
    EXPECT_QSTRING_EQ(QStringLiteral("/contents/a/b.mp3"),
            dlpPathKey(QStringLiteral("\\Contents\\A\\B.mp3")));
    EXPECT_QSTRING_EQ(dlpPathKey(QStringLiteral("/Contents/Zoë/a.mp3")),
            dlpPathKey(QStringLiteral("/Contents/ZOË/A.mp3")));
}

TEST(DeviceLibraryPlusMappingTest, AnlzFolderIsStableAndFree) {
    const QString path = QStringLiteral("/Contents/Artist/Album/Track.mp3");
    const QString folder = dlpAnlzFolder(path, {});
    EXPECT_QSTRING_EQ(folder, dlpAnlzFolder(path, {}));
    EXPECT_QSTRING_EQ(folder, dlpAnlzFolder(QStringLiteral("/CONTENTS/artist/album/track.MP3"), {}));
    static const QRegularExpression kFormat(
            QStringLiteral("^/PIONEER/USBANLZ/P[0-9A-F]{3}/[0-9A-F]{8}$"));
    EXPECT_TRUE(kFormat.match(folder).hasMatch()) << qPrintable(folder);

    const QString other = dlpAnlzFolder(path, {dlpPathKey(folder)});
    EXPECT_NE(folder, other);
    EXPECT_TRUE(kFormat.match(other).hasMatch()) << qPrintable(other);
    // Only the 32-bit part changes, by 1.
    EXPECT_EQ(folder.left(folder.size() - 8), other.left(other.size() - 8));
    EXPECT_EQ(folder.right(8).toUInt(nullptr, 16) + 1, other.right(8).toUInt(nullptr, 16));

    // Two folders in use: the third one.
    const QString third = dlpAnlzFolder(path, {dlpPathKey(folder), dlpPathKey(other)});
    EXPECT_NE(third, folder);
    EXPECT_NE(third, other);
}

TEST(DeviceLibraryPlusMappingTest, AnlzFolderOnDiskIsUsed) {
    QTemporaryDir stick;
    ASSERT_TRUE(stick.isValid());
    const QString path = QStringLiteral("/Contents/Artist/Album/Track.mp3");
    const QString folder = dlpAnlzFolder(path, {});
    // Without a stick root, or with an empty stick, the folder is free.
    EXPECT_QSTRING_EQ(folder, dlpAnlzFolder(path, {}, stick.path()));
    // A folder on the stick that no row uses is not free: another program
    // or an export that did not finish can have files in it.
    ASSERT_TRUE(QDir().mkpath(stick.path() + folder));
    const QString next = dlpAnlzFolder(path, {}, stick.path());
    EXPECT_NE(folder, next);
    EXPECT_EQ(folder.right(8).toUInt(nullptr, 16) + 1, next.right(8).toUInt(nullptr, 16));
    // A folder in use and the next one on the disk: the third one.
    ASSERT_TRUE(QDir().mkpath(stick.path() + next));
    const QString third = dlpAnlzFolder(path, {dlpPathKey(folder)}, stick.path());
    EXPECT_NE(third, folder);
    EXPECT_NE(third, next);
    EXPECT_FALSE(QFileInfo::exists(stick.path() + third));
}

TEST(DeviceLibraryPlusMappingTest, SafeStickPaths) {
    for (const char* path : {"/Contents/A/b.mp3",
                 "/PIONEER/USBANLZ/P001/00000001/ANLZ0000.DAT",
                 "/PIONEER/Artwork/00001/b1.jpg",
                 "/a",
                 "/Music/My Set/.hidden.mp3",
                 "/Music/a..b.mp3"}) {
        EXPECT_TRUE(dlpIsSafeStickPath(QString::fromUtf8(path))) << path;
    }
    EXPECT_TRUE(dlpIsSafeStickPath(QString::fromUtf8("/Contents/Zoë/€.mp3")));
    for (const char* path : {"",
                 "/",
                 "Contents/A/b.mp3",
                 "//Contents/b.mp3",
                 "/Contents//b.mp3",
                 "/Contents/A/",
                 "/Contents/./b.mp3",
                 "/Contents/../b.mp3",
                 "/PIONEER/Artwork/../../../Users/x/a.jpg",
                 "/PIONEER/Artwork/00001/..",
                 "/.",
                 "/..",
                 "\\Contents\\b.mp3",
                 "/Contents\\..\\b.mp3",
                 "/C:/Windows/a.mp3",
                 "/Contents/a.mp3:stream"}) {
        EXPECT_FALSE(dlpIsSafeStickPath(QString::fromUtf8(path))) << path;
    }
    // A control character, for example a NUL or a line feed.
    EXPECT_FALSE(dlpIsSafeStickPath(QStringLiteral("/Contents/a") + QChar(0) +
            QStringLiteral(".mp3")));
    EXPECT_FALSE(dlpIsSafeStickPath(QStringLiteral("/Contents/a\n.mp3")));
}

TEST_F(DeviceLibraryPlusMappingFiles, MpegFramesCountsFrames) {
    QByteArray file = id3v2Tag(90); // 10 bytes header + 90 = 100 bytes
    ASSERT_EQ(100, file.size());
    // The Info frame: "Info" after the side info (36 bytes from the start).
    QByteArray infoFrame = mpeg1Frame();
    infoFrame.replace(36, 4, "Info");
    file += infoFrame;
    for (int i = 0; i < 10; ++i) {
        file += mpeg1Frame(i == 3); // one frame has the padding byte
    }
    file += id3v1Tag();
    const QString path = writeFile(QStringLiteral("frames.mp3"), file);
    const std::optional<quint32> frames = dlpMpegPcmFrames(path);
    ASSERT_TRUE(frames.has_value());
    EXPECT_EQ(11520u, *frames);
}

TEST_F(DeviceLibraryPlusMappingFiles, MpegFramesWithoutInfoFrameAndWithApeTag) {
    QByteArray file;
    for (int i = 0; i < 4; ++i) {
        file += mpeg1Frame();
    }
    file += QByteArray("APETAGEX") + QByteArray(100, '\xff'); // false syncs after the tag
    const std::optional<quint32> frames =
            dlpMpegPcmFrames(writeFile(QStringLiteral("ape.mp3"), file));
    ASSERT_TRUE(frames.has_value());
    EXPECT_EQ(4u * 1152u, *frames);
}

TEST_F(DeviceLibraryPlusMappingFiles, MpegFramesSkipsGarbageAndFalseSync) {
    QByteArray file(50, '\0');
    file += QByteArray("\xff\xfb\x90", 3); // a lone header start in garbage
    file += QByteArray(20, '\0');
    for (int i = 0; i < 5; ++i) {
        file += mpeg1Frame();
    }
    const std::optional<quint32> frames =
            dlpMpegPcmFrames(writeFile(QStringLiteral("garbage.mp3"), file));
    ASSERT_TRUE(frames.has_value());
    EXPECT_EQ(5u * 1152u, *frames);
}

TEST_F(DeviceLibraryPlusMappingFiles, MpegFramesMpeg2) {
    QByteArray file = id3v2Tag(20);
    for (int i = 0; i < 7; ++i) {
        file += mpeg2Frame();
    }
    const std::optional<quint32> frames =
            dlpMpegPcmFrames(writeFile(QStringLiteral("mpeg2.mp3"), file));
    ASSERT_TRUE(frames.has_value());
    EXPECT_EQ(7u * 576u, *frames);
}

TEST(DeviceLibraryPlusMappingTest, MpegFramesVbrTestFile) {
    // The file has a Xing header with a frame count. The function must give
    // that count times 1152 (the Xing frame itself is not counted).
    const QString path = MixxxTest::getOrInitTestDir().filePath(
            QStringLiteral("id3-test-data/cover-test-øé~ł€˚-vbr.mp3"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    const QByteArray data = file.readAll();
    ASSERT_GT(data.size(), 10);
    ASSERT_TRUE(data.startsWith("ID3"));
    const int tagEnd = 10 +
            ((static_cast<quint8>(data.at(6)) << 21) |
                    (static_cast<quint8>(data.at(7)) << 14) |
                    (static_cast<quint8>(data.at(8)) << 7) |
                    static_cast<quint8>(data.at(9)));
    // The Xing header is within the first 40 bytes of the first frame.
    const int xing = data.indexOf("Xing", tagEnd);
    ASSERT_GE(xing, tagEnd);
    ASSERT_LT(xing, tagEnd + 40);
    const quint32 flags = (static_cast<quint8>(data.at(xing + 4)) << 24) |
            (static_cast<quint8>(data.at(xing + 5)) << 16) |
            (static_cast<quint8>(data.at(xing + 6)) << 8) |
            static_cast<quint8>(data.at(xing + 7));
    ASSERT_TRUE(flags & 0x1) << "no frame count in the Xing header";
    const quint32 xingFrames = (static_cast<quint8>(data.at(xing + 8)) << 24) |
            (static_cast<quint8>(data.at(xing + 9)) << 16) |
            (static_cast<quint8>(data.at(xing + 10)) << 8) |
            static_cast<quint8>(data.at(xing + 11));

    const std::optional<quint32> frames = dlpMpegPcmFrames(path);
    ASSERT_TRUE(frames.has_value());
    EXPECT_EQ(xingFrames * 1152u, *frames);
}

TEST_F(DeviceLibraryPlusMappingFiles, MpegFramesNoAudio) {
    EXPECT_FALSE(dlpMpegPcmFrames(writeFile(QStringLiteral("text.mp3"),
                                          QByteArray("This is not audio.\n")))
                    .has_value());
    EXPECT_FALSE(dlpMpegPcmFrames(writeFile(QStringLiteral("empty.mp3"), QByteArray()))
                    .has_value());
    EXPECT_FALSE(dlpMpegPcmFrames(m_dir.filePath(QStringLiteral("missing.mp3"))).has_value());
}

TEST_F(DeviceLibraryPlusMappingFiles, BitDepthFromHeaders) {
    // 24-bit WAV and AIFF, made here: the test data files are 16-bit.
    const QByteArray pcm(60, '\0');
    QByteArray fmt = u16le(1) + u16le(2) + u32le(44100) + u32le(44100 * 6) + u16le(6) + u16le(24);
    QByteArray wavBody = QByteArray("WAVE") + "fmt " + u32le(fmt.size()) + fmt + "data" +
            u32le(pcm.size()) + pcm;
    const QString wav = writeFile(QStringLiteral("a.wav"), QByteArray("RIFF") + u32le(wavBody.size()) + wavBody);

    // 80-bit extended 44100: exponent 0x400E, mantissa 0xAC44 << 48.
    const QByteArray comm = u16be(2) + u32be(10) + u16be(24) + u16be(0x400e) + u32be(0xac440000) + u32be(0);
    const QByteArray ssnd = u32be(0) + u32be(0) + pcm;
    const QByteArray aiffBody = QByteArray("AIFF") + "COMM" + u32be(comm.size()) + comm +
            "SSND" + u32be(ssnd.size()) + ssnd;
    const QString aiff = writeFile(QStringLiteral("a.aiff"), QByteArray("FORM") + u32be(aiffBody.size()) + aiffBody);

    EXPECT_EQ(24, dlpBitDepth(wav, 11));
    EXPECT_EQ(24, dlpBitDepth(aiff, 12));
    // The other types and the failures give 16.
    EXPECT_EQ(16, dlpBitDepth(wav, 1));
    EXPECT_EQ(16, dlpBitDepth(wav, 0));
    EXPECT_EQ(16, dlpBitDepth(m_dir.filePath(QStringLiteral("missing.wav")), 11));
    EXPECT_EQ(16, dlpBitDepth(m_dir.filePath(QStringLiteral("missing.flac")), 5));
    EXPECT_EQ(16, dlpBitDepth(writeFile(QStringLiteral("text.wav"), QByteArray("no")), 11));
}

TEST(DeviceLibraryPlusMappingTest, BitDepthOfFlacTestFile) {
    const QString flac = MixxxTest::getOrInitTestDir().filePath(
            QStringLiteral("id3-test-data/cover-test-øé~ł€˚.flac"));
    EXPECT_EQ(16, dlpBitDepth(flac, 5));
}
