#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QPair>
#include <QString>
#include <QTemporaryDir>
#include <QtGlobal>
#include <memory>
#include <optional>

struct sqlite3;

namespace mixxx {
namespace rekordbox {

/// Mix Two: the values of one row of the content table (one track) that Mix
/// Two sets. The other columns of the row have fixed values (spec section 6);
/// addContent() and updateContent() write them. The ID of the track is not in
/// the struct: addContent() gives it.
struct DlpContent {
    QString title;
    /// The IDs of rows in the artist, genre, key, album and image tables. An
    /// empty value is NULL. The IDs are 64-bit: rekordbox writes IDs that are
    /// wider than 31 bits.
    std::optional<qint64> artistId, composerId, albumId, genreId, keyId, imageId;
    /// The ID of one of the 8 rekordbox colours, or 0 for none.
    int colorId = 0;
    int bpmx100 = 0;
    int lengthSeconds = 0;
    /// 0 to 5.
    int rating = 0;
    int releaseYear = 0;
    int playCount = 0;
    QString djComment;
    /// Dates are "yyyy-MM-dd".
    QString dateAdded;
    QString dateCreated;
    /// The path on the stick, from the root of the stick; it starts with '/'.
    QString path;
    QString fileName;
    qint64 fileSize = 0;
    /// The rekordbox code of the file type (spec F13).
    int fileType = 0;
    /// In kbit/s.
    int bitrate = 0;
    int bitDepth = 0;
    int samplingRate = 0;
    /// The ID of the library of the track and the ID of the track in that
    /// library (spec F12).
    qint64 masterDbId = 0;
    qint64 masterContentId = 0;
    /// The path of the ANLZ0000.DAT file on the stick.
    QString analysisDataFilePath;
};

/// Mix Two: what the export needs to know about a track that the library has
/// already: to find the track of Mix Two, to list the files that the library
/// uses, and to keep the cover image of the track.
struct DlpContentRef {
    qint64 id = 0;
    QString path;
    qint64 masterDbId = 0;
    qint64 masterContentId = 0;
    QString analysisDataFilePath;
    std::optional<qint64> imageId;
};

/// Mix Two: rekordbox's Device Library Plus database of a stick
/// (PIONEER/rekordbox/exportLibrary.db): a SQLite database in the SQLCipher 4
/// file format.
///
/// The object works on a plain copy of the database in a temporary folder
/// (mixtwo-dlp-XXXXXX in the temporary folder of the computer; journal mode
/// WAL, 4096 byte pages, 80 reserved bytes on each page, as rekordbox writes
/// it). SQLite does not sync the copy to the disk: save() writes the stick, and
/// the copy is thrown away. It reads the file on the stick only when it is
/// opened. It writes the stick only in save(), and only the one file
/// exportLibrary.db. Destroying the object throws away the changes that
/// save() did not write, and removes the copy.
class DeviceLibraryPlusDb {
  public:
    /// The place of the database file, from the root of the stick.
    static const QString kRelativePath;

    /// Makes a new library in a temporary copy. Nothing is written to the
    /// stick before save(). stickRoot must be an absolute path to a folder that
    /// exists. Returns null if it is not, if the stick already has a library
    /// file, if a WAL file with data is next to the library file, or if it
    /// cannot make the copy; then it sets *pErrorMessage (if it is not null) to
    /// a message for the user.
    static std::unique_ptr<DeviceLibraryPlusDb> create(const QString& stickRoot,
            const QString& deviceName,
            qint64 masterDbId,
            QString* pErrorMessage);

    /// Opens the library of the stick. Never writes to the stick. stickRoot
    /// must be an absolute path to a folder that exists. Returns null and sets
    /// *pErrorMessage (if it is not null) if it is not, if the file cannot be
    /// read or decrypted, if a WAL file with data is next to it (a player or
    /// rekordbox did not finish with the stick), if dbVersion is not '1000',
    /// or if the tables are not those of rekordbox (dlpschema::statements()).
    /// It records the size, the time of the last change and the SHA-256 of the
    /// bytes that it read, for save().
    static std::unique_ptr<DeviceLibraryPlusDb> open(
            const QString& stickRoot, QString* pErrorMessage);

    /// Closes the database and removes the temporary copy.
    ~DeviceLibraryPlusDb();

    DeviceLibraryPlusDb(const DeviceLibraryPlusDb&) = delete;
    DeviceLibraryPlusDb& operator=(const DeviceLibraryPlusDb&) = delete;

    /// Writes the library to the stick: a checkpoint, then encryption with the
    /// salt of the library and new IVs, then a temporary file next to the
    /// library file that is synced and renamed over it (QSaveFile). The old
    /// file stays until the rename. Before it writes, it checks the stick
    /// again. It refuses if lastErrorMessage() is set, if the stick folder is
    /// gone, if a WAL file with data is next to the library file, or if the
    /// library file is not as it was: after create() the file must not exist,
    /// and after open() or an earlier save() the file must have the size, the
    /// time of the last change and the SHA-256 that were recorded. It makes
    /// only the folders PIONEER/rekordbox, and only below the stick folder. It
    /// refuses while a batch is open (beginBatch()).
    /// After a successful save() it records the bytes that it wrote, so a
    /// second save() works. Returns false and sets *pErrorMessage (if it is not
    /// null) on an error; a failed save() does not set lastErrorMessage().
    bool save(QString* pErrorMessage);

    QString deviceName() const;

    /// The first error of a change to the library, or an empty string. A
    /// library with an error is not complete, so save() refuses it.
    QString lastErrorMessage() const;

    /// Starts one transaction for many changes. While the batch is open, the
    /// typed methods below do not commit on their own, and save() refuses.
    /// Returns false and sets lastErrorMessage() if a batch is open already or
    /// if SQLite cannot start it.
    bool beginBatch();

    /// Commits the batch. Returns false and sets lastErrorMessage() if no
    /// batch is open or if SQLite cannot commit. After a failed commit the
    /// batch is rolled back and closed.
    bool commitBatch();

    /// Undoes all changes of the batch and closes it. This does not set
    /// lastErrorMessage(). It clears the name caches of findOrAddArtist(),
    /// findOrAddAlbum(), findOrAddGenre() and findOrAddKey(). The destructor
    /// does this for a batch that is open. Returns false and sets
    /// lastErrorMessage() if no batch is open.
    bool rollbackBatch();

    // The methods below change or read the rows of the library. All IDs are
    // 64-bit. A new row gets the ID MAX(id) + 1 (1 for an empty table). Outside
    // a batch each method does its own commit; a method that changes more than
    // one row (setPlaylistContents()) undoes only its own change on an error.
    // If SQLite reports an error, the method sets lastErrorMessage() and
    // returns -1 (or an empty value, or nothing), so that save() refuses.

    /// The ID of the artist with this name (a case-sensitive match). If there
    /// is none, it adds the artist. The object keeps the IDs of the names of
    /// the artist, album, genre and key tables in memory, and reads each table
    /// once, at the first call: it does not see a row that a call from outside
    /// adds later.
    qint64 findOrAddArtist(const QString& name);

    /// The ID of the album with this name and this album artist (no artist
    /// matches an album with no artist). If there is none, it adds the album.
    qint64 findOrAddAlbum(const QString& name, std::optional<qint64> artistId);

    /// Like findOrAddArtist(), for the genre table.
    qint64 findOrAddGenre(const QString& name);

    /// Like findOrAddArtist(), for the key table.
    qint64 findOrAddKey(const QString& name);

    /// Adds a cover image, for example "/PIONEER/Artwork/00001/b1.jpg". It
    /// adds a row on each call: the caller makes sure that it does not add the
    /// same image twice.
    qint64 addImage(const QString& stickPath);

    /// Adds a track and returns its ID.
    qint64 addContent(const DlpContent& content);

    /// Writes all columns of an existing track again. The ID does not change.
    /// It is an error if the library has no track with this ID.
    void updateContent(qint64 contentId, const DlpContent& content);

    /// The ID of the first track with this library ID and track ID.
    std::optional<qint64> findContentByMaster(qint64 masterDbId, qint64 masterContentId);

    /// The ID of the first track with this path (a case-sensitive match).
    std::optional<qint64> findContentByPath(const QString& stickPath);

    /// Adds a playlist, or a folder of playlists, after the playlists and
    /// folders with the same parent. parentId is the ID of a folder, or 0 for
    /// the top level. Returns the ID.
    qint64 addPlaylist(const QString& name, qint64 parentId, bool isFolder);

    /// The ID of the first playlist or folder with this name and this parent.
    std::optional<qint64> findPlaylist(const QString& name, qint64 parentId);

    /// Like findPlaylist(), but only for folders of playlists.
    std::optional<qint64> findFolder(const QString& name, qint64 parentId);

    /// Like findPlaylist(), but only for playlists and not for folders.
    std::optional<qint64> findPlaylistOnly(const QString& name, qint64 parentId);

    /// All tracks of the library, in the order of their IDs.
    QList<DlpContentRef> listContents();

    /// The ID and the path of all cover images, in the order of their IDs.
    QList<QPair<qint64, QString>> listImages();

    /// Replaces the tracks of a playlist with these tracks, in this order.
    void setPlaylistContents(qint64 playlistId, const QList<qint64>& contentIds);

    /// Sets property.numberOfContents to the number of tracks.
    void updateContentCount();

  protected:
    /// The SQLite handle of the temporary copy; for the typed row methods.
    sqlite3* handle() const;

    /// Records an error of a change to the library. The first one stays.
    void setLastError(const QString& message);

  private:
    friend class DeviceLibraryPlusDbTest;

    explicit DeviceLibraryPlusDb(const QString& stickRoot);

    bool prepareWorkingFile(QString* pErrorMessage);
    bool openWorkingFile(QString* pErrorMessage);
    bool buildNewLibrary(const QString& deviceName, qint64 masterDbId, QString* pErrorMessage);
    bool checkExistingLibrary(QString* pErrorMessage);
    /// Checks the stick just before save() writes to it.
    bool checkStickBeforeWrite(QString* pErrorMessage) const;

    /// The IDs of the rows of a table that has a name: filled from the table
    /// at the first use. Only the first row for a name is in the cache.
    struct NameCache {
        bool loaded = false;
        QHash<QString, qint64> ids;
    };

    qint64 findOrAddNamed(const char* table,
            const char* idColumn,
            NameCache& cache,
            const QString& name);
    /// Reads the rows of a table into the cache. Returns false and sets
    /// lastErrorMessage() on an error.
    bool loadNameCache(const char* table, const char* idColumn, NameCache& cache);
    bool loadAlbumCache();
    /// Forgets all names, so that the next call reads the tables again.
    void clearNameCaches();
    std::optional<qint64> findPlaylistRow(
            const QString& name, qint64 parentId, std::optional<int> attribute);
    /// Records an error of a change to the library. Returns -1.
    qint64 failChange(const QString& reason);

    /// What Mix Two knows about the library file on the stick: the values of
    /// the bytes that it read in open() or wrote in save().
    struct StickFileState {
        qint64 size = 0;
        QDateTime modified;
        QByteArray sha256;
    };

    const QString m_stickRoot;
    QTemporaryDir m_workDir;
    QString m_workingFilePath;
    sqlite3* m_pDb;
    QByteArray m_salt;
    QString m_deviceName;
    QString m_lastError;
    /// Empty after create(): the stick has no library file. Set by open() and
    /// by each save().
    std::optional<StickFileState> m_knownStickFile;
    /// True between beginBatch() and commitBatch() or rollbackBatch().
    bool m_batchOpen = false;
    NameCache m_artistCache, m_genreCache, m_keyCache;
    /// The key is the name and the album artist; an album with no artist has
    /// the ID -1 as the artist. The IDs of the rows are not negative.
    bool m_albumCacheLoaded = false;
    QHash<QPair<QString, qint64>, qint64> m_albumIds;
};

} // namespace rekordbox
} // namespace mixxx
