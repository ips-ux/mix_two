#include "library/rekordbox/devicelibraryplusexporter.h"

#include <gtest/gtest.h>
#include <sqlite3.h>

#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QList>
#include <QRegularExpression>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QTime>
#include <initializer_list>
#include <memory>
#include <optional>

#include "audio/frame.h"
#include "audio/types.h"
#include "library/rekordbox/devicelibraryplusdelete.h"
#include "library/rekordbox/devicelibrarypluskey.h"
#include "library/rekordbox/devicelibraryplusmapping.h"
#include "library/rekordbox/sqlciphercodec.h"
#include "proto/keys.pb.h"
#include "sources/soundsourceproxy.h"
#include "test/mixxxtest.h"
#include "test/soundsourceproviderregistration.h"
#include "track/beats.h"
#include "track/bpm.h"
#include "track/cue.h"
#include "track/track.h"
#include "waveform/waveform.h"

namespace mixxx {
namespace rekordbox {

namespace {

constexpr qint64 kMasterDbId = 4242;
constexpr qint64 kOtherMasterDbId = 99;
const QString kDeviceName = QStringLiteral("MIX TWO TEST");
const QString kSampleDeviceName = QStringLiteral("MIX TWO SAMPLE");
const QString kOggFile = QString::fromUtf8("cover-test-øé~ł€˚.ogg");
const QString kWavFile = QString::fromUtf8("cover-test-øé~ł€˚.wav");
const QString kFlacFile = QString::fromUtf8("cover-test-øé~ł€˚.flac");
const QString kCoverMp3File = QString::fromUtf8("cover-test-øé~ł€˚-jpg.mp3");

QByteArray readFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    return file.readAll();
}

/// The relative paths of all files below a folder.
QStringList filesBelow(const QString& root) {
    QStringList paths;
    QDirIterator it(root, QDir::Files | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        paths.append(QDir(root).relativeFilePath(it.next()));
    }
    paths.sort();
    return paths;
}

/// The library file of a stick, decrypted into a temporary folder, for SQL
/// queries of the tests. It reads what save() wrote to the stick.
class StickLibrary {
  public:
    explicit StickLibrary(const QString& stickRoot) {
        if (!m_dir.isValid()) {
            m_error = QStringLiteral("no temporary folder");
            return;
        }
        const QByteArray file = readFile(
                QDir(stickRoot).filePath(DeviceLibraryPlusDb::kRelativePath));
        const auto plain = sqlcipher::decrypt(file, deviceLibraryPlusPassphrase(), &m_error);
        if (!plain) {
            return;
        }
        const QString path = m_dir.filePath(QStringLiteral("plain.db"));
        QFile out(path);
        if (!out.open(QIODevice::WriteOnly) || out.write(*plain) != plain->size()) {
            m_error = QStringLiteral("cannot write the plain copy");
            return;
        }
        out.close();
        if (sqlite3_open_v2(path.toUtf8().constData(), &m_pDb, SQLITE_OPEN_READWRITE, nullptr) !=
                SQLITE_OK) {
            m_error = QStringLiteral("cannot open the plain copy");
        }
    }
    ~StickLibrary() {
        sqlite3_close(m_pDb);
    }
    StickLibrary(const StickLibrary&) = delete;
    StickLibrary& operator=(const StickLibrary&) = delete;

    bool isValid() const {
        return m_pDb && m_error.isEmpty();
    }
    QString error() const {
        return m_error;
    }

    /// The rows of a query, each row as the quote() values of its columns
    /// joined by '|': NULL, 0, '' and 'text' are different.
    QStringList rows(const QString& sql) const {
        QStringList result;
        sqlite3_stmt* pStmt = nullptr;
        if (sqlite3_prepare_v2(m_pDb, sql.toUtf8().constData(), -1, &pStmt, nullptr) !=
                SQLITE_OK) {
            result.append(QString(QStringLiteral("SQL ERROR: ") +
                    QString::fromUtf8(sqlite3_errmsg(m_pDb))));
            return result;
        }
        while (sqlite3_step(pStmt) == SQLITE_ROW) {
            QStringList columns;
            for (int i = 0; i < sqlite3_column_count(pStmt); ++i) {
                if (sqlite3_column_type(pStmt, i) == SQLITE_NULL) {
                    columns.append(QStringLiteral("NULL"));
                } else {
                    columns.append(QString::fromUtf8(reinterpret_cast<const char*>(
                            sqlite3_column_text(pStmt, i))));
                }
            }
            result.append(columns.join('|'));
        }
        sqlite3_finalize(pStmt);
        return result;
    }

    /// The first row of a query; an empty string when there is none.
    QString value(const QString& sql) const {
        const QStringList all = rows(sql);
        return all.isEmpty() ? QString() : all.first();
    }

  private:
    QTemporaryDir m_dir;
    sqlite3* m_pDb = nullptr;
    QString m_error;
};

class DeviceLibraryPlusExporterTest : public MixxxTest, ::SoundSourceProviderRegistration {
  protected:
    void SetUp() override {
        ASSERT_TRUE(m_computer.isValid());
        ASSERT_TRUE(m_stick.isValid());
    }

    QString stickRoot() const {
        return m_stick.path();
    }

    /// The absolute path of a stick path.
    QString onStick(const QString& stickPath) const {
        return m_stick.path() + stickPath;
    }

    /// Copies a file of src/test/id3-test-data to a folder and returns the
    /// copy's path.
    static QString copyTestFile(
            const QString& testFile, const QString& folder, const QString& name = QString()) {
        const QString source = getOrInitTestDir().filePath(QStringLiteral("id3-test-data/") + testFile);
        EXPECT_TRUE(QDir().mkpath(folder));
        const QString target = QDir(folder).filePath(name.isEmpty() ? testFile : name);
        EXPECT_TRUE(QFile::copy(source, target)) << qPrintable(target);
        QFile::setPermissions(target, QFile::ReadOwner | QFile::WriteOwner);
        return target;
    }

    /// Copies a test file into a folder of the "computer".
    QString toComputer(const QString& testFile,
            const QString& subFolder = QStringLiteral("music"),
            const QString& name = QString()) const {
        return copyTestFile(testFile, m_computer.filePath(subFolder), name);
    }

    /// A track of a file with the metadata of the file, a 120 BPM grid, one
    /// hot cue and a date added. A file that is missing gives a track with
    /// only the path.
    static TrackPointer makeTrack(const QString& location, int id) {
        TrackPointer pTrack = Track::newDummy(location, TrackId(QVariant(id)));
        if (!QFileInfo::exists(location)) {
            return pTrack;
        }
        EXPECT_EQ(SoundSourceProxy::UpdateTrackFromSourceResult::MetadataImportedAndUpdated,
                SoundSourceProxy(pTrack).updateTrackFromSource(
                        SoundSourceProxy::UpdateTrackFromSourceMode::Once,
                        SyncTrackMetadataParams{}));
        const audio::SampleRate sampleRate = pTrack->getSampleRate();
        EXPECT_TRUE(sampleRate.isValid());
        pTrack->trySetBeats(Beats::fromConstTempo(sampleRate, audio::FramePos(0), Bpm(120)));
        pTrack->createAndAddCue(mixxx::CueType::HotCue,
                0,
                audio::FramePos(100),
                audio::kInvalidFramePos);
        pTrack->setDateAdded(QDateTime(QDate(2026, 10, 1), QTime(12, 0)));
        return pTrack;
    }

    /// A complete waveform for the length of the track.
    static ConstWaveformPointer makeWaveform(const Track& track) {
        const int sampleRate = static_cast<int>(track.getSampleRate().value());
        const auto frames = static_cast<SINT>(track.getDuration() * sampleRate);
        QSharedPointer<Waveform> pWaveform(new Waveform(sampleRate, frames, 441, -1, 0));
        WaveformData* pData = pWaveform->data();
        for (int i = 0; i < pWaveform->getDataSize(); ++i) {
            pData[i].filtered.low = 100;
            pData[i].filtered.mid = 100;
            pData[i].filtered.high = 100;
            pData[i].filtered.all = 100;
        }
        pWaveform->setCompletion(pWaveform->getDataSize());
        return pWaveform;
    }

    /// A track of an Ogg file, without its metadata (players cannot play the
    /// type; the export skips it before it reads the metadata).
    static DlpExportTrack oggItem(const QString& location, int id) {
        DlpExportTrack item;
        item.pTrack = Track::newDummy(location, TrackId(QVariant(id)));
        return item;
    }

    static DlpExportTrack itemFor(const TrackPointer& pTrack,
            const QImage& cover = QImage(),
            std::optional<mixxx::cache_key_t> coverHash = std::nullopt) {
        DlpExportTrack item;
        item.pTrack = pTrack;
        item.pWaveform = makeWaveform(*pTrack);
        item.cover = cover;
        item.coverHash = coverHash;
        return item;
    }

    /// A 300x200 image: red at the left, blue at the right.
    static QImage makeCover() {
        QImage image(300, 200, QImage::Format_RGB32);
        image.fill(Qt::red);
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 150; x < image.width(); ++x) {
                image.setPixel(x, y, qRgb(0, 0, 255));
            }
        }
        return image;
    }

    std::unique_ptr<DeviceLibraryPlusDb> newDb(qint64 masterDbId = kMasterDbId) const {
        QString error;
        auto pDb = DeviceLibraryPlusDb::create(stickRoot(), kDeviceName, masterDbId, &error);
        EXPECT_TRUE(pDb) << qPrintable(error);
        return pDb;
    }

    std::unique_ptr<DeviceLibraryPlusDb> openDb() const {
        QString error;
        auto pDb = DeviceLibraryPlusDb::open(stickRoot(), &error);
        EXPECT_TRUE(pDb) << qPrintable(error);
        return pDb;
    }

    /// Ruling R3: create() writes nothing before save(). This writes an empty
    /// library to the stick.
    void saveEmptyLibrary(qint64 masterDbId = kMasterDbId) const {
        auto pDb = newDb(masterDbId);
        ASSERT_TRUE(pDb);
        QString error;
        ASSERT_TRUE(pDb->save(&error)) << qPrintable(error);
    }

    DeviceLibraryPlusExporter::Options options() const {
        DeviceLibraryPlusExporter::Options result;
        result.stickRoot = stickRoot();
        result.masterDbId = kMasterDbId;
        return result;
    }

    // AI-generated (Claude Code). Start of AI-generated text.
    /// The counts of an exporter after finish().
    struct ExportCounts {
        qint64 bytesCopied = -1;
        int audioFilesCopied = -1;
        int anlzWritten = -1;
        int anlzKept = -1;
        int replacedFiles = -1;
    };
    // End of AI-generated text.

    /// Exports the tracks with a new exporter on the database and calls
    /// finish(). Returns the results. `stickRoot` replaces the stick root of
    /// options() when it is not empty. `pCounts` (if it is not null) gets the
    /// counts of the exporter.
    QList<DlpTrackResult> exportAndFinish(DeviceLibraryPlusDb* pDb,
            const QList<DlpExportTrack>& items,
            const QList<DlpExportPlaylist>& playlists = {},
            const QList<DlpExportPlaylist>& crates = {},
            const QString& stickRoot = QString(),
            ExportCounts* pCounts = nullptr) const {
        QList<DlpTrackResult> results;
        DeviceLibraryPlusExporter::Options exportOptions = options();
        if (!stickRoot.isEmpty()) {
            exportOptions.stickRoot = stickRoot;
        }
        DeviceLibraryPlusExporter exporter(pDb, exportOptions);
        QString error;
        EXPECT_TRUE(exporter.prepare(&error)) << qPrintable(error);
        for (const DlpExportTrack& item : items) {
            results.append(exporter.exportTrack(item));
        }
        EXPECT_TRUE(exporter.exportPlaylists(playlists, crates, &error)) << qPrintable(error);
        if (pCounts) {
            // Before finish(): finish() does not change the counts.
            pCounts->replacedFiles = exporter.replacedFileCount();
        }
        EXPECT_TRUE(exporter.finish(&error)) << qPrintable(error);
        if (pCounts) {
            pCounts->bytesCopied = exporter.bytesCopied();
            pCounts->audioFilesCopied = exporter.audioFilesCopied();
            pCounts->anlzWritten = exporter.anlzWrittenCount();
            pCounts->anlzKept = exporter.anlzKeptCount();
        }
        return results;
    }

    QTemporaryDir m_computer;
    QTemporaryDir m_stick;
};

const QRegularExpression kAnlzPathPattern(
        QStringLiteral("^/PIONEER/USBANLZ/P[0-9A-F]{3}/[0-9A-F]{8}/ANLZ0000\\.DAT$"));

} // namespace

TEST_F(DeviceLibraryPlusExporterTest, ExportsOneTrack) {
    const QString source = toComputer(QStringLiteral("artist.mp3"));
    TrackPointer pTrack = makeTrack(source, 7);
    pTrack->setTitle(QStringLiteral("One Track"));
    pTrack->setArtist(QStringLiteral("Test Artist"));
    pTrack->setAlbum(QStringLiteral("Test Album"));
    pTrack->setKey(track::io::key::A_MINOR, track::io::key::USER);
    const int sampleRate = static_cast<int>(pTrack->getSampleRate().value());
    {
        auto pDb = newDb();
        ASSERT_TRUE(pDb);
        const auto results = exportAndFinish(pDb.get(), {itemFor(pTrack)});
        ASSERT_EQ(1, results.size());
        ASSERT_TRUE(results[0].contentId) << qPrintable(results[0].skipReason);
        EXPECT_TRUE(results[0].skipReason.isEmpty());
    }

    const QString stickPath = QStringLiteral("/Contents/Test Artist/Test Album/artist.mp3");
    EXPECT_EQ(readFile(source), readFile(onStick(stickPath)));

    auto pDb = openDb();
    ASSERT_TRUE(pDb);
    const QList<DlpContentRef> contents = pDb->listContents();
    ASSERT_EQ(1, contents.size());
    EXPECT_EQ(stickPath, contents[0].path);
    EXPECT_EQ(kMasterDbId, contents[0].masterDbId);
    EXPECT_EQ(7, contents[0].masterContentId);
    const QString anlzPath = contents[0].analysisDataFilePath;
    EXPECT_TRUE(kAnlzPathPattern.match(anlzPath).hasMatch()) << qPrintable(anlzPath);
    const QString anlzFolder = QFileInfo(onStick(anlzPath)).path();
    EXPECT_EQ((QStringList{QStringLiteral("ANLZ0000.2EX"),
                      QStringLiteral("ANLZ0000.DAT"),
                      QStringLiteral("ANLZ0000.EXT")}),
            filesBelow(anlzFolder));

    StickLibrary library(stickRoot());
    ASSERT_TRUE(library.isValid()) << qPrintable(library.error());
    EXPECT_EQ(QStringLiteral("One Track|12000|%1|1|16|%2|%3|7|2026-10-01|2026-10-01|"
                             "41|788224|1|0|artist.mp3|%4")
                      .arg(QString::number(qRound(pTrack->getDuration())),
                              QString::number(sampleRate),
                              QString::number(kMasterDbId),
                              QString::number(QFileInfo(source).size())),
            library.value(QStringLiteral(
                    "SELECT title, bpmx100, length, fileType, bitDepth, samplingRate, "
                    "masterDbId, masterContentId, dateCreated, dateAdded, analysedBits, "
                    "contentLink, isHotCueAutoLoadOn, hasModified, fileName, fileSize "
                    "FROM content")));
    EXPECT_EQ(QStringLiteral("Test Artist|Test Album|8A|NULL"),
            library.value(QStringLiteral(
                    "SELECT a.name, al.name, k.name, c.image_id FROM content c "
                    "JOIN artist a ON a.artist_id = c.artist_id_artist "
                    "JOIN album al ON al.album_id = c.album_id "
                    "JOIN key k ON k.key_id = c.key_id")));
    // The album artist is the track artist when the album artist is empty.
    EXPECT_EQ(QStringLiteral("Test Artist"),
            library.value(QStringLiteral(
                    "SELECT a.name FROM album al JOIN artist a ON a.artist_id = al.artist_id")));
    EXPECT_EQ(QStringLiteral("1"),
            library.value(QStringLiteral("SELECT numberOfContents FROM property")));
}

// AI-generated (Claude Code). Start of AI-generated text.
TEST_F(DeviceLibraryPlusExporterTest, WritesTheHotCueAutoLoadFlagOfEachTrack) {
    TrackPointer pOn = makeTrack(toComputer(QStringLiteral("artist.mp3"), "a"), 1);
    TrackPointer pOff = makeTrack(toComputer(QStringLiteral("all.mp3"), "b"), 2);
    pOn->setTitle(QStringLiteral("Flag On"));
    pOff->setTitle(QStringLiteral("Flag Off"));
    // A track has the flag on when it comes into the library.
    ASSERT_TRUE(pOn->isHotCueAutoLoad());
    pOff->setHotCueAutoLoad(false);
    {
        auto pDb = newDb();
        ASSERT_TRUE(pDb);
        const auto results = exportAndFinish(pDb.get(), {itemFor(pOn), itemFor(pOff)});
        ASSERT_TRUE(results[0].contentId) << qPrintable(results[0].skipReason);
        ASSERT_TRUE(results[1].contentId) << qPrintable(results[1].skipReason);
    }
    StickLibrary library(stickRoot());
    ASSERT_TRUE(library.isValid()) << qPrintable(library.error());
    EXPECT_EQ(QStringLiteral("1"),
            library.value(QStringLiteral(
                    "SELECT isHotCueAutoLoadOn FROM content WHERE title = 'Flag On'")));
    EXPECT_EQ(QStringLiteral("0"),
            library.value(QStringLiteral(
                    "SELECT isHotCueAutoLoadOn FROM content WHERE title = 'Flag Off'")));

    // A new export of the changed flag updates the row.
    pOff->setHotCueAutoLoad(true);
    pOn->setHotCueAutoLoad(false);
    {
        auto pDb = openDb();
        ASSERT_TRUE(pDb);
        exportAndFinish(pDb.get(), {itemFor(pOn), itemFor(pOff)});
    }
    StickLibrary again(stickRoot());
    ASSERT_TRUE(again.isValid()) << qPrintable(again.error());
    EXPECT_EQ(QStringLiteral("0"),
            again.value(QStringLiteral(
                    "SELECT isHotCueAutoLoadOn FROM content WHERE title = 'Flag On'")));
    EXPECT_EQ(QStringLiteral("1"),
            again.value(QStringLiteral(
                    "SELECT isHotCueAutoLoadOn FROM content WHERE title = 'Flag Off'")));
}
// End of AI-generated text.

TEST_F(DeviceLibraryPlusExporterTest, WritesArtwork) {
    TrackPointer pFirst = makeTrack(toComputer(QStringLiteral("artist.mp3"), "a"), 1);
    TrackPointer pSecond = makeTrack(toComputer(QStringLiteral("all.mp3"), "b"), 2);
    const QImage cover = makeCover();
    {
        auto pDb = newDb();
        ASSERT_TRUE(pDb);
        const auto results = exportAndFinish(pDb.get(),
                {itemFor(pFirst, cover, mixxx::cache_key_t(555)),
                        itemFor(pSecond, cover, mixxx::cache_key_t(555))});
        ASSERT_EQ(2, results.size());
        ASSERT_TRUE(results[0].contentId) << qPrintable(results[0].skipReason);
        ASSERT_TRUE(results[1].contentId) << qPrintable(results[1].skipReason);
        EXPECT_TRUE(results[0].warnings.isEmpty()) << qPrintable(results[0].warnings.join('\n'));
    }

    const QImage smallImage(onStick(QStringLiteral("/PIONEER/Artwork/00001/b1.jpg")));
    const QImage mediumImage(onStick(QStringLiteral("/PIONEER/Artwork/00001/b1_m.jpg")));
    ASSERT_FALSE(smallImage.isNull());
    ASSERT_FALSE(mediumImage.isNull());
    EXPECT_EQ(QSize(80, 80), smallImage.size());
    EXPECT_EQ(QSize(240, 240), mediumImage.size());
    // The square is cut from the centre: half red, half blue.
    EXPECT_GT(qRed(smallImage.pixel(10, 40)), 200);
    EXPECT_GT(qBlue(smallImage.pixel(70, 40)), 200);

    StickLibrary library(stickRoot());
    ASSERT_TRUE(library.isValid()) << qPrintable(library.error());
    EXPECT_EQ(QStringList{QStringLiteral("1|/PIONEER/Artwork/00001/b1.jpg")},
            library.rows(QStringLiteral("SELECT image_id, path FROM image")));
    EXPECT_EQ((QStringList{QStringLiteral("1"), QStringLiteral("1")}),
            library.rows(QStringLiteral("SELECT image_id FROM content ORDER BY content_id")));
    EXPECT_FALSE(QFileInfo::exists(onStick(QStringLiteral("/PIONEER/Artwork/00001/b2.jpg"))));
}

TEST_F(DeviceLibraryPlusExporterTest, ReExportUpdatesSameRow) {
    const QString source = toComputer(QStringLiteral("artist.mp3"));
    TrackPointer pTrack = makeTrack(source, 3);
    pTrack->setArtist(QStringLiteral("Again Artist"));
    pTrack->setAlbum(QStringLiteral("Again Album"));
    pTrack->setTitle(QStringLiteral("First Title"));
    {
        auto pDb = newDb();
        ASSERT_TRUE(pDb);
        const auto results = exportAndFinish(
                pDb.get(), {itemFor(pTrack, makeCover(), mixxx::cache_key_t(1))});
        ASSERT_TRUE(results[0].contentId) << qPrintable(results[0].skipReason);
    }
    const QString stickPath = QStringLiteral("/Contents/Again Artist/Again Album/artist.mp3");
    const QDateTime copied = QFileInfo(onStick(stickPath)).lastModified();
    ASSERT_TRUE(copied.isValid());
    const QString smallArtwork = onStick(QStringLiteral("/PIONEER/Artwork/00001/b1.jpg"));
    const QByteArray artworkBytes = readFile(smallArtwork);
    const QDateTime artworkWritten = QFileInfo(smallArtwork).lastModified();
    ASSERT_FALSE(artworkBytes.isEmpty());
    QList<DlpContentRef> before;
    {
        auto pDb = openDb();
        ASSERT_TRUE(pDb);
        before = pDb->listContents();
    }
    ASSERT_EQ(1, before.size());

    pTrack->setTitle(QStringLiteral("Second Title"));
    {
        auto pDb = openDb();
        ASSERT_TRUE(pDb);
        // Another cover hash in a new export: the track keeps its image row.
        const auto results = exportAndFinish(
                pDb.get(), {itemFor(pTrack, makeCover(), mixxx::cache_key_t(2))});
        ASSERT_TRUE(results[0].contentId) << qPrintable(results[0].skipReason);
        EXPECT_EQ(before[0].id, *results[0].contentId);
    }

    auto pDb = openDb();
    ASSERT_TRUE(pDb);
    const QList<DlpContentRef> after = pDb->listContents();
    ASSERT_EQ(1, after.size());
    EXPECT_EQ(before[0].id, after[0].id);
    EXPECT_EQ(stickPath, after[0].path);
    // AI-generated (Claude Code). Start of AI-generated text.
    // Ruling R17: the title is not in the ANLZ files, so their bytes did not
    // change. The row keeps its ANLZ folder.
    EXPECT_EQ(before[0].analysisDataFilePath, after[0].analysisDataFilePath);
    // End of AI-generated text.
    EXPECT_TRUE(kAnlzPathPattern.match(after[0].analysisDataFilePath).hasMatch())
            << qPrintable(after[0].analysisDataFilePath);
    const QString newAnlzFolder =
            QFileInfo(after[0].analysisDataFilePath).path().mid(QStringLiteral("/PIONEER/USBANLZ/").size());
    EXPECT_EQ((QStringList{newAnlzFolder + QStringLiteral("/ANLZ0000.2EX"),
                      newAnlzFolder + QStringLiteral("/ANLZ0000.DAT"),
                      newAnlzFolder + QStringLiteral("/ANLZ0000.EXT")}),
            filesBelow(onStick(QStringLiteral("/PIONEER/USBANLZ"))));
    // The same cover bytes: the track keeps its image row.
    EXPECT_EQ(before[0].imageId, after[0].imageId);
    EXPECT_EQ(copied, QFileInfo(onStick(stickPath)).lastModified());

    StickLibrary library(stickRoot());
    ASSERT_TRUE(library.isValid()) << qPrintable(library.error());
    EXPECT_EQ(QStringLiteral("Second Title"), library.value(QStringLiteral("SELECT title FROM content")));
    EXPECT_EQ(QStringLiteral("1"), library.value(QStringLiteral("SELECT count(*) FROM artist")));
    EXPECT_EQ(QStringLiteral("1"), library.value(QStringLiteral("SELECT count(*) FROM album")));
    EXPECT_EQ(QStringLiteral("1"), library.value(QStringLiteral("SELECT count(*) FROM image")));
    EXPECT_EQ((QStringList{QStringLiteral("00001/b1.jpg"), QStringLiteral("00001/b1_m.jpg")}),
            filesBelow(onStick(QStringLiteral("/PIONEER/Artwork"))));
    // Ruling R17: nothing is written for the same bytes.
    EXPECT_EQ(artworkBytes, readFile(smallArtwork));
    EXPECT_EQ(artworkWritten, QFileInfo(smallArtwork).lastModified());
}

// AI-generated (Claude Code). Start of AI-generated text.
namespace {

/// The modification time of each file below a folder, by its path.
QHash<QString, QDateTime> fileTimesBelow(const QString& root) {
    QHash<QString, QDateTime> times;
    QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        times.insert(path, QFileInfo(path).lastModified());
    }
    return times;
}

const QDateTime kAnlzMarkTime(QDate(2020, 1, 2), QTime(3, 4, 5));

} // namespace

TEST_F(DeviceLibraryPlusExporterTest, ReExportKeepsAnlzFolderWithSameBytes) {
    const QString sourceA = toComputer(QStringLiteral("artist.mp3"), "music", "a.mp3");
    const QString sourceB = toComputer(QStringLiteral("all.mp3"), "music", "b.mp3");
    TrackPointer pA = makeTrack(sourceA, 21);
    TrackPointer pB = makeTrack(sourceB, 22);
    pA->setArtist(QStringLiteral("Keep A"));
    pB->setArtist(QStringLiteral("Keep B"));
    ExportCounts counts;
    {
        auto pDb = newDb();
        ASSERT_TRUE(pDb);
        const auto results =
                exportAndFinish(pDb.get(), {itemFor(pA), itemFor(pB)}, {}, {}, QString(), &counts);
        ASSERT_TRUE(results[0].contentId && results[1].contentId);
    }
    // The first export copies both files and writes both ANLZ sets.
    EXPECT_EQ(QFileInfo(sourceA).size() + QFileInfo(sourceB).size(), counts.bytesCopied);
    EXPECT_EQ(2, counts.audioFilesCopied);
    EXPECT_EQ(2, counts.anlzWritten);
    EXPECT_EQ(0, counts.anlzKept);

    const QString anlzRoot = onStick(QStringLiteral("/PIONEER/USBANLZ"));
    QList<DlpContentRef> before;
    {
        auto pDb = openDb();
        ASSERT_TRUE(pDb);
        before = pDb->listContents();
    }
    ASSERT_EQ(2, before.size());
    // A marker time on each ANLZ file: a file that is written again loses it.
    const QHash<QString, QDateTime> firstFiles = fileTimesBelow(anlzRoot);
    ASSERT_EQ(6, firstFiles.size());
    for (auto it = firstFiles.constBegin(); it != firstFiles.constEnd(); ++it) {
        QFile file(it.key());
        ASSERT_TRUE(file.open(QIODevice::ReadWrite)) << qPrintable(it.key());
        ASSERT_TRUE(file.setFileTime(kAnlzMarkTime, QFileDevice::FileModificationTime));
    }

    // The same tracks again: nothing is copied or written.
    {
        auto pDb = openDb();
        ASSERT_TRUE(pDb);
        const auto results =
                exportAndFinish(pDb.get(), {itemFor(pA), itemFor(pB)}, {}, {}, QString(), &counts);
        ASSERT_TRUE(results[0].contentId && results[1].contentId);
    }
    EXPECT_EQ(0, counts.bytesCopied);
    EXPECT_EQ(0, counts.audioFilesCopied);
    EXPECT_EQ(0, counts.anlzWritten);
    EXPECT_EQ(2, counts.anlzKept);
    EXPECT_EQ(0, counts.replacedFiles);
    QList<DlpContentRef> kept;
    {
        auto pDb = openDb();
        ASSERT_TRUE(pDb);
        kept = pDb->listContents();
    }
    ASSERT_EQ(2, kept.size());
    for (int i = 0; i < 2; ++i) {
        EXPECT_EQ(before[i].analysisDataFilePath, kept[i].analysisDataFilePath);
    }
    const QHash<QString, QDateTime> keptFiles = fileTimesBelow(anlzRoot);
    EXPECT_EQ(firstFiles.keys().size(), keptFiles.size());
    for (auto it = keptFiles.constBegin(); it != keptFiles.constEnd(); ++it) {
        EXPECT_TRUE(firstFiles.contains(it.key())) << qPrintable(it.key());
        EXPECT_EQ(kAnlzMarkTime, it.value()) << qPrintable(it.key());
    }

    // Another hot cue for A, and one other byte in the EXT file of B (the
    // same size): both get a new folder, and finish() removes the old ones.
    pA->createAndAddCue(mixxx::CueType::HotCue, 1, audio::FramePos(5000), audio::kInvalidFramePos);
    const QString extOfB = QFileInfo(onStick(kept[1].analysisDataFilePath)).path() +
            QStringLiteral("/ANLZ0000.EXT");
    {
        QFile file(extOfB);
        ASSERT_TRUE(file.open(QIODevice::ReadWrite));
        QByteArray bytes = file.readAll();
        ASSERT_FALSE(bytes.isEmpty());
        bytes[bytes.size() - 1] = static_cast<char>(bytes.at(bytes.size() - 1) ^ 0x01);
        ASSERT_TRUE(file.seek(0));
        ASSERT_EQ(bytes.size(), file.write(bytes));
    }
    {
        auto pDb = openDb();
        ASSERT_TRUE(pDb);
        const auto results =
                exportAndFinish(pDb.get(), {itemFor(pA), itemFor(pB)}, {}, {}, QString(), &counts);
        ASSERT_TRUE(results[0].contentId && results[1].contentId);
    }
    EXPECT_EQ(0, counts.bytesCopied);
    EXPECT_EQ(2, counts.anlzWritten);
    EXPECT_EQ(0, counts.anlzKept);
    auto pDb = openDb();
    ASSERT_TRUE(pDb);
    const QList<DlpContentRef> after = pDb->listContents();
    ASSERT_EQ(2, after.size());
    for (int i = 0; i < 2; ++i) {
        EXPECT_NE(dlpPathKey(kept[i].analysisDataFilePath),
                dlpPathKey(after[i].analysisDataFilePath));
        EXPECT_TRUE(QFileInfo::exists(onStick(after[i].analysisDataFilePath)));
        EXPECT_FALSE(QFileInfo::exists(QFileInfo(onStick(kept[i].analysisDataFilePath)).path()));
    }
    EXPECT_EQ(6, fileTimesBelow(anlzRoot).size());
}

TEST_F(DeviceLibraryPlusExporterTest, MissingAnlzFileGivesNewFolder) {
    TrackPointer pTrack = makeTrack(toComputer(QStringLiteral("artist.mp3")), 23);
    {
        auto pDb = newDb();
        ASSERT_TRUE(pDb);
        ASSERT_TRUE(exportAndFinish(pDb.get(), {itemFor(pTrack)})[0].contentId);
    }
    QList<DlpContentRef> before;
    {
        auto pDb = openDb();
        ASSERT_TRUE(pDb);
        before = pDb->listContents();
    }
    ASSERT_EQ(1, before.size());
    ASSERT_TRUE(QFile::remove(
            QFileInfo(onStick(before[0].analysisDataFilePath)).path() +
            QStringLiteral("/ANLZ0000.EXT")));
    ExportCounts counts;
    {
        auto pDb = openDb();
        ASSERT_TRUE(pDb);
        ASSERT_TRUE(exportAndFinish(pDb.get(), {itemFor(pTrack)}, {}, {}, QString(), &counts)[0]
                            .contentId);
    }
    EXPECT_EQ(1, counts.anlzWritten);
    EXPECT_EQ(0, counts.anlzKept);
    auto pDb = openDb();
    ASSERT_TRUE(pDb);
    const QList<DlpContentRef> after = pDb->listContents();
    ASSERT_EQ(1, after.size());
    EXPECT_NE(dlpPathKey(before[0].analysisDataFilePath),
            dlpPathKey(after[0].analysisDataFilePath));
    EXPECT_TRUE(QFileInfo::exists(
            QFileInfo(onStick(after[0].analysisDataFilePath)).path() +
            QStringLiteral("/ANLZ0000.EXT")));
}
// End of AI-generated text.

TEST_F(DeviceLibraryPlusExporterTest, ReExportWithOtherCoverWritesNewImage) {
    TrackPointer pTrack = makeTrack(toComputer(QStringLiteral("artist.mp3")), 3);
    {
        auto pDb = newDb();
        ASSERT_TRUE(pDb);
        const auto results = exportAndFinish(
                pDb.get(), {itemFor(pTrack, makeCover(), mixxx::cache_key_t(1))});
        ASSERT_TRUE(results[0].contentId) << qPrintable(results[0].skipReason);
    }
    QImage green(100, 100, QImage::Format_RGB32);
    green.fill(Qt::green);
    {
        auto pDb = openDb();
        ASSERT_TRUE(pDb);
        const auto results =
                exportAndFinish(pDb.get(), {itemFor(pTrack, green, mixxx::cache_key_t(2))});
        ASSERT_TRUE(results[0].contentId) << qPrintable(results[0].skipReason);
        EXPECT_TRUE(results[0].warnings.isEmpty()) << qPrintable(results[0].warnings.join('\n'));
    }
    // Ruling R17: other bytes give a new image. After the save no row uses
    // b1, so its two files are removed. The image row stays.
    EXPECT_EQ((QStringList{QStringLiteral("00001/b2.jpg"), QStringLiteral("00001/b2_m.jpg")}),
            filesBelow(onStick(QStringLiteral("/PIONEER/Artwork"))));
    const QImage newCover(onStick(QStringLiteral("/PIONEER/Artwork/00001/b2.jpg")));
    ASSERT_FALSE(newCover.isNull());
    EXPECT_GT(qGreen(newCover.pixel(40, 40)), 200);
    StickLibrary library(stickRoot());
    ASSERT_TRUE(library.isValid()) << qPrintable(library.error());
    EXPECT_EQ((QStringList{QStringLiteral("1|/PIONEER/Artwork/00001/b1.jpg"),
                      QStringLiteral("2|/PIONEER/Artwork/00001/b2.jpg")}),
            library.rows(QStringLiteral("SELECT image_id, path FROM image ORDER BY image_id")));
    EXPECT_EQ(QStringLiteral("2"), library.value(QStringLiteral("SELECT image_id FROM content")));
}

TEST_F(DeviceLibraryPlusExporterTest, FailedSaveKeepsTheOldFiles) {
    TrackPointer pTrack = makeTrack(toComputer(QStringLiteral("artist.mp3")), 3);
    {
        auto pDb = newDb();
        ASSERT_TRUE(pDb);
        const auto results = exportAndFinish(
                pDb.get(), {itemFor(pTrack, makeCover(), mixxx::cache_key_t(1))});
        ASSERT_TRUE(results[0].contentId) << qPrintable(results[0].skipReason);
    }
    QList<DlpContentRef> before;
    {
        auto pDb = openDb();
        ASSERT_TRUE(pDb);
        before = pDb->listContents();
    }
    ASSERT_EQ(1, before.size());
    const QString anlzFolder = QFileInfo(onStick(before[0].analysisDataFilePath)).path();
    QStringList oldFiles;
    for (const char* name : {"ANLZ0000.DAT", "ANLZ0000.EXT", "ANLZ0000.2EX"}) {
        oldFiles.append(anlzFolder + QLatin1Char('/') + QLatin1String(name));
    }
    oldFiles.append(onStick(QStringLiteral("/PIONEER/Artwork/00001/b1.jpg")));
    oldFiles.append(onStick(QStringLiteral("/PIONEER/Artwork/00001/b1_m.jpg")));
    QList<QByteArray> oldBytes;
    for (const QString& file : oldFiles) {
        oldBytes.append(readFile(file));
        ASSERT_FALSE(oldBytes.last().isEmpty()) << qPrintable(file);
    }

    // Other data for the track: another hot cue and another cover.
    pTrack->createAndAddCue(mixxx::CueType::HotCue, 1, audio::FramePos(5000), audio::kInvalidFramePos);
    QImage green(100, 100, QImage::Format_RGB32);
    green.fill(Qt::green);
    const QString libraryFile = QDir(stickRoot()).filePath(DeviceLibraryPlusDb::kRelativePath);
    {
        auto pDb = openDb();
        ASSERT_TRUE(pDb);
        DeviceLibraryPlusExporter exporter(pDb.get(), options());
        QString error;
        ASSERT_TRUE(exporter.prepare(&error)) << qPrintable(error);
        ASSERT_TRUE(exporter.exportTrack(itemFor(pTrack, green, mixxx::cache_key_t(2))).contentId);
        ASSERT_TRUE(exporter.exportPlaylists({}, {}, &error)) << qPrintable(error);
        // Another program changes the library file, so save() refuses.
        QFile file(libraryFile);
        ASSERT_TRUE(file.open(QIODevice::ReadWrite));
        ASSERT_TRUE(file.setFileTime(QDateTime::currentDateTime().addSecs(-3600),
                QFileDevice::FileModificationTime));
        file.close();
        EXPECT_FALSE(exporter.finish(&error));
    }
    // The library on the stick still points at its old files, and they did
    // not change.
    for (int i = 0; i < oldFiles.size(); ++i) {
        EXPECT_EQ(oldBytes.at(i), readFile(oldFiles.at(i))) << qPrintable(oldFiles.at(i));
    }
    auto pDb = openDb();
    ASSERT_TRUE(pDb);
    const QList<DlpContentRef> after = pDb->listContents();
    ASSERT_EQ(1, after.size());
    EXPECT_EQ(before[0].analysisDataFilePath, after[0].analysisDataFilePath);
    EXPECT_EQ(before[0].imageId, after[0].imageId);
}

TEST_F(DeviceLibraryPlusExporterTest, ChangedSourceGetsNewCopy) {
    // Ruling R18.
    const QString source = toComputer(QStringLiteral("artist.mp3"), "music", "track.mp3");
    TrackPointer pTrack = makeTrack(source, 4);
    pTrack->setArtist(QStringLiteral("Changed"));
    pTrack->setAlbum(QStringLiteral("Album"));
    {
        auto pDb = newDb();
        ASSERT_TRUE(pDb);
        const auto results = exportAndFinish(pDb.get(), {itemFor(pTrack)});
        ASSERT_TRUE(results[0].contentId) << qPrintable(results[0].skipReason);
    }
    const QString oldPath = QStringLiteral("/Contents/Changed/Album/track.mp3");
    ASSERT_EQ(readFile(source), readFile(onStick(oldPath)));

    // The DJ replaces the file with one of another size (a new encoding).
    ASSERT_TRUE(QFile::remove(source));
    copyTestFile(QStringLiteral("all.mp3"), m_computer.filePath(QStringLiteral("music")), "track.mp3");
    ASSERT_NE(QFileInfo(source).size(), QFileInfo(onStick(oldPath)).size());
    {
        auto pDb = openDb();
        ASSERT_TRUE(pDb);
        DeviceLibraryPlusExporter exporter(pDb.get(), options());
        QString error;
        ASSERT_TRUE(exporter.prepare(&error)) << qPrintable(error);
        EXPECT_EQ(QFileInfo(source).size(),
                exporter.bytesToCopy({DlpCopyCandidate{pTrack->getId(),
                        source,
                        pTrack->getArtist(),
                        pTrack->getAlbum()}}));
        ASSERT_TRUE(exporter.exportTrack(itemFor(pTrack)).contentId);
        ASSERT_TRUE(exporter.exportPlaylists({}, {}, &error)) << qPrintable(error);
        ASSERT_TRUE(exporter.finish(&error)) << qPrintable(error);
    }
    // The new file gets a new name; nothing is copied over the old file.
    const QString newPath = QStringLiteral("/Contents/Changed/Album/track (2).mp3");
    EXPECT_EQ(readFile(source), readFile(onStick(newPath)));
    // After the save no row uses the old copy, so it is removed.
    EXPECT_FALSE(QFileInfo::exists(onStick(oldPath)));
    auto pDb = openDb();
    ASSERT_TRUE(pDb);
    const QList<DlpContentRef> rows = pDb->listContents();
    ASSERT_EQ(1, rows.size());
    EXPECT_EQ(newPath, rows[0].path);
    StickLibrary library(stickRoot());
    ASSERT_TRUE(library.isValid()) << qPrintable(library.error());
    EXPECT_EQ(QString::number(QFileInfo(source).size()),
            library.value(QStringLiteral("SELECT fileSize FROM content")));
}

TEST_F(DeviceLibraryPlusExporterTest, OwnFileOutsideContentsIsNeverReplaced) {
    // Ruling R18: a file outside /Contents is the DJ's own file.
    const QString stickFile = copyTestFile(
            QStringLiteral("artist.mp3"), onStick(QStringLiteral("/Music/Set")), "own.mp3");
    TrackPointer pTrack = makeTrack(stickFile, 12);
    {
        auto pDb = newDb();
        ASSERT_TRUE(pDb);
        const auto results = exportAndFinish(pDb.get(), {itemFor(pTrack)});
        ASSERT_TRUE(results[0].contentId) << qPrintable(results[0].skipReason);
    }
    const QByteArray djBytes = readFile(stickFile);
    // The track is now on the computer, in a file of another size.
    const QString moved = toComputer(QStringLiteral("all.mp3"), "music", "own.mp3");
    ASSERT_NE(QFileInfo(moved).size(), djBytes.size());
    TrackPointer pMoved = makeTrack(moved, 12);
    {
        auto pDb = openDb();
        ASSERT_TRUE(pDb);
        DeviceLibraryPlusExporter exporter(pDb.get(), options());
        QString error;
        ASSERT_TRUE(exporter.prepare(&error)) << qPrintable(error);
        EXPECT_EQ(0,
                exporter.bytesToCopy({DlpCopyCandidate{
                        pMoved->getId(), moved, pMoved->getArtist(), pMoved->getAlbum()}}));
        ASSERT_TRUE(exporter.exportTrack(itemFor(pMoved)).contentId);
        ASSERT_TRUE(exporter.finish(&error)) << qPrintable(error);
    }
    EXPECT_EQ(djBytes, readFile(stickFile));
    EXPECT_FALSE(QFileInfo::exists(onStick(QStringLiteral("/Contents"))));
    auto pDb = openDb();
    ASSERT_TRUE(pDb);
    const QList<DlpContentRef> rows = pDb->listContents();
    ASSERT_EQ(1, rows.size());
    EXPECT_EQ(QStringLiteral("/Music/Set/own.mp3"), rows[0].path);
}

// AI-generated (Claude Code). Start of AI-generated text.
TEST_F(DeviceLibraryPlusExporterTest, RekordboxTrackAtContentsTargetIsUpdatedInPlace) {
    // Ruling R19: rekordbox exported the track to the /Contents path that Mix
    // Two would use, and the file there has the size of the track. A stick
    // has one library: the export updates that row with Mix Two's data and
    // adds no second row.
    const QString source = toComputer(QStringLiteral("artist.mp3"));
    const QString target = QStringLiteral("/Contents/Shared Artist/UnknownAlbum/artist.mp3");
    const QString theirAnlz = QStringLiteral("/PIONEER/USBANLZ/P001/00000001/ANLZ0000.DAT");
    copyTestFile(QStringLiteral("artist.mp3"), QFileInfo(onStick(target)).path());
    ASSERT_TRUE(QDir().mkpath(QFileInfo(onStick(theirAnlz)).path()));
    {
        QFile anlz(onStick(theirAnlz));
        ASSERT_TRUE(anlz.open(QIODevice::WriteOnly));
        ASSERT_EQ(5, anlz.write("other"));
    }
    {
        auto pDb = newDb(kOtherMasterDbId);
        ASSERT_TRUE(pDb);
        DlpContent other;
        other.title = QStringLiteral("Their Title");
        other.path = target;
        other.fileName = QStringLiteral("artist.mp3");
        other.fileSize = QFileInfo(source).size();
        other.fileType = 1;
        other.masterDbId = kOtherMasterDbId;
        other.masterContentId = 7;
        other.analysisDataFilePath = theirAnlz;
        ASSERT_EQ(1, pDb->addContent(other));
        pDb->updateContentCount();
        QString error;
        ASSERT_TRUE(pDb->save(&error)) << qPrintable(error);
    }
    // Another Mix Two track ID than the masterContentId of the row: the row
    // is found by its stick path.
    TrackPointer pTrack = makeTrack(source, 3);
    pTrack->setArtist(QStringLiteral("Shared Artist"));
    pTrack->setAlbum(QString());
    pTrack->setTitle(QStringLiteral("Mix Two Title"));
    for (int run = 0; run < 2; ++run) {
        SCOPED_TRACE(run);
        auto pDb = openDb();
        ASSERT_TRUE(pDb);
        DeviceLibraryPlusExporter exporter(pDb.get(), options());
        QString error;
        ASSERT_TRUE(exporter.prepare(&error)) << qPrintable(error);
        EXPECT_EQ(0,
                exporter.bytesToCopy({DlpCopyCandidate{
                        pTrack->getId(), source, pTrack->getArtist(), pTrack->getAlbum()}}));
        const DlpTrackResult result =
                exporter.exportTrack(itemFor(pTrack, makeCover(), mixxx::cache_key_t(4)));
        ASSERT_TRUE(result.contentId) << qPrintable(result.skipReason);
        EXPECT_EQ(1, *result.contentId);
        EXPECT_TRUE(result.warnings.isEmpty()) << qPrintable(result.warnings.join('\n'));
        ASSERT_TRUE(exporter.exportPlaylists(
                {DlpExportPlaylist{QStringLiteral("Ours"), {pTrack->getId()}}}, {}, &error))
                << qPrintable(error);
        ASSERT_TRUE(exporter.finish(&error)) << qPrintable(error);
    }
    // One audio file, no second copy.
    EXPECT_EQ(QStringList{QStringLiteral("Contents/Shared Artist/UnknownAlbum/artist.mp3")},
            filesBelow(stickRoot()).filter(QStringLiteral("Contents/")));
    StickLibrary library(stickRoot());
    ASSERT_TRUE(library.isValid()) << qPrintable(library.error());
    EXPECT_EQ(QStringLiteral("1"), library.value(QStringLiteral("SELECT count(*) FROM content")));
    EXPECT_EQ(QStringLiteral("1"),
            library.value(QStringLiteral("SELECT numberOfContents FROM property")));
    // Mix Two's data is in the row. The row keeps the library IDs that it
    // had (a choice for the controller to confirm).
    EXPECT_EQ(QStringLiteral("1|Mix Two Title|%1|%2|7")
                      .arg(target, QString::number(kOtherMasterDbId)),
            library.value(QStringLiteral("SELECT content_id, title, path, masterDbId, "
                                         "masterContentId FROM content")));
    const QString anlzPath =
            library.value(QStringLiteral("SELECT analysisDataFilePath FROM content"));
    EXPECT_TRUE(kAnlzPathPattern.match(anlzPath).hasMatch()) << qPrintable(anlzPath);
    EXPECT_NE(theirAnlz, anlzPath);
    EXPECT_TRUE(QFileInfo::exists(onStick(anlzPath)));
    // The old ANLZ file of the row has no user now.
    EXPECT_FALSE(QFileInfo::exists(onStick(theirAnlz)));
    EXPECT_EQ(QStringLiteral("1"), library.value(QStringLiteral("SELECT count(*) FROM image")));
    EXPECT_NE(QStringLiteral("NULL"),
            library.value(QStringLiteral("SELECT image_id FROM content")));
    EXPECT_EQ(QStringLiteral("1"),
            library.value(QStringLiteral(
                    "SELECT group_concat(pc.content_id) FROM playlist_content pc "
                    "JOIN playlist p ON p.playlist_id = pc.playlist_id WHERE p.name = 'Ours'")));
}
// End of AI-generated text.

TEST_F(DeviceLibraryPlusExporterTest, SkippedTrackKeepsOlderRowInPlaylists) {
    // Ruling R20: the music drive is not connected for the second export.
    TrackPointer pA = makeTrack(toComputer(QStringLiteral("artist.mp3"), "a"), 1);
    TrackPointer pB = makeTrack(toComputer(QStringLiteral("all.mp3"), "b"), 2);
    const QList<DlpExportPlaylist> playlists{
            DlpExportPlaylist{QStringLiteral("Set"), {pB->getId(), pA->getId()}}};
    {
        auto pDb = newDb();
        ASSERT_TRUE(pDb);
        const auto results = exportAndFinish(pDb.get(), {itemFor(pA), itemFor(pB)}, playlists);
        ASSERT_TRUE(results[0].contentId) << qPrintable(results[0].skipReason);
        ASSERT_TRUE(results[1].contentId) << qPrintable(results[1].skipReason);
    }
    QList<DlpContentRef> before;
    {
        auto pDb = openDb();
        ASSERT_TRUE(pDb);
        before = pDb->listContents();
    }
    ASSERT_EQ(2, before.size());
    ASSERT_TRUE(QFile::remove(pA->getLocation()));
    ASSERT_TRUE(QFile::remove(pB->getLocation()));
    {
        auto pDb = openDb();
        ASSERT_TRUE(pDb);
        const auto results = exportAndFinish(pDb.get(), {itemFor(pA), itemFor(pB)}, playlists);
        for (const DlpTrackResult& result : results) {
            EXPECT_FALSE(result.contentId);
            EXPECT_EQ(QStringLiteral("The track file is missing."), result.skipReason);
            EXPECT_EQ(QStringList{QStringLiteral(
                              "The stick has an older copy of this track. The playlist uses it.")},
                    result.warnings);
        }
    }
    auto pDb = openDb();
    ASSERT_TRUE(pDb);
    const QList<DlpContentRef> after = pDb->listContents();
    ASSERT_EQ(2, after.size());
    for (int i = 0; i < 2; ++i) {
        EXPECT_EQ(before[i].path, after[i].path);
        EXPECT_EQ(before[i].analysisDataFilePath, after[i].analysisDataFilePath);
        EXPECT_TRUE(QFileInfo::exists(onStick(after[i].analysisDataFilePath)));
    }
    StickLibrary library(stickRoot());
    ASSERT_TRUE(library.isValid()) << qPrintable(library.error());
    EXPECT_EQ(QStringLiteral("%1,%2").arg(before[1].id).arg(before[0].id),
            library.value(QStringLiteral(
                    "SELECT group_concat(content_id, ',') FROM (SELECT pc.content_id "
                    "FROM playlist_content pc JOIN playlist p ON p.playlist_id = pc.playlist_id "
                    "WHERE p.name = 'Set' ORDER BY pc.sequenceNo)")));
}

TEST_F(DeviceLibraryPlusExporterTest, NoTrackExportedRefusesFinish) {
    // Ruling R20: a first export in which every track is skipped.
    TrackPointer pA = makeTrack(m_computer.filePath(QStringLiteral("gone.mp3")), 1);
    const DlpExportTrack skippedItem = oggItem(toComputer(kOggFile), 2);
    auto pDb = newDb();
    ASSERT_TRUE(pDb);
    DeviceLibraryPlusExporter exporter(pDb.get(), options());
    QString error;
    ASSERT_TRUE(exporter.prepare(&error)) << qPrintable(error);
    DlpExportTrack item;
    item.pTrack = pA;
    for (const DlpExportTrack& each : {item, skippedItem}) {
        const DlpTrackResult result = exporter.exportTrack(each);
        EXPECT_FALSE(result.contentId);
        EXPECT_TRUE(result.warnings.isEmpty()) << qPrintable(result.warnings.join('\n'));
    }
    EXPECT_EQ(0, exporter.usableTrackCount());
    ASSERT_TRUE(exporter.exportPlaylists(
            {DlpExportPlaylist{QStringLiteral("Set"), {pA->getId()}}}, {}, &error));
    EXPECT_FALSE(exporter.finish(&error));
    EXPECT_EQ(QStringLiteral("No track was exported. The stick did not change."), error);
    EXPECT_TRUE(filesBelow(stickRoot()).isEmpty());
}

TEST_F(DeviceLibraryPlusExporterTest, CraftedRowPathsAreNotUsed) {
    // Important 4: rows of Mix Two (its masterDbId) with paths that leave the
    // stick. The stick is a folder in a larger folder, so that the test can
    // see a file outside the stick.
    const QString outer = m_computer.filePath(QStringLiteral("outer"));
    const QString root = outer + QStringLiteral("/stick");
    ASSERT_TRUE(QDir().mkpath(root));
    const QByteArray victimBytes("not a file of Mix Two");
    const QStringList victims{outer + QStringLiteral("/victim/a.mp3"),
            outer + QStringLiteral("/victim/ANLZ0000.DAT"),
            outer + QStringLiteral("/victim/ANLZ0000.EXT"),
            outer + QStringLiteral("/victim/ANLZ0000.2EX"),
            outer + QStringLiteral("/victim/b1.jpg"),
            outer + QStringLiteral("/victim/b1_m.jpg"),
            // The join of a path without a leading '/'.
            outer + QStringLiteral("/stickvictim.mp3")};
    for (const QString& victim : victims) {
        ASSERT_TRUE(QDir().mkpath(QFileInfo(victim).path()));
        QFile file(victim);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        ASSERT_EQ(victimBytes.size(), file.write(victimBytes));
    }
    {
        QString error;
        auto pDb = DeviceLibraryPlusDb::create(root, kDeviceName, kMasterDbId, &error);
        ASSERT_TRUE(pDb) << qPrintable(error);
        const qint64 imageId = pDb->addImage(QStringLiteral("/PIONEER/Artwork/../../../victim/b1.jpg"));
        DlpContent crafted;
        crafted.path = QStringLiteral("/../victim/a.mp3");
        crafted.fileName = QStringLiteral("a.mp3");
        crafted.fileType = 1;
        crafted.masterDbId = kMasterDbId;
        crafted.masterContentId = 5;
        crafted.analysisDataFilePath =
                QStringLiteral("/PIONEER/USBANLZ/../../../victim/ANLZ0000.DAT");
        crafted.imageId = imageId;
        ASSERT_GT(pDb->addContent(crafted), 0);
        DlpContent noSlash;
        noSlash.path = QStringLiteral("victim.mp3");
        noSlash.fileName = QStringLiteral("victim.mp3");
        noSlash.fileType = 1;
        noSlash.masterDbId = kMasterDbId;
        noSlash.masterContentId = 6;
        noSlash.analysisDataFilePath = QStringLiteral("/PIONEER/USBANLZ/P001/00000001/ANLZ0000.DAT");
        ASSERT_GT(pDb->addContent(noSlash), 0);
        ASSERT_TRUE(pDb->save(&error)) << qPrintable(error);
    }
    TrackPointer pA = makeTrack(toComputer(QStringLiteral("artist.mp3"), "a"), 5);
    TrackPointer pB = makeTrack(toComputer(QStringLiteral("all.mp3"), "b"), 6);
    {
        QString error;
        auto pDb = DeviceLibraryPlusDb::open(root, &error);
        ASSERT_TRUE(pDb) << qPrintable(error);
        DeviceLibraryPlusExporter::Options craftedOptions = options();
        craftedOptions.stickRoot = root;
        DeviceLibraryPlusExporter exporter(pDb.get(), craftedOptions);
        ASSERT_TRUE(exporter.prepare(&error)) << qPrintable(error);
        // The rows are treated as rows without a path: both files are copied.
        EXPECT_EQ(QFileInfo(pA->getLocation()).size() + QFileInfo(pB->getLocation()).size(),
                exporter.bytesToCopy(
                        {DlpCopyCandidate{pA->getId(), pA->getLocation(), QString(), QString()},
                                DlpCopyCandidate{
                                        pB->getId(), pB->getLocation(), QString(), QString()}}));
        for (const TrackPointer& pTrack : {pA, pB}) {
            const DlpTrackResult result =
                    exporter.exportTrack(itemFor(pTrack, makeCover(), mixxx::cache_key_t(8)));
            ASSERT_TRUE(result.contentId) << qPrintable(result.skipReason);
        }
        ASSERT_TRUE(exporter.finish(&error)) << qPrintable(error);
    }
    for (const QString& victim : victims) {
        EXPECT_EQ(victimBytes, readFile(victim)) << qPrintable(victim);
    }
    // Nothing else outside the stick.
    QStringList outside = filesBelow(outer).filter(QRegularExpression(QStringLiteral("^(?!stick/)")));
    outside.sort();
    QStringList expectedOutside{QStringLiteral("stickvictim.mp3"),
            QStringLiteral("victim/ANLZ0000.2EX"),
            QStringLiteral("victim/ANLZ0000.DAT"),
            QStringLiteral("victim/ANLZ0000.EXT"),
            QStringLiteral("victim/a.mp3"),
            QStringLiteral("victim/b1.jpg"),
            QStringLiteral("victim/b1_m.jpg")};
    EXPECT_EQ(expectedOutside, outside);
    QString error;
    auto pDb = DeviceLibraryPlusDb::open(root, &error);
    ASSERT_TRUE(pDb) << qPrintable(error);
    const QList<DlpContentRef> rows = pDb->listContents();
    ASSERT_EQ(2, rows.size());
    for (const DlpContentRef& row : rows) {
        EXPECT_TRUE(row.path.startsWith(QStringLiteral("/Contents/"))) << qPrintable(row.path);
        EXPECT_TRUE(dlpIsSafeStickPath(row.path)) << qPrintable(row.path);
        EXPECT_TRUE(QFileInfo(root + row.path).isFile()) << qPrintable(row.path);
        EXPECT_TRUE(kAnlzPathPattern.match(row.analysisDataFilePath).hasMatch())
                << qPrintable(row.analysisDataFilePath);
        EXPECT_TRUE(QFileInfo(root + row.analysisDataFilePath).isFile());
    }
}

TEST_F(DeviceLibraryPlusExporterTest, RemoveReplacedFilesChecksEveryPath) {
    // Rulings R17 and R18 with Important 4: the removal after the save takes
    // only safe paths below its three folders, and only files that no row
    // uses.
    const QString outer = m_computer.filePath(QStringLiteral("outer"));
    const QString root = outer + QStringLiteral("/stick");
    const QByteArray bytes("data");
    const auto put = [&bytes](const QString& path) {
        EXPECT_TRUE(QDir().mkpath(QFileInfo(path).path()));
        QFile file(path);
        EXPECT_TRUE(file.open(QIODevice::WriteOnly));
        file.write(bytes);
    };
    const QStringList kept{outer + QStringLiteral("/victim/ANLZ0000.DAT"),
            outer + QStringLiteral("/victim/b1.jpg"),
            outer + QStringLiteral("/victim/a.mp3"),
            root + QStringLiteral("/Contents/A/ANLZ0000.DAT"),
            root + QStringLiteral("/PIONEER/USBANLZ/P001/00000001/ANLZ0000.DAT"),
            root + QStringLiteral("/PIONEER/USBANLZ/P002/00000002/other.txt"),
            root + QStringLiteral("/PIONEER/Artwork/00001/b1.jpg"),
            root + QStringLiteral("/PIONEER/Artwork/00001/b1_m.jpg"),
            root + QStringLiteral("/Music/own.mp3"),
            root + QStringLiteral("/Contents/A/used.mp3")};
    const QStringList removed{root + QStringLiteral("/PIONEER/USBANLZ/P002/00000002/ANLZ0000.DAT"),
            root + QStringLiteral("/PIONEER/USBANLZ/P002/00000002/ANLZ0000.EXT"),
            root + QStringLiteral("/PIONEER/USBANLZ/P002/00000002/ANLZ0000.2EX"),
            root + QStringLiteral("/PIONEER/USBANLZ/P003/00000003/ANLZ0000.DAT"),
            root + QStringLiteral("/PIONEER/Artwork/00001/b2.jpg"),
            root + QStringLiteral("/PIONEER/Artwork/00001/b2_m.jpg"),
            root + QStringLiteral("/Contents/A/old.mp3")};
    for (const QString& path : kept + removed) {
        put(path);
    }

    DlpReplacedFiles replaced;
    replaced.anlzFolders = {QStringLiteral("/PIONEER/USBANLZ/../../../victim"),
            QStringLiteral("/Contents/A"),
            QStringLiteral("PIONEER/USBANLZ/P001/00000001"),
            QStringLiteral("/PIONEER/USBANLZ/P001/00000001"),
            QStringLiteral("/PIONEER/USBANLZ/P002/00000002"),
            QStringLiteral("/PIONEER/USBANLZ/P003/00000003")};
    replaced.imagePaths = {QStringLiteral("/PIONEER/Artwork/../../../victim/b1.jpg"),
            QStringLiteral("/PIONEER/Artwork/00001/b1.jpg"),
            QStringLiteral("/PIONEER/Artwork/00001/b2.jpg")};
    replaced.audioPaths = {QStringLiteral("/Music/own.mp3"),
            QStringLiteral("/Contents/../../victim/a.mp3"),
            QStringLiteral("/Contents/A/used.mp3"),
            QStringLiteral("/Contents/A/old.mp3")};
    DlpContentRef row;
    row.id = 1;
    row.path = QStringLiteral("/CONTENTS/a/USED.mp3");
    row.analysisDataFilePath = QStringLiteral("/PIONEER/USBANLZ/P001/00000001/ANLZ0000.DAT");
    row.imageId = 1;
    const QHash<qint64, QString> imagePaths{{1, QStringLiteral("/PIONEER/Artwork/00001/b1.jpg")},
            {2, QStringLiteral("/PIONEER/Artwork/00001/b2.jpg")}};

    EXPECT_EQ(removed.size(), dlpRemoveReplacedFiles(root, replaced, {row}, imagePaths));
    for (const QString& path : kept) {
        EXPECT_EQ(bytes, readFile(path)) << qPrintable(path);
    }
    for (const QString& path : removed) {
        EXPECT_FALSE(QFileInfo::exists(path)) << qPrintable(path);
    }
    // A folder with another file stays; an empty folder and its empty P###
    // folder go.
    EXPECT_TRUE(QFileInfo(root + QStringLiteral("/PIONEER/USBANLZ/P002/00000002")).isDir());
    EXPECT_FALSE(QFileInfo::exists(root + QStringLiteral("/PIONEER/USBANLZ/P003")));
    EXPECT_TRUE(QFileInfo(root + QStringLiteral("/PIONEER/USBANLZ")).isDir());
    // A second call removes nothing.
    EXPECT_EQ(0, dlpRemoveReplacedFiles(root, replaced, {row}, imagePaths));
    EXPECT_EQ(0, dlpRemoveReplacedFiles(QString(), replaced, {}, imagePaths));
}

TEST_F(DeviceLibraryPlusExporterTest, FolderOnStickIsNotUsedForAnlz) {
    TrackPointer pTrack = makeTrack(toComputer(QStringLiteral("artist.mp3")), 7);
    pTrack->setArtist(QStringLiteral("A"));
    pTrack->setAlbum(QStringLiteral("B"));
    const QString taken =
            dlpAnlzFolder(dlpAudioStickPath(QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("artist.mp3")), {});
    const QString foreign = onStick(taken + QStringLiteral("/ANLZ0000.DAT"));
    ASSERT_TRUE(QDir().mkpath(QFileInfo(foreign).path()));
    {
        QFile file(foreign);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write("foreign");
    }
    {
        auto pDb = newDb();
        ASSERT_TRUE(pDb);
        const auto results = exportAndFinish(pDb.get(), {itemFor(pTrack)});
        ASSERT_TRUE(results[0].contentId) << qPrintable(results[0].skipReason);
    }
    EXPECT_EQ(QByteArray("foreign"), readFile(foreign));
    auto pDb = openDb();
    ASSERT_TRUE(pDb);
    const QList<DlpContentRef> rows = pDb->listContents();
    ASSERT_EQ(1, rows.size());
    EXPECT_NE(dlpPathKey(taken + QStringLiteral("/ANLZ0000.DAT")),
            dlpPathKey(rows[0].analysisDataFilePath));
}

TEST_F(DeviceLibraryPlusExporterTest, ArtworkFileOnStickIsNotOverwritten) {
    const QString foreign = onStick(QStringLiteral("/PIONEER/Artwork/00001/b1.jpg"));
    ASSERT_TRUE(QDir().mkpath(QFileInfo(foreign).path()));
    {
        QFile file(foreign);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write("foreign");
    }
    TrackPointer pTrack = makeTrack(toComputer(QStringLiteral("artist.mp3")), 1);
    {
        auto pDb = newDb();
        ASSERT_TRUE(pDb);
        const auto results =
                exportAndFinish(pDb.get(), {itemFor(pTrack, makeCover(), mixxx::cache_key_t(3))});
        ASSERT_TRUE(results[0].contentId) << qPrintable(results[0].skipReason);
    }
    EXPECT_EQ(QByteArray("foreign"), readFile(foreign));
    EXPECT_FALSE(QFileInfo::exists(onStick(QStringLiteral("/PIONEER/Artwork/00001/b1_m.jpg"))));
    StickLibrary library(stickRoot());
    ASSERT_TRUE(library.isValid()) << qPrintable(library.error());
    EXPECT_EQ(QStringLiteral("/PIONEER/Artwork/00001/b2.jpg"),
            library.value(QStringLiteral("SELECT path FROM image")));
}

TEST_F(DeviceLibraryPlusExporterTest, BytesToCopyFollowsTheExport) {
    const QString sourceA = toComputer(QStringLiteral("artist.mp3"), "a");
    const QString sourceB = toComputer(QStringLiteral("all.mp3"), "b");
    TrackPointer pA = makeTrack(sourceA, 1);
    TrackPointer pB = makeTrack(sourceB, 2);
    const auto candidateOf = [](const TrackPointer& pTrack) {
        return DlpCopyCandidate{pTrack->getId(),
                pTrack->getLocation(),
                pTrack->getArtist(),
                pTrack->getAlbum()};
    };
    const qint64 sizeA = QFileInfo(sourceA).size();
    const qint64 sizeB = QFileInfo(sourceB).size();
    ASSERT_GT(sizeA, 0);
    ASSERT_GT(sizeB, 0);

    {
        auto pDb = newDb();
        ASSERT_TRUE(pDb);
        DeviceLibraryPlusExporter exporter(pDb.get(), options());
        ASSERT_TRUE(exporter.prepare(nullptr));
        // Before the first export the stick has none of the files.
        EXPECT_EQ(sizeA + sizeB, exporter.bytesToCopy({candidateOf(pA), candidateOf(pB)}));
        // Writes nothing.
        EXPECT_TRUE(filesBelow(stickRoot()).isEmpty());
        ASSERT_TRUE(exporter.exportTrack(itemFor(pA)).contentId);
        QString error;
        ASSERT_TRUE(exporter.finish(&error)) << qPrintable(error);
    }

    // A second export: track A has a row whose file exists, so it counts 0.
    // Track B is new and counts its size. The files stay on the computer.
    auto pDb = openDb();
    ASSERT_TRUE(pDb);
    DeviceLibraryPlusExporter exporter(pDb.get(), options());
    ASSERT_TRUE(exporter.prepare(nullptr));
    EXPECT_EQ(0, exporter.bytesToCopy({candidateOf(pA)}));
    EXPECT_EQ(sizeB, exporter.bytesToCopy({candidateOf(pB)}));
    EXPECT_EQ(sizeB, exporter.bytesToCopy({candidateOf(pA), candidateOf(pB)}));

    // A track that exportTrack() skips copies nothing.
    EXPECT_EQ(0, exporter.bytesToCopy({DlpCopyCandidate{TrackId(QVariant(3)), m_computer.filePath(QStringLiteral("gone.mp3")), QString(), QString()}}));
    EXPECT_EQ(0, exporter.bytesToCopy({DlpCopyCandidate{TrackId(), sourceB, QString(), QString()}}));
    EXPECT_EQ(0, exporter.bytesToCopy({}));

    // A file at the target with the same size and no row is used, not copied
    // (spec 7.3).
    const QString target =
            onStick(dlpAudioStickPath(pB->getArtist(), pB->getAlbum(), QStringLiteral("all.mp3")));
    ASSERT_TRUE(QDir().mkpath(QFileInfo(target).path()));
    ASSERT_TRUE(QFile::copy(sourceB, target));
    EXPECT_EQ(0, exporter.bytesToCopy({candidateOf(pB)}));
}

TEST_F(DeviceLibraryPlusExporterTest, TrackOnStickIsNotCopied) {
    const QString source = copyTestFile(
            QStringLiteral("artist.mp3"), onStick(QStringLiteral("/Music/My Set")), "own.mp3");
    TrackPointer pTrack = makeTrack(source, 11);
    {
        auto pDb = newDb();
        ASSERT_TRUE(pDb);
        const auto results = exportAndFinish(pDb.get(), {itemFor(pTrack)});
        ASSERT_TRUE(results[0].contentId) << qPrintable(results[0].skipReason);
    }
    EXPECT_FALSE(QFileInfo::exists(onStick(QStringLiteral("/Contents"))));
    QList<DlpContentRef> before;
    {
        auto pDb = openDb();
        ASSERT_TRUE(pDb);
        before = pDb->listContents();
    }
    ASSERT_EQ(1, before.size());
    EXPECT_EQ(QStringLiteral("/Music/My Set/own.mp3"), before[0].path);

    // The same file, with the path in other letter case (FAT32 and exFAT do
    // not tell the two apart), and the stick root with native separators and
    // a separator at the end. A case-sensitive file system of the temporary
    // folder keeps the path of the first export.
    QString location = stickRoot() + QStringLiteral("/MUSIC/my set/OWN.mp3");
    if (!QFileInfo::exists(location)) {
        location = source;
    }
    TrackPointer pAgain = makeTrack(location, 11);
    {
        auto pDb = openDb();
        ASSERT_TRUE(pDb);
        const auto results = exportAndFinish(pDb.get(),
                {itemFor(pAgain)},
                {},
                {},
                QDir::toNativeSeparators(stickRoot()) + QDir::separator());
        ASSERT_TRUE(results[0].contentId) << qPrintable(results[0].skipReason);
        EXPECT_EQ(before[0].id, *results[0].contentId);
    }
    auto pDb = openDb();
    ASSERT_TRUE(pDb);
    const QList<DlpContentRef> after = pDb->listContents();
    ASSERT_EQ(1, after.size());
    EXPECT_EQ(before[0].path, after[0].path);
    // AI-generated (Claude Code). Start of AI-generated text.
    // Ruling R17: one ANLZ folder stays, the folder of the row: the old one
    // when the bytes are the same, else a new one.
    EXPECT_TRUE(QFileInfo::exists(onStick(after[0].analysisDataFilePath)));
    const QString anlzFolder = QFileInfo(after[0].analysisDataFilePath)
                                       .path()
                                       .mid(QStringLiteral("/PIONEER/USBANLZ/").size());
    EXPECT_EQ((QStringList{anlzFolder + QStringLiteral("/ANLZ0000.2EX"),
                      anlzFolder + QStringLiteral("/ANLZ0000.DAT"),
                      anlzFolder + QStringLiteral("/ANLZ0000.EXT")}),
            filesBelow(onStick(QStringLiteral("/PIONEER/USBANLZ"))));
    // End of AI-generated text.
    EXPECT_FALSE(QFileInfo::exists(onStick(QStringLiteral("/Contents"))));
}

TEST_F(DeviceLibraryPlusExporterTest, SameFileNameGetsNumber) {
    TrackPointer pFirst = makeTrack(toComputer(QStringLiteral("artist.mp3"), "a", "x.mp3"), 1);
    TrackPointer pSecond = makeTrack(toComputer(QStringLiteral("all.mp3"), "b", "x.mp3"), 2);
    for (const TrackPointer& pTrack : {pFirst, pSecond}) {
        pTrack->setArtist(QStringLiteral("Same"));
        pTrack->setAlbum(QStringLiteral("Same"));
    }
    {
        auto pDb = newDb();
        ASSERT_TRUE(pDb);
        const auto results = exportAndFinish(pDb.get(), {itemFor(pFirst), itemFor(pSecond)});
        ASSERT_TRUE(results[0].contentId) << qPrintable(results[0].skipReason);
        ASSERT_TRUE(results[1].contentId) << qPrintable(results[1].skipReason);
    }
    EXPECT_EQ(readFile(pFirst->getLocation()),
            readFile(onStick(QStringLiteral("/Contents/Same/Same/x.mp3"))));
    EXPECT_EQ(readFile(pSecond->getLocation()),
            readFile(onStick(QStringLiteral("/Contents/Same/Same/x (2).mp3"))));
    StickLibrary library(stickRoot());
    ASSERT_TRUE(library.isValid()) << qPrintable(library.error());
    EXPECT_EQ((QStringList{QStringLiteral("/Contents/Same/Same/x.mp3|x.mp3"),
                      QStringLiteral("/Contents/Same/Same/x (2).mp3|x (2).mp3")}),
            library.rows(QStringLiteral(
                    "SELECT path, fileName FROM content ORDER BY content_id")));
}

TEST_F(DeviceLibraryPlusExporterTest, SkipsMissingFile) {
    TrackPointer pTrack = makeTrack(m_computer.filePath(QStringLiteral("gone.mp3")), 1);
    auto pDb = newDb();
    ASSERT_TRUE(pDb);
    DeviceLibraryPlusExporter exporter(pDb.get(), options());
    ASSERT_TRUE(exporter.prepare(nullptr));
    DlpExportTrack item;
    item.pTrack = pTrack;
    const DlpTrackResult result = exporter.exportTrack(item);
    EXPECT_FALSE(result.contentId);
    EXPECT_EQ(QStringLiteral("The track file is missing."), result.skipReason);
    EXPECT_TRUE(pDb->listContents().isEmpty());
}

TEST_F(DeviceLibraryPlusExporterTest, SkipsUnsupportedType) {
    const DlpExportTrack item = oggItem(toComputer(kOggFile), 1);
    auto pDb = newDb();
    ASSERT_TRUE(pDb);
    DeviceLibraryPlusExporter exporter(pDb.get(), options());
    ASSERT_TRUE(exporter.prepare(nullptr));
    const DlpTrackResult result = exporter.exportTrack(item);
    EXPECT_FALSE(result.contentId);
    EXPECT_EQ(QStringLiteral("Players cannot play this file type."), result.skipReason);
    EXPECT_TRUE(filesBelow(stickRoot()).isEmpty());
}

TEST_F(DeviceLibraryPlusExporterTest, SkipsTrackWithoutWaveform) {
    TrackPointer pTrack = makeTrack(toComputer(QStringLiteral("artist.mp3")), 1);
    auto pDb = newDb();
    ASSERT_TRUE(pDb);
    DeviceLibraryPlusExporter exporter(pDb.get(), options());
    ASSERT_TRUE(exporter.prepare(nullptr));
    DlpExportTrack item = itemFor(pTrack, makeCover(), mixxx::cache_key_t(9));
    item.pWaveform.clear();
    const DlpTrackResult result = exporter.exportTrack(item);
    EXPECT_FALSE(result.contentId);
    EXPECT_EQ(QStringLiteral("The track has no waveform. Analyse the track first."),
            result.skipReason);
    // Nothing is copied or written for it.
    EXPECT_TRUE(filesBelow(stickRoot()).isEmpty());
    EXPECT_TRUE(pDb->listContents().isEmpty());
}

TEST_F(DeviceLibraryPlusExporterTest, PrepareRefusesInvalidMasterDbId) {
    auto pDb = newDb();
    ASSERT_TRUE(pDb);
    for (const qint64 masterDbId : {qint64(0), qint64(-5)}) {
        DeviceLibraryPlusExporter::Options invalid = options();
        invalid.masterDbId = masterDbId;
        DeviceLibraryPlusExporter exporter(pDb.get(), invalid);
        QString error;
        EXPECT_FALSE(exporter.prepare(&error)) << masterDbId;
        EXPECT_EQ(QStringLiteral("The library of Mix Two has no valid ID."), error);
        EXPECT_FALSE(exporter.prepare(nullptr));
    }
}

TEST_F(DeviceLibraryPlusExporterTest, PrepareRefusesBadStickRoot) {
    auto pDb = newDb();
    ASSERT_TRUE(pDb);
    const QString file = toComputer(QStringLiteral("artist.mp3"));
    const QStringList badRoots = {QString(),
            QStringLiteral("   "),
            QStringLiteral("relative/stick"),
            m_computer.filePath(QStringLiteral("missing")),
            // A file is not a folder.
            file};
    for (const QString& root : badRoots) {
        DeviceLibraryPlusExporter::Options invalid = options();
        invalid.stickRoot = root;
        DeviceLibraryPlusExporter exporter(pDb.get(), invalid);
        QString error;
        EXPECT_FALSE(exporter.prepare(&error)) << qPrintable(root);
        EXPECT_EQ(QStringLiteral("The stick folder does not exist."), error) << qPrintable(root);
    }
    // The valid options still work with the same database.
    DeviceLibraryPlusExporter exporter(pDb.get(), options());
    EXPECT_TRUE(exporter.prepare(nullptr));
}

TEST_F(DeviceLibraryPlusExporterTest, SkipsTrackWithoutValidId) {
    const QString source = toComputer(QStringLiteral("artist.mp3"));
    TrackPointer pTrack = makeTrack(source, 1);
    // The same file, but without an ID.
    TrackPointer pNoId = Track::newDummy(source, TrackId());
    ASSERT_FALSE(pNoId->getId().isValid());
    auto pDb = newDb();
    ASSERT_TRUE(pDb);
    DeviceLibraryPlusExporter exporter(pDb.get(), options());
    ASSERT_TRUE(exporter.prepare(nullptr));
    DlpExportTrack item = itemFor(pTrack);
    item.pTrack = pNoId;
    const DlpTrackResult result = exporter.exportTrack(item);
    EXPECT_FALSE(result.contentId);
    EXPECT_EQ(QStringLiteral("The track is not in the Mix Two library."), result.skipReason);
    EXPECT_TRUE(filesBelow(stickRoot()).isEmpty());
    EXPECT_TRUE(pDb->listContents().isEmpty());
}

TEST_F(DeviceLibraryPlusExporterTest, SkipsLargeFileOnFat32) {
    TrackPointer pTrack = makeTrack(toComputer(QStringLiteral("artist.mp3")), 1);
    ASSERT_GT(QFileInfo(pTrack->getLocation()).size(), 1000);
    auto pDb = newDb();
    ASSERT_TRUE(pDb);
    DeviceLibraryPlusExporter::Options fat32 = options();
    fat32.maxFileSize = 1000;
    DeviceLibraryPlusExporter exporter(pDb.get(), fat32);
    ASSERT_TRUE(exporter.prepare(nullptr));
    const DlpTrackResult result = exporter.exportTrack(itemFor(pTrack));
    EXPECT_FALSE(result.contentId);
    EXPECT_EQ(QStringLiteral("The file is larger than 4 GB. FAT32 cannot hold it."),
            result.skipReason);
    EXPECT_TRUE(filesBelow(stickRoot()).isEmpty());
}

TEST_F(DeviceLibraryPlusExporterTest, PlaylistsAndCrates) {
    TrackPointer pA = makeTrack(toComputer(QStringLiteral("artist.mp3"), "a"), 1);
    TrackPointer pB = makeTrack(toComputer(QStringLiteral("all.mp3"), "b"), 2);
    const DlpExportTrack skippedItem = oggItem(toComputer(kOggFile, "c"), 3);
    const TrackId a = pA->getId();
    const TrackId b = pB->getId();
    const TrackId skipped = skippedItem.pTrack->getId();
    QList<DlpTrackResult> results;
    {
        auto pDb = newDb();
        ASSERT_TRUE(pDb);
        DeviceLibraryPlusExporter exporter(pDb.get(), options());
        QString error;
        ASSERT_TRUE(exporter.prepare(&error)) << qPrintable(error);
        results.append(exporter.exportTrack(itemFor(pA)));
        results.append(exporter.exportTrack(itemFor(pB)));
        results.append(exporter.exportTrack(skippedItem));
        ASSERT_TRUE(exporter.exportPlaylists(
                {DlpExportPlaylist{QStringLiteral("Warm Up"), {b, skipped, a}},
                        DlpExportPlaylist{QStringLiteral("Peak"), {a}}},
                {DlpExportPlaylist{QStringLiteral("House"), {a, b}},
                        DlpExportPlaylist{QStringLiteral("Only Skipped"), {skipped}}},
                &error))
                << qPrintable(error);
        EXPECT_EQ(4, exporter.playlistCount());
        ASSERT_TRUE(exporter.finish(&error)) << qPrintable(error);
    }
    ASSERT_TRUE(results[0].contentId);
    ASSERT_TRUE(results[1].contentId);
    ASSERT_FALSE(results[2].contentId);
    const QString idA = QString::number(*results[0].contentId);
    const QString idB = QString::number(*results[1].contentId);

    const QString playlistsSql = QStringLiteral(
            "SELECT p.name, p.attribute, coalesce(f.name, '-'), "
            "coalesce((SELECT group_concat(content_id, ',') FROM "
            "(SELECT content_id FROM playlist_content pc WHERE pc.playlist_id = p.playlist_id "
            "ORDER BY pc.sequenceNo)), '') "
            "FROM playlist p LEFT JOIN playlist f ON f.playlist_id = p.playlist_id_parent "
            "ORDER BY p.playlist_id_parent, p.sequenceNo");
    const QStringList expected{
            QStringLiteral("Warm Up|0|-|%1,%2").arg(idB, idA),
            QStringLiteral("Peak|0|-|%1").arg(idA),
            QStringLiteral("Crates|1|-|"),
            QStringLiteral("House|0|Crates|%1,%2").arg(idA, idB),
            QStringLiteral("Only Skipped|0|Crates|"),
    };
    {
        StickLibrary library(stickRoot());
        ASSERT_TRUE(library.isValid()) << qPrintable(library.error());
        EXPECT_EQ(expected, library.rows(playlistsSql));
    }

    // A second export with the same playlist name replaces its tracks.
    {
        auto pDb = openDb();
        ASSERT_TRUE(pDb);
        DeviceLibraryPlusExporter exporter(pDb.get(), options());
        QString error;
        ASSERT_TRUE(exporter.prepare(&error)) << qPrintable(error);
        ASSERT_TRUE(exporter.exportTrack(itemFor(pA)).contentId);
        ASSERT_TRUE(exporter.exportPlaylists(
                {DlpExportPlaylist{QStringLiteral("Warm Up"), {a}}},
                {DlpExportPlaylist{QStringLiteral("House"), {a}}},
                &error))
                << qPrintable(error);
        EXPECT_EQ(2, exporter.playlistCount());
        ASSERT_TRUE(exporter.finish(&error)) << qPrintable(error);
    }
    QStringList replaced = expected;
    replaced[0] = QStringLiteral("Warm Up|0|-|%1").arg(idA);
    replaced[3] = QStringLiteral("House|0|Crates|%1").arg(idA);
    StickLibrary library(stickRoot());
    ASSERT_TRUE(library.isValid()) << qPrintable(library.error());
    EXPECT_EQ(replaced, library.rows(playlistsSql));
}

TEST_F(DeviceLibraryPlusExporterTest, UnrelatedRekordboxRowWithSameIdStaysUntouched) {
    // A library that another program wrote: a track at the path that the
    // export would use, an artist with the name of the export's artist, and a
    // playlist. The row has the Mix Two track ID as its masterContentId, but
    // no file of the track's size is at its path: it is another track, and
    // the export does not change it.
    {
        auto pDb = newDb(kOtherMasterDbId);
        ASSERT_TRUE(pDb);
        DlpContent other;
        other.title = QStringLiteral("Other Title");
        other.artistId = pDb->findOrAddArtist(QStringLiteral("Shared Artist"));
        other.path = QStringLiteral("/Contents/Shared Artist/UnknownAlbum/artist.mp3");
        other.fileName = QStringLiteral("artist.mp3");
        other.fileSize = 5;
        other.fileType = 1;
        other.masterDbId = kOtherMasterDbId;
        other.masterContentId = 7;
        other.analysisDataFilePath = QStringLiteral("/PIONEER/USBANLZ/P001/00000001/ANLZ0000.DAT");
        const qint64 otherId = pDb->addContent(other);
        ASSERT_EQ(1, otherId);
        const qint64 playlistId = pDb->addPlaylist(QStringLiteral("Theirs"), 0, false);
        pDb->setPlaylistContents(playlistId, {otherId});
        QString error;
        ASSERT_TRUE(pDb->save(&error)) << qPrintable(error);
    }
    const QString source = toComputer(QStringLiteral("artist.mp3"));
    // The same Mix Two track ID as the other row's masterContentId.
    TrackPointer pTrack = makeTrack(source, 7);
    pTrack->setArtist(QStringLiteral("Shared Artist"));
    pTrack->setAlbum(QString());

    const QString contentSql = QStringLiteral("SELECT * FROM content WHERE content_id = 1");
    const QString artistSql = QStringLiteral("SELECT * FROM artist ORDER BY artist_id");
    const QString playlistSql = QStringLiteral(
            "SELECT p.*, (SELECT group_concat(content_id) FROM playlist_content pc "
            "WHERE pc.playlist_id = p.playlist_id) FROM playlist p WHERE name = 'Theirs'");
    QString contentBefore, artistBefore, playlistBefore;
    {
        StickLibrary library(stickRoot());
        ASSERT_TRUE(library.isValid()) << qPrintable(library.error());
        contentBefore = library.value(contentSql);
        artistBefore = library.value(artistSql);
        playlistBefore = library.value(playlistSql);
    }
    ASSERT_FALSE(contentBefore.isEmpty());

    QList<DlpTrackResult> results;
    {
        auto pDb = openDb();
        ASSERT_TRUE(pDb);
        results = exportAndFinish(pDb.get(),
                {itemFor(pTrack)},
                {DlpExportPlaylist{QStringLiteral("Ours"), {pTrack->getId()}}});
    }
    ASSERT_TRUE(results[0].contentId) << qPrintable(results[0].skipReason);
    EXPECT_NE(1, *results[0].contentId);

    StickLibrary library(stickRoot());
    ASSERT_TRUE(library.isValid()) << qPrintable(library.error());
    EXPECT_EQ(contentBefore, library.value(contentSql));
    EXPECT_EQ(playlistBefore, library.value(playlistSql));
    // The export uses the artist row and adds no second one.
    EXPECT_EQ(QStringList{artistBefore}, library.rows(artistSql));
    EXPECT_EQ(QStringLiteral("2"), library.value(QStringLiteral("SELECT count(*) FROM content")));
    EXPECT_EQ(QStringLiteral("2"),
            library.value(QStringLiteral("SELECT numberOfContents FROM property")));
    // The path of the other row is taken: the new file gets a number.
    EXPECT_EQ(QStringLiteral("/Contents/Shared Artist/UnknownAlbum/artist (2).mp3|NULL"),
            library.value(QStringLiteral(
                    "SELECT path, album_id FROM content WHERE content_id = %1")
                            .arg(*results[0].contentId)));
}

TEST_F(DeviceLibraryPlusExporterTest, SharedImageIsNotRewritten) {
    TrackPointer pA = makeTrack(toComputer(QStringLiteral("artist.mp3"), "a"), 1);
    TrackPointer pB = makeTrack(toComputer(QStringLiteral("all.mp3"), "b"), 2);
    {
        auto pDb = newDb();
        ASSERT_TRUE(pDb);
        const auto results = exportAndFinish(pDb.get(),
                {itemFor(pA, makeCover(), mixxx::cache_key_t(5)),
                        itemFor(pB, makeCover(), mixxx::cache_key_t(5))});
        ASSERT_TRUE(results[0].contentId) << qPrintable(results[0].skipReason);
        ASSERT_TRUE(results[1].contentId) << qPrintable(results[1].skipReason);
    }
    const QString smallFile = onStick(QStringLiteral("/PIONEER/Artwork/00001/b1.jpg"));
    const QString mediumFile = onStick(QStringLiteral("/PIONEER/Artwork/00001/b1_m.jpg"));
    const QByteArray smallBefore = readFile(smallFile);
    const QByteArray mediumBefore = readFile(mediumFile);
    ASSERT_FALSE(smallBefore.isEmpty());
    ASSERT_FALSE(mediumBefore.isEmpty());

    // Track A gets another cover. Track B also uses b1, so b1 stays.
    QImage green(100, 100, QImage::Format_RGB32);
    green.fill(Qt::green);
    {
        auto pDb = openDb();
        ASSERT_TRUE(pDb);
        const auto results =
                exportAndFinish(pDb.get(), {itemFor(pA, green, mixxx::cache_key_t(6))});
        ASSERT_TRUE(results[0].contentId) << qPrintable(results[0].skipReason);
    }
    EXPECT_EQ(smallBefore, readFile(smallFile));
    EXPECT_EQ(mediumBefore, readFile(mediumFile));
    const QImage newCover(onStick(QStringLiteral("/PIONEER/Artwork/00001/b2.jpg")));
    ASSERT_FALSE(newCover.isNull());
    EXPECT_GT(qGreen(newCover.pixel(40, 40)), 200);
    EXPECT_LT(qRed(newCover.pixel(40, 40)), 60);

    StickLibrary library(stickRoot());
    ASSERT_TRUE(library.isValid()) << qPrintable(library.error());
    EXPECT_EQ((QStringList{QStringLiteral("1|/PIONEER/Artwork/00001/b1.jpg"),
                      QStringLiteral("2|/PIONEER/Artwork/00001/b2.jpg")}),
            library.rows(QStringLiteral("SELECT image_id, path FROM image ORDER BY image_id")));
    EXPECT_EQ((QStringList{QStringLiteral("1|2"), QStringLiteral("2|1")}),
            library.rows(QStringLiteral(
                    "SELECT masterContentId, image_id FROM content ORDER BY masterContentId")));
}

// AI-generated (Claude Code). Start of AI-generated text.
TEST_F(DeviceLibraryPlusExporterTest, RekordboxTrackOfFileOnStickIsUpdatedInPlace) {
    // Ruling R8. The file is on the stick, and rekordbox has a row for it;
    // the row has the path in other letter case. The export updates that row
    // with Mix Two's data and adds no second row.
    copyTestFile(QStringLiteral("artist.mp3"), onStick(QStringLiteral("/Music/Set")), "own.mp3");
    {
        auto pDb = newDb(kOtherMasterDbId);
        ASSERT_TRUE(pDb);
        DlpContent other;
        other.title = QStringLiteral("Their Title");
        other.path = QStringLiteral("/MUSIC/set/Own.mp3");
        other.fileName = QStringLiteral("Own.mp3");
        other.fileType = 1;
        other.masterDbId = kOtherMasterDbId;
        other.masterContentId = 3;
        other.analysisDataFilePath = QStringLiteral("/PIONEER/USBANLZ/P001/00000001/ANLZ0000.DAT");
        ASSERT_EQ(1, pDb->addContent(other));
        const qint64 theirs = pDb->addPlaylist(QStringLiteral("Theirs"), 0, false);
        pDb->setPlaylistContents(theirs, {1});
        pDb->updateContentCount();
        QString error;
        ASSERT_TRUE(pDb->save(&error)) << qPrintable(error);
    }

    TrackPointer pTrack = makeTrack(onStick(QStringLiteral("/Music/Set/own.mp3")), 8);
    pTrack->setTitle(QStringLiteral("Mix Two Title"));
    QList<DlpTrackResult> results;
    {
        auto pDb = openDb();
        ASSERT_TRUE(pDb);
        results = exportAndFinish(pDb.get(),
                {itemFor(pTrack, makeCover(), mixxx::cache_key_t(4))},
                {DlpExportPlaylist{QStringLiteral("Ours"), {pTrack->getId()}}});
    }
    ASSERT_EQ(1, results.size());
    ASSERT_TRUE(results[0].contentId) << qPrintable(results[0].skipReason);
    EXPECT_EQ(1, *results[0].contentId);
    EXPECT_TRUE(results[0].warnings.isEmpty()) << qPrintable(results[0].warnings.join('\n'));
    // The file stays where it is: no copy in /Contents.
    EXPECT_FALSE(QFileInfo::exists(onStick(QStringLiteral("/Contents"))));

    StickLibrary library(stickRoot());
    ASSERT_TRUE(library.isValid()) << qPrintable(library.error());
    EXPECT_EQ(QStringLiteral("1"), library.value(QStringLiteral("SELECT count(*) FROM content")));
    // Mix Two's data is in the row; the path keeps the letter case of the row.
    EXPECT_EQ(QStringLiteral("1|Mix Two Title|/MUSIC/set/Own.mp3"),
            library.value(QStringLiteral("SELECT content_id, title, path FROM content")));
    const QString anlzPath =
            library.value(QStringLiteral("SELECT analysisDataFilePath FROM content"));
    EXPECT_TRUE(kAnlzPathPattern.match(anlzPath).hasMatch()) << qPrintable(anlzPath);
    EXPECT_TRUE(QFileInfo::exists(onStick(anlzPath)));
    EXPECT_EQ(QStringLiteral("1"), library.value(QStringLiteral("SELECT count(*) FROM image")));
    // Both playlists hold the one row.
    EXPECT_EQ((QStringList{QStringLiteral("Theirs|1"), QStringLiteral("Ours|1")}),
            library.rows(QStringLiteral(
                    "SELECT p.name, group_concat(pc.content_id) FROM playlist p "
                    "JOIN playlist_content pc ON p.playlist_id = pc.playlist_id "
                    "GROUP BY p.playlist_id ORDER BY p.playlist_id")));
}

TEST_F(DeviceLibraryPlusExporterTest, RekordboxPlaylistWithSameNameIsReused) {
    // rekordbox wrote the playlist "Set" and the crate "K" in the folder
    // "Crates", each with one rekordbox track. Mix Two exports a playlist and
    // a crate with the same names.
    qint64 setId = 0;
    qint64 cratesId = 0;
    qint64 crateId = 0;
    {
        auto pDb = newDb(kOtherMasterDbId);
        ASSERT_TRUE(pDb);
        DlpContent other;
        other.title = QStringLiteral("Their Title");
        other.path = QStringLiteral("/Contents/Their Artist/Their Album/theirs.mp3");
        other.fileName = QStringLiteral("theirs.mp3");
        other.fileSize = 5;
        other.fileType = 1;
        other.masterDbId = kOtherMasterDbId;
        other.masterContentId = 7;
        const qint64 otherId = pDb->addContent(other);
        ASSERT_EQ(1, otherId);
        setId = pDb->addPlaylist(QStringLiteral("Set"), 0, false);
        pDb->setPlaylistContents(setId, {otherId});
        cratesId = pDb->addPlaylist(QStringLiteral("Crates"), 0, true);
        crateId = pDb->addPlaylist(QStringLiteral("K"), cratesId, false);
        pDb->setPlaylistContents(crateId, {otherId});
        pDb->updateContentCount();
        QString error;
        ASSERT_TRUE(pDb->save(&error)) << qPrintable(error);
    }
    TrackPointer pTrack = makeTrack(toComputer(QStringLiteral("artist.mp3")), 1);
    QList<DlpTrackResult> results;
    {
        auto pDb = openDb();
        ASSERT_TRUE(pDb);
        results = exportAndFinish(pDb.get(),
                {itemFor(pTrack)},
                {DlpExportPlaylist{QStringLiteral("Set"), {pTrack->getId()}}},
                {DlpExportPlaylist{QStringLiteral("K"), {pTrack->getId()}}});
    }
    ASSERT_TRUE(results[0].contentId) << qPrintable(results[0].skipReason);
    const QString ownId = QString::number(*results[0].contentId);
    StickLibrary library(stickRoot());
    ASSERT_TRUE(library.isValid()) << qPrintable(library.error());
    // One playlist of each name, with the IDs that rekordbox gave them, and
    // the track list of the export.
    EXPECT_EQ((QStringList{QStringLiteral("%1|Set|0|%2").arg(setId).arg(ownId),
                      QStringLiteral("%1|Crates|%2|NULL").arg(cratesId).arg(0),
                      QStringLiteral("%1|K|%2|%3").arg(crateId).arg(cratesId).arg(ownId)}),
            library.rows(QStringLiteral(
                    "SELECT p.playlist_id, p.name, p.playlist_id_parent, "
                    "(SELECT group_concat(content_id) FROM playlist_content pc "
                    "WHERE pc.playlist_id = p.playlist_id) FROM playlist p "
                    "ORDER BY p.playlist_id")));
}
// End of AI-generated text.

TEST_F(DeviceLibraryPlusExporterTest, NoFinishLeavesLibraryUnchanged) {
    saveEmptyLibrary();
    const QString libraryFile = QDir(stickRoot()).filePath(DeviceLibraryPlusDb::kRelativePath);
    const QByteArray before = readFile(libraryFile);
    ASSERT_FALSE(before.isEmpty());
    TrackPointer pTrack = makeTrack(toComputer(QStringLiteral("artist.mp3")), 1);
    {
        auto pDb = openDb();
        ASSERT_TRUE(pDb);
        DeviceLibraryPlusExporter exporter(pDb.get(), options());
        QString error;
        ASSERT_TRUE(exporter.prepare(&error)) << qPrintable(error);
        ASSERT_TRUE(exporter.exportTrack(itemFor(pTrack)).contentId);
        ASSERT_TRUE(exporter.exportPlaylists(
                {DlpExportPlaylist{QStringLiteral("P"), {pTrack->getId()}}}, {}, &error));
    }
    EXPECT_EQ(before, readFile(libraryFile));
}

TEST_F(DeviceLibraryPlusExporterTest, WritesSampleStick) {
    // For the independent checks (tools/dlp_verify.py and tools/anlz_verify.py):
    //   MIXTWO_DLP_EXPORT_SAMPLE_DIR=<empty folder> mixxx-test
    //       --gtest_filter=DeviceLibraryPlusExporterTest.WritesSampleStick
    // The folder must be empty or missing; the test writes nothing to a
    // folder that has files.
    const QString sampleDir = qEnvironmentVariable("MIXTWO_DLP_EXPORT_SAMPLE_DIR");
    if (sampleDir.isEmpty()) {
        GTEST_SKIP() << "MIXTWO_DLP_EXPORT_SAMPLE_DIR is not set.";
    }
    ASSERT_TRUE(QDir().mkpath(sampleDir));
    if (!QDir(sampleDir).isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden)) {
        FAIL() << "MIXTWO_DLP_EXPORT_SAMPLE_DIR is not empty. Use an empty folder.";
    }

    TrackPointer pMp3 = makeTrack(toComputer(kCoverMp3File, "a"), 1);
    TrackPointer pWav = makeTrack(toComputer(kWavFile, "b"), 2);
    TrackPointer pFlac = makeTrack(toComputer(kFlacFile, "c"), 3);
    pMp3->setKey(track::io::key::A_MINOR, track::io::key::USER);
    pWav->setKey(track::io::key::C_MAJOR, track::io::key::USER);
    // Rows in the artist, album and genre tables for the checks; the FLAC
    // track keeps its empty names (UnknownArtist, UnknownAlbum).
    for (const TrackPointer& pTrack : {pMp3, pWav}) {
        pTrack->setArtist(QStringLiteral("Sample Artist"));
        pTrack->setAlbum(QStringLiteral("Sample Album"));
        pTrack->setComposer(QStringLiteral("Sample Composer"));
        pTrack->updateGenre(QStringLiteral("House"));
    }
    pWav->setAlbumArtist(QStringLiteral("Sample Album Artist"));
    QString error;
    auto pDb = DeviceLibraryPlusDb::create(sampleDir, kSampleDeviceName, kMasterDbId, &error);
    ASSERT_TRUE(pDb) << qPrintable(error);
    DeviceLibraryPlusExporter::Options sampleOptions = options();
    sampleOptions.stickRoot = sampleDir;
    DeviceLibraryPlusExporter exporter(pDb.get(), sampleOptions);
    ASSERT_TRUE(exporter.prepare(&error)) << qPrintable(error);
    const QImage cover = makeCover();
    for (const DlpExportTrack& item :
            {itemFor(pMp3, cover, mixxx::cache_key_t(1)),
                    itemFor(pWav, cover, mixxx::cache_key_t(1)),
                    itemFor(pFlac)}) {
        const DlpTrackResult result = exporter.exportTrack(item);
        ASSERT_TRUE(result.contentId) << qPrintable(result.skipReason);
    }
    ASSERT_TRUE(exporter.exportPlaylists(
            {DlpExportPlaylist{QStringLiteral("Sample Playlist"),
                    {pFlac->getId(), pMp3->getId()}}},
            {DlpExportPlaylist{QStringLiteral("Sample Crate"),
                    {pMp3->getId(), pWav->getId(), pFlac->getId()}}},
            &error))
            << qPrintable(error);
    ASSERT_TRUE(exporter.finish(&error)) << qPrintable(error);
    pDb.reset();

    // A second export to the same stick (Ruling R17), so that the checks also
    // see a re-export: another hot cue for the MP3 track (a new ANLZ folder;
    // the old one is removed), and another cover for the WAV track (a new
    // image; b1 stays, because the MP3 track uses it). The WAV and FLAC
    // tracks keep their ANLZ folders, because the bytes did not change.
    pMp3->createAndAddCue(mixxx::CueType::HotCue, 1, audio::FramePos(20000), audio::kInvalidFramePos);
    QImage green(120, 90, QImage::Format_RGB32);
    green.fill(Qt::green);
    auto pAgain = DeviceLibraryPlusDb::open(sampleDir, &error);
    ASSERT_TRUE(pAgain) << qPrintable(error);
    DeviceLibraryPlusExporter again(pAgain.get(), sampleOptions);
    ASSERT_TRUE(again.prepare(&error)) << qPrintable(error);
    for (const DlpExportTrack& item :
            {itemFor(pMp3, cover, mixxx::cache_key_t(1)),
                    itemFor(pWav, green, mixxx::cache_key_t(2)),
                    itemFor(pFlac)}) {
        const DlpTrackResult result = again.exportTrack(item);
        ASSERT_TRUE(result.contentId) << qPrintable(result.skipReason);
    }
    ASSERT_TRUE(again.exportPlaylists(
            {DlpExportPlaylist{QStringLiteral("Sample Playlist"),
                    {pFlac->getId(), pMp3->getId(), pWav->getId()}}},
            {DlpExportPlaylist{QStringLiteral("Sample Crate"),
                    {pMp3->getId(), pWav->getId(), pFlac->getId()}}},
            &error))
            << qPrintable(error);
    ASSERT_TRUE(again.finish(&error)) << qPrintable(error);
    const QList<DlpContentRef> rows = pAgain->listContents();
    ASSERT_EQ(3, rows.size());
    QStringList anlzFolders;
    for (const DlpContentRef& row : rows) {
        EXPECT_TRUE(QFileInfo(sampleDir + row.analysisDataFilePath).isFile());
        anlzFolders.append(QFileInfo(row.analysisDataFilePath).path());
    }
    // Only the folders of the rows are left.
    QStringList onDisk;
    QDirIterator it(sampleDir + QStringLiteral("/PIONEER/USBANLZ"),
            QDir::Files,
            QDirIterator::Subdirectories);
    while (it.hasNext()) {
        onDisk.append(QStringLiteral("/PIONEER/USBANLZ/") +
                QFileInfo(QDir(sampleDir + QStringLiteral("/PIONEER/USBANLZ"))
                                  .relativeFilePath(it.next()))
                        .path());
    }
    onDisk.removeDuplicates();
    onDisk.sort();
    anlzFolders.sort();
    EXPECT_EQ(anlzFolders, onDisk);
    EXPECT_EQ((QStringList{QStringLiteral("00001/b1.jpg"),
                      QStringLiteral("00001/b1_m.jpg"),
                      QStringLiteral("00001/b2.jpg"),
                      QStringLiteral("00001/b2_m.jpg")}),
            filesBelow(sampleDir + QStringLiteral("/PIONEER/Artwork")));
}

// AI-generated (Claude Code). Start of AI-generated text.
namespace {

/// A stick that the exporter wrote: the tracks A, B and C (Mix Two track IDs 1,
/// 2 and 3), the playlist P1 with A and B, the playlist P2 with B and C, and
/// the crate K with C. A has the cover 1 and C has the cover 2.
class DeviceLibraryPlusDeleteTest : public DeviceLibraryPlusExporterTest {
  protected:
    void SetUp() override {
        DeviceLibraryPlusExporterTest::SetUp();
        TrackPointer pA = makeTrack(toComputer(QStringLiteral("artist.mp3"), "a"), 1);
        pA->setArtist(QStringLiteral("Artist A"));
        pA->setAlbum(QStringLiteral("Album A"));
        TrackPointer pB = makeTrack(toComputer(QStringLiteral("all.mp3"), "b"), 2);
        pB->setArtist(QStringLiteral("Artist B"));
        pB->setAlbum(QStringLiteral("Album B"));
        TrackPointer pC = makeTrack(toComputer(kCoverMp3File, "c"), 3);
        pC->setArtist(QStringLiteral("Artist C"));
        pC->setAlbum(QStringLiteral("Album C"));
        auto pDb = newDb();
        ASSERT_TRUE(pDb);
        const auto results = exportAndFinish(pDb.get(),
                {itemFor(pA, makeCover(), mixxx::cache_key_t(1)),
                        itemFor(pB),
                        itemFor(pC, makeCover(), mixxx::cache_key_t(2))},
                {DlpExportPlaylist{QStringLiteral("P1"), {pA->getId(), pB->getId()}},
                        DlpExportPlaylist{QStringLiteral("P2"), {pB->getId(), pC->getId()}}},
                {DlpExportPlaylist{QStringLiteral("K"), {pC->getId()}}});
        for (const DlpTrackResult& result : results) {
            ASSERT_TRUE(result.contentId) << qPrintable(result.skipReason);
        }
    }

    /// The row of the track with the Mix Two track ID.
    DlpContentRef rowOf(qint64 masterContentId) const {
        auto pDb = openDb();
        EXPECT_TRUE(pDb);
        if (pDb) {
            for (const DlpContentRef& row : pDb->listContents()) {
                if (row.masterContentId == masterContentId) {
                    return row;
                }
            }
        }
        return DlpContentRef();
    }

    qint64 playlistId(const QString& name) const {
        auto pDb = openDb();
        EXPECT_TRUE(pDb);
        if (pDb) {
            for (const DlpPlaylistRef& playlist : pDb->listPlaylists()) {
                if (playlist.name == name) {
                    return playlist.id;
                }
            }
        }
        return -1;
    }

    bool fileExists(const QString& stickPath) const {
        return QFileInfo(onStick(stickPath)).isFile();
    }

    bool anlzFilesExist(const DlpContentRef& row) const {
        const QString folder = QFileInfo(onStick(row.analysisDataFilePath)).path();
        return QFileInfo(folder + QStringLiteral("/ANLZ0000.DAT")).isFile() ||
                QFileInfo(folder + QStringLiteral("/ANLZ0000.EXT")).isFile() ||
                QFileInfo(folder + QStringLiteral("/ANLZ0000.2EX")).isFile() ||
                QFileInfo(folder).isDir();
    }

    QByteArray libraryBytes() const {
        return readFile(onStick(QStringLiteral("/") + DeviceLibraryPlusDb::kRelativePath));
    }

    static QString playlistsSql() {
        return QStringLiteral(
                "SELECT p.playlist_id, p.name, p.attribute, p.playlist_id_parent, "
                "p.sequenceNo, (SELECT group_concat(content_id) FROM playlist_content pc "
                "WHERE pc.playlist_id = p.playlist_id) FROM playlist p ORDER BY p.playlist_id");
    }
};

} // namespace

TEST_F(DeviceLibraryPlusDeleteTest, KeepsTrackOfAnotherPlaylist) {
    const DlpContentRef rowA = rowOf(1);
    const DlpContentRef rowB = rowOf(2);
    const DlpContentRef rowC = rowOf(3);
    ASSERT_NE(0, rowA.id);
    ASSERT_NE(0, rowB.id);
    ASSERT_NE(0, rowC.id);
    ASSERT_TRUE(fileExists(rowA.path));
    ASSERT_TRUE(anlzFilesExist(rowA));
    const QString sharedSql = QStringLiteral(
            "SELECT 'artist', artist_id, name FROM artist UNION ALL "
            "SELECT 'album', album_id, name FROM album UNION ALL "
            "SELECT 'genre', genre_id, name FROM genre UNION ALL "
            "SELECT 'key', key_id, name FROM key");
    QString contentBefore, sharedBefore;
    {
        StickLibrary library(stickRoot());
        ASSERT_TRUE(library.isValid()) << qPrintable(library.error());
        sharedBefore = library.rows(sharedSql).join(QLatin1Char('\n'));
        contentBefore = library.rows(QStringLiteral(
                                                "SELECT * FROM content WHERE masterContentId IN "
                                                "(2, 3) ORDER BY content_id"))
                                .join('\n');
        ASSERT_FALSE(contentBefore.isEmpty());
        EXPECT_EQ(QStringLiteral("1|/PIONEER/Artwork/00001/b1.jpg"),
                library.value(QStringLiteral("SELECT image_id, path FROM image ORDER BY 1")));
    }
    ASSERT_TRUE(fileExists(QStringLiteral("/PIONEER/Artwork/00001/b1.jpg")));
    ASSERT_TRUE(fileExists(QStringLiteral("/PIONEER/Artwork/00001/b2.jpg")));

    const DlpDeletePlaylistResult result =
            dlpDeletePlaylist(stickRoot(), kMasterDbId, playlistId(QStringLiteral("P1")));
    ASSERT_TRUE(result.ok) << qPrintable(result.errorMessage);
    EXPECT_EQ(1, result.playlistsRemoved);
    EXPECT_EQ(1, result.tracksRemoved);
    EXPECT_EQ(1, result.tracksKept);
    EXPECT_TRUE(result.refused.isEmpty());
    EXPECT_TRUE(result.filesNotDeleted.isEmpty()) << qPrintable(result.filesNotDeleted.join('\n'));
    ASSERT_EQ(1, result.removedTracks.size());
    EXPECT_EQ(rowA.path, result.removedTracks[0].stickPath);
    EXPECT_FALSE(result.removedTracks[0].inMixTwoLibrary);

    // A is gone, with its files. B stays: P2 holds it.
    EXPECT_FALSE(fileExists(rowA.path));
    EXPECT_FALSE(QFileInfo(onStick(rowA.analysisDataFilePath)).dir().exists());
    EXPECT_TRUE(fileExists(rowB.path));
    EXPECT_TRUE(anlzFilesExist(rowB));
    EXPECT_TRUE(fileExists(rowC.path));
    // The cover of A has no user now. The cover of C stays.
    EXPECT_FALSE(fileExists(QStringLiteral("/PIONEER/Artwork/00001/b1.jpg")));
    EXPECT_FALSE(fileExists(QStringLiteral("/PIONEER/Artwork/00001/b1_m.jpg")));
    EXPECT_TRUE(fileExists(QStringLiteral("/PIONEER/Artwork/00001/b2.jpg")));
    EXPECT_TRUE(fileExists(QStringLiteral("/PIONEER/Artwork/00001/b2_m.jpg")));

    StickLibrary library(stickRoot());
    ASSERT_TRUE(library.isValid()) << qPrintable(library.error());
    EXPECT_EQ(QStringLiteral("ok"), library.value(QStringLiteral("PRAGMA integrity_check")));
    EXPECT_EQ(QStringLiteral("0"),
            library.value(QStringLiteral("SELECT count(*) FROM content WHERE masterContentId = 1")));
    // The tracks and playlists that the delete did not touch have their rows.
    EXPECT_EQ(contentBefore,
            library.rows(QStringLiteral("SELECT * FROM content WHERE masterContentId IN "
                                        "(2, 3) ORDER BY content_id"))
                    .join('\n'));
    EXPECT_EQ(QStringLiteral("2"), library.value(QStringLiteral("SELECT count(*) FROM content")));
    EXPECT_EQ(QStringLiteral("2"),
            library.value(QStringLiteral("SELECT numberOfContents FROM property")));
    const QString idB = QString::number(rowB.id);
    const QString idC = QString::number(rowC.id);
    // P1 and its entries are gone. P2 and the crate keep their tracks. The
    // later playlists have a sequence number that is one less.
    EXPECT_EQ((QStringList{QStringLiteral("2|P2|0|0|0|%1,%2").arg(idB, idC),
                      QStringLiteral("3|Crates|1|0|1|NULL"),
                      QStringLiteral("4|K|0|3|0|%1").arg(idC)}),
            library.rows(playlistsSql()));
    EXPECT_EQ(QStringLiteral("0"),
            library.value(QStringLiteral(
                    "SELECT count(*) FROM playlist_content WHERE playlist_id = 1")));
    // The shared rows stay: the tracks use the same artists and albums as before.
    EXPECT_EQ(sharedBefore, library.rows(sharedSql).join(QLatin1Char('\n')));
    EXPECT_FALSE(sharedBefore.isEmpty());
    // The library opens again.
    auto pDb = openDb();
    ASSERT_TRUE(pDb);
    EXPECT_EQ(2, pDb->listContents().size());
}

TEST_F(DeviceLibraryPlusDeleteTest, RemovesTrackWhenNoPlaylistHoldsItAnyMore) {
    const DlpContentRef rowB = rowOf(2);
    const DlpContentRef rowC = rowOf(3);
    ASSERT_TRUE(dlpDeletePlaylist(stickRoot(), kMasterDbId, playlistId(QStringLiteral("P1"))).ok);

    // P2 holds B and C. The crate K holds C.
    const DlpDeletePlaylistResult result =
            dlpDeletePlaylist(stickRoot(), kMasterDbId, playlistId(QStringLiteral("P2")));
    ASSERT_TRUE(result.ok) << qPrintable(result.errorMessage);
    EXPECT_EQ(1, result.playlistsRemoved);
    EXPECT_EQ(1, result.tracksRemoved);
    EXPECT_EQ(1, result.tracksKept);
    EXPECT_FALSE(fileExists(rowB.path));
    EXPECT_FALSE(QFileInfo(onStick(rowB.analysisDataFilePath)).dir().exists());
    EXPECT_TRUE(fileExists(rowC.path));
    EXPECT_TRUE(anlzFilesExist(rowC));

    // The crate goes by its path of names. Then C has no playlist.
    const DlpDeletePlaylistResult crate = dlpDeletePlaylistByPath(
            stickRoot(), kMasterDbId, {QStringLiteral("Crates"), QStringLiteral("K")});
    ASSERT_TRUE(crate.ok) << qPrintable(crate.errorMessage);
    EXPECT_EQ(1, crate.tracksRemoved);
    EXPECT_EQ(0, crate.tracksKept);
    EXPECT_FALSE(fileExists(rowC.path));
    EXPECT_FALSE(anlzFilesExist(rowC));
    EXPECT_FALSE(fileExists(QStringLiteral("/PIONEER/Artwork/00001/b2.jpg")));

    StickLibrary library(stickRoot());
    ASSERT_TRUE(library.isValid()) << qPrintable(library.error());
    EXPECT_EQ(QStringLiteral("ok"), library.value(QStringLiteral("PRAGMA integrity_check")));
    EXPECT_EQ(QStringLiteral("0"), library.value(QStringLiteral("SELECT count(*) FROM content")));
    EXPECT_EQ(QStringLiteral("0"),
            library.value(QStringLiteral("SELECT numberOfContents FROM property")));
    // Only the folder Crates is left, and it is empty.
    EXPECT_EQ((QStringList{QStringLiteral("3|Crates|1|0|0|NULL")}), library.rows(playlistsSql()));
}

TEST_F(DeviceLibraryPlusDeleteTest, ByPathFindsPlaylistAtTopLevel) {
    const DlpDeletePlaylistResult result =
            dlpDeletePlaylistByPath(stickRoot(), kMasterDbId, {QStringLiteral("P1")});
    ASSERT_TRUE(result.ok) << qPrintable(result.errorMessage);
    EXPECT_EQ(1, result.tracksRemoved);
    EXPECT_EQ(-1, playlistId(QStringLiteral("P1")));
}

TEST_F(DeviceLibraryPlusDeleteTest, UnknownPlaylistChangesNothing) {
    const QByteArray before = libraryBytes();
    ASSERT_FALSE(before.isEmpty());
    const DlpContentRef rowA = rowOf(1);

    DlpDeletePlaylistResult result = dlpDeletePlaylist(stickRoot(), kMasterDbId, 9999);
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.errorMessage.isEmpty());
    EXPECT_EQ(0, result.playlistsRemoved);
    EXPECT_EQ(0, result.tracksRemoved);

    result = dlpDeletePlaylistByPath(stickRoot(), kMasterDbId, {QStringLiteral("Nope")});
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.errorMessage.isEmpty());
    result = dlpDeletePlaylistByPath(
            stickRoot(), kMasterDbId, {QStringLiteral("Nope"), QStringLiteral("K")});
    EXPECT_FALSE(result.ok);
    // K is in the folder Crates, not at the top level.
    result = dlpDeletePlaylistByPath(stickRoot(), kMasterDbId, {QStringLiteral("K")});
    EXPECT_FALSE(result.ok);
    result = dlpDeletePlaylistByPath(stickRoot(), kMasterDbId, QStringList());
    EXPECT_FALSE(result.ok);

    // A stick without a library, and a library ID that is not valid.
    QTemporaryDir empty;
    ASSERT_TRUE(empty.isValid());
    EXPECT_FALSE(dlpDeletePlaylist(empty.path(), kMasterDbId, 1).ok);
    EXPECT_FALSE(dlpDeletePlaylist(QString(), kMasterDbId, 1).ok);
    EXPECT_FALSE(dlpDeletePlaylist(stickRoot(), 0, playlistId(QStringLiteral("P1"))).ok);

    EXPECT_EQ(before, libraryBytes());
    EXPECT_TRUE(fileExists(rowA.path));
}

TEST_F(DeviceLibraryPlusDeleteTest, FolderIsRefused) {
    const QByteArray before = libraryBytes();
    const DlpContentRef rowC = rowOf(3);
    const DlpDeletePlaylistResult result =
            dlpDeletePlaylist(stickRoot(), kMasterDbId, playlistId(QStringLiteral("Crates")));
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.errorMessage.isEmpty());
    EXPECT_EQ(0, result.playlistsRemoved);
    ASSERT_EQ(1, result.refused.size());
    EXPECT_EQ(QStringLiteral("Crates"), result.refused[0].name);
    EXPECT_FALSE(result.refused[0].reason.isEmpty());
    EXPECT_EQ(before, libraryBytes());
    EXPECT_TRUE(fileExists(rowC.path));

    // The path of a folder gives the same answer. An empty folder is a folder too.
    EXPECT_FALSE(dlpDeletePlaylistByPath(stickRoot(), kMasterDbId, {QStringLiteral("Crates")}).ok);
    {
        auto pDb = openDb();
        ASSERT_TRUE(pDb);
        pDb->addPlaylist(QStringLiteral("Empty"), 0, true);
        QString error;
        ASSERT_TRUE(pDb->save(&error)) << qPrintable(error);
    }
    const QByteArray withEmptyFolder = libraryBytes();
    EXPECT_FALSE(dlpDeletePlaylistByPath(stickRoot(), kMasterDbId, {QStringLiteral("Empty")}).ok);
    EXPECT_EQ(withEmptyFolder, libraryBytes());
}

TEST_F(DeviceLibraryPlusDeleteTest, RemovesRekordboxTracksThatNoOtherListUses) {
    // rekordbox adds two tracks with files and ANLZ files, and the playlist
    // "Theirs" with the first track. P1 gets A and both rekordbox tracks.
    const QString otherAudio = QStringLiteral("/Contents/Their Artist/Their Album/theirs.mp3");
    const QString otherAudio2 = QStringLiteral("/Contents/Their Artist/Their Album/theirs2.mp3");
    const QString otherAnlz = QStringLiteral("/PIONEER/USBANLZ/P0AA/0000AAAA/ANLZ0000.DAT");
    const QString otherAnlz2 = QStringLiteral("/PIONEER/USBANLZ/P0AA/0000BBBB/ANLZ0000.DAT");
    for (const QString& path : {otherAudio, otherAudio2, otherAnlz, otherAnlz2}) {
        ASSERT_TRUE(QDir().mkpath(QFileInfo(onStick(path)).path()));
        QFile file(onStick(path));
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        ASSERT_EQ(5, file.write("other"));
    }
    qint64 firstId = 0;
    qint64 secondId = 0;
    const qint64 p1Id = playlistId(QStringLiteral("P1"));
    const DlpContentRef rowA = rowOf(1);
    {
        auto pDb = openDb();
        ASSERT_TRUE(pDb);
        const auto makeOther = [&pDb](const QString& path, const QString& anlz, int id) {
            DlpContent other;
            other.title = QStringLiteral("Other %1").arg(id);
            other.artistId = pDb->findOrAddArtist(QStringLiteral("Their Artist"));
            other.path = path;
            other.fileName = QFileInfo(path).fileName();
            other.fileSize = 5;
            other.fileType = 1;
            other.masterDbId = kOtherMasterDbId;
            other.masterContentId = id;
            other.analysisDataFilePath = anlz;
            return pDb->addContent(other);
        };
        firstId = makeOther(otherAudio, otherAnlz, 7);
        secondId = makeOther(otherAudio2, otherAnlz2, 8);
        const qint64 theirs = pDb->addPlaylist(QStringLiteral("Theirs"), 0, false);
        pDb->setPlaylistContents(theirs, {firstId});
        pDb->setPlaylistContents(p1Id, {rowA.id, firstId, secondId});
        pDb->updateContentCount();
        QString error;
        ASSERT_TRUE(pDb->save(&error)) << qPrintable(error);
    }
    const QString firstRowSql =
            QStringLiteral("SELECT * FROM content WHERE content_id = %1").arg(firstId);
    QString firstRowBefore;
    {
        StickLibrary library(stickRoot());
        ASSERT_TRUE(library.isValid()) << qPrintable(library.error());
        firstRowBefore = library.value(firstRowSql);
    }
    ASSERT_FALSE(firstRowBefore.isEmpty());

    // The caller knows the tracks of its library: here the rows of Mix Two.
    QList<qint64> askedRows;
    const DlpDeletePlaylistResult result = dlpDeletePlaylist(stickRoot(),
            kMasterDbId,
            p1Id,
            [&askedRows](const DlpRemovedTrack& track) {
                askedRows.append(track.contentId);
                return track.masterDbId == kMasterDbId;
            });
    ASSERT_TRUE(result.ok) << qPrintable(result.errorMessage);
    EXPECT_EQ(1, result.playlistsRemoved);
    // A and the second rekordbox track go. "Theirs" holds the first one.
    EXPECT_EQ(2, result.tracksRemoved);
    EXPECT_EQ(1, result.tracksKept);
    EXPECT_TRUE(result.refused.isEmpty());
    EXPECT_TRUE(result.filesNotDeleted.isEmpty()) << qPrintable(result.filesNotDeleted.join('\n'));

    // The report: each removed track, in the order of the playlist.
    ASSERT_EQ(2, result.removedTracks.size());
    EXPECT_EQ(rowA.id, result.removedTracks[0].contentId);
    EXPECT_EQ(rowA.path, result.removedTracks[0].stickPath);
    EXPECT_FALSE(result.removedTracks[0].title.isEmpty());
    EXPECT_EQ(QStringLiteral("Artist A"), result.removedTracks[0].artist);
    EXPECT_TRUE(result.removedTracks[0].inMixTwoLibrary);
    EXPECT_EQ(secondId, result.removedTracks[1].contentId);
    EXPECT_EQ(otherAudio2, result.removedTracks[1].stickPath);
    EXPECT_EQ(QStringLiteral("Other 8"), result.removedTracks[1].title);
    EXPECT_EQ(QStringLiteral("Their Artist"), result.removedTracks[1].artist);
    EXPECT_EQ(kOtherMasterDbId, result.removedTracks[1].masterDbId);
    EXPECT_EQ(8, result.removedTracks[1].masterContentId);
    EXPECT_FALSE(result.removedTracks[1].inMixTwoLibrary);
    // The check runs only for the removed tracks.
    EXPECT_EQ((QList<qint64>{rowA.id, secondId}), askedRows);

    EXPECT_FALSE(fileExists(rowA.path));
    EXPECT_TRUE(fileExists(otherAudio));
    EXPECT_TRUE(fileExists(otherAnlz));
    EXPECT_FALSE(fileExists(otherAudio2));
    EXPECT_FALSE(fileExists(otherAnlz2));
    StickLibrary library(stickRoot());
    ASSERT_TRUE(library.isValid()) << qPrintable(library.error());
    EXPECT_EQ(QStringLiteral("ok"), library.value(QStringLiteral("PRAGMA integrity_check")));
    EXPECT_EQ(firstRowBefore, library.value(firstRowSql));
    EXPECT_EQ(QString::number(firstId),
            library.value(QStringLiteral("SELECT group_concat(content_id) FROM playlist_content "
                                         "WHERE playlist_id = (SELECT playlist_id FROM playlist "
                                         "WHERE name = 'Theirs')")));
    // B, C and the first rekordbox track.
    EXPECT_EQ(QStringLiteral("3"), library.value(QStringLiteral("SELECT count(*) FROM content")));
    EXPECT_EQ(QStringLiteral("3"),
            library.value(QStringLiteral("SELECT numberOfContents FROM property")));
    EXPECT_EQ(QStringLiteral("0"),
            library.value(QStringLiteral("SELECT count(*) FROM content WHERE content_id = %1")
                            .arg(secondId)));

    // Then "Theirs" goes, and with it the first rekordbox track. Without a
    // check, no track counts as in the library.
    const DlpDeletePlaylistResult theirs =
            dlpDeletePlaylist(stickRoot(), kMasterDbId, playlistId(QStringLiteral("Theirs")));
    ASSERT_TRUE(theirs.ok) << qPrintable(theirs.errorMessage);
    EXPECT_EQ(1, theirs.tracksRemoved);
    ASSERT_EQ(1, theirs.removedTracks.size());
    EXPECT_EQ(otherAudio, theirs.removedTracks[0].stickPath);
    EXPECT_FALSE(theirs.removedTracks[0].inMixTwoLibrary);
    EXPECT_FALSE(fileExists(otherAudio));
    EXPECT_FALSE(fileExists(otherAnlz));
}

// AI-generated (Claude Code). Start of AI-generated text.
TEST_F(DeviceLibraryPlusDeleteTest, BacksUpLibraryBeforeDelete) {
    const QByteArray before = libraryBytes();
    QTemporaryDir backupDir;
    ASSERT_TRUE(backupDir.isValid());

    // A backup folder that is a file: the backup fails, and the stick does
    // not change.
    const QString notAFolder = backupDir.filePath(QStringLiteral("file"));
    {
        QFile file(notAFolder);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        ASSERT_EQ(5, file.write("other"));
    }
    const DlpDeletePlaylistResult refused = dlpDeletePlaylist(
            stickRoot(), kMasterDbId, playlistId(QStringLiteral("P1")), {}, notAFolder);
    EXPECT_FALSE(refused.ok);
    EXPECT_FALSE(refused.errorMessage.isEmpty());
    EXPECT_EQ(before, libraryBytes());

    // A good backup folder: the copy of the library is there before the
    // delete changes the stick.
    const QString backupBase = backupDir.filePath(QStringLiteral("backups"));
    const DlpDeletePlaylistResult result = dlpDeletePlaylist(
            stickRoot(), kMasterDbId, playlistId(QStringLiteral("P1")), {}, backupBase);
    ASSERT_TRUE(result.ok) << qPrintable(result.errorMessage);
    EXPECT_NE(before, libraryBytes());
    QStringList copies;
    QDirIterator it(backupBase,
            {QStringLiteral("exportLibrary.db")},
            QDir::Files,
            QDirIterator::Subdirectories);
    while (it.hasNext()) {
        copies.append(it.next());
    }
    ASSERT_EQ(1, copies.size()) << qPrintable(copies.join('\n'));
    EXPECT_EQ(before, readFile(copies[0]));
}
// End of AI-generated text.

TEST_F(DeviceLibraryPlusDeleteTest, ReportsFileThatCannotBeDeleted) {
    const DlpContentRef rowA = rowOf(1);
    // A folder at the place of the audio file: Mix Two removes no folder.
    ASSERT_TRUE(QFile::remove(onStick(rowA.path)));
    ASSERT_TRUE(QDir().mkpath(onStick(rowA.path)));

    const DlpDeletePlaylistResult result =
            dlpDeletePlaylist(stickRoot(), kMasterDbId, playlistId(QStringLiteral("P1")));
    // The library is saved. The file is reported and is not fatal.
    ASSERT_TRUE(result.ok) << qPrintable(result.errorMessage);
    EXPECT_EQ(1, result.tracksRemoved);
    EXPECT_EQ(QStringList{rowA.path}, result.filesNotDeleted);
    EXPECT_TRUE(QFileInfo(onStick(rowA.path)).isDir());
    EXPECT_FALSE(QFileInfo(onStick(rowA.analysisDataFilePath)).dir().exists());
    EXPECT_EQ(-1, playlistId(QStringLiteral("P1")));
}
// End of AI-generated text.

} // namespace rekordbox
} // namespace mixxx
