#pragma once

#include <QColor>
#include <QDateTime>
#include <QSet>
#include <QString>
#include <optional>

#include "proto/keys.pb.h"

namespace mixxx {
namespace rekordbox {

/// Mix Two: the rules that turn Mix Two track data into the values of
/// rekordbox's Device Library Plus. The functions are pure (the three that
/// read a file only read it). They have no state and no database, so any
/// thread can call them. rekordboxTimingOffsetMs() calls mp3guessenc, which
/// keeps its state in global variables, through mixxx::mp3guessencTimingShiftCase()
/// (util/mp3timingshift.h): its lock lets only one mp3guessenc call at a time
/// run, also against the Serato tag import.

/// The `fileType` code of a file extension (case-insensitive, no dot):
/// mp3 1, mp4 3, m4a and aac 4, flac 5, wav 11, aif and aiff 12. Any other
/// extension gives 0.
int dlpFileType(const QString& fileExtension);

/// The ID (1-8) of the rekordbox colour that is nearest to the colour (by
/// the squared distance of the RGB values). No colour, or black, gives 0.
/// The ANLZ writer uses it for memory cues.
int dlpColorId(const std::optional<QRgb>& color);

/// The key as Lancelot text (`8A` for A minor, `8B` for C major). An empty
/// string for an invalid key.
QString dlpKeyName(track::io::key::ChromaticKey key);

/// The BPM times 100, rounded. 0 for a value that is not finite or not above 0.
int dlpBpmx100(double bpm);

/// The date as `yyyy-MM-dd` in local time. Today's date when the date is
/// not valid.
QString dlpDate(const QDateTime& dateTime);

/// A file or folder name that a FAT32 stick and Windows accept: the
/// characters `\ / : * ? " < > |` and the characters below U+0020 become
/// `_`, trailing dots and spaces are removed, an empty name becomes `_`, a
/// Windows device name (CON, PRN, AUX, NUL, COM1-COM9, LPT1-LPT9, with or
/// without an extension) gets a leading `_`. The name is cut to 255 UTF-16
/// units, never inside a surrogate pair, and keeps its extension.
QString fatSafeName(const QString& name);

/// `/Contents/<artist>/<album>/<file name>` on the stick. An empty artist
/// gives `UnknownArtist`, an empty album gives `UnknownAlbum`. Each part goes
/// through fatSafeName().
QString dlpAudioStickPath(
        const QString& artist, const QString& album, const QString& fileName);

/// The key for every look-up of a stick path: the case-folded path with `/`
/// for `\`. FAT32 does not tell `A.mp3` from `a.mp3`.
QString dlpPathKey(const QString& stickPath);

/// The ANLZ folder of an audio file, for example
/// `/PIONEER/USBANLZ/P0AB/1234ABCD` (no file name). `P` and 3 hex digits are
/// the first 12 bits, and 8 hex digits the next 32 bits, of the SHA-1 of the
/// path key. When the folder is in `usedFolderKeys` (the dlpPathKey() of
/// the used folders), 1 is added to the 32-bit part until the folder is free.
/// When `stickRoot` (the stick root without a `/` at the end) is not empty,
/// a folder or file that exists at that place on the stick is also used:
/// another program, or an export that did not finish, can have files there.
QString dlpAnlzFolder(const QString& stickPath,
        const QSet<QString>& usedFolderKeys,
        const QString& stickRoot = QString());

/// True when a path that the database of a stick gives can be joined to the
/// stick root: it starts with `/`, it has no empty, `.` or `..` part, no `\`,
/// no `:` and no character below U+0020, and QDir::cleanPath() does not
/// change it. The export uses no other path of the database (row paths, ANLZ
/// paths, image paths) and removes no file whose path fails the check.
bool dlpIsSafeStickPath(const QString& stickPath);

/// The number of PCM frames of an MP3 file: the number of MPEG audio frames
/// times the samples of a frame (1152 for MPEG-1 Layer III, 576 for MPEG-2
/// and MPEG-2.5 Layer III). A Xing, Info or VBRI frame is not counted. The
/// function skips an ID3v2 tag at the start and stops at an ID3v1 tag, an APE
/// tag or the end of the file. A frame counts when the next header is also
/// valid. Only Layer III is known. No value when the file has no frame or
/// cannot be read.
std::optional<quint32> dlpMpegPcmFrames(const QString& filePath);

/// The timing offset in ms between the decoder of Mix Two and the one of
/// rekordbox for a file. It is the offset that the rekordbox reader
/// subtracts from cue, loop and beat times. 0 when the file needs none.
/// The mp3guessenc part runs under the lock of mp3guessencTimingShiftCase().
int rekordboxTimingOffsetMs(const QString& location);

/// The bit depth of a file: from TagLib for WAV (11), AIFF (12) and FLAC (5),
/// 16 for all other types and when TagLib fails.
int dlpBitDepth(const QString& filePath, int fileType);

} // namespace rekordbox
} // namespace mixxx
