#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

#include "audio/frame.h"
#include "audio/types.h"
#include "library/rekordbox/anlztrackdata.h"

namespace mixxx {
namespace rekordbox {

/// Mix Two: the bytes of the analysis files, for the tests and for
/// writeAnlzFiles().
namespace anlz {

/// The PMAI file header followed by the sections.
QByteArray fileBytes(const QList<QByteArray>& sections);

/// One section: the four letters, `lenHeader`, the length of the section
/// (12 + the body) and the body. The body starts after these 12 bytes.
QByteArray section(const char* fourcc, quint32 lenHeader, const QByteArray& body);

/// The bytes of ANLZ0000.DAT.
QByteArray datBytes(const AnlzTrackData& data);
/// The bytes of ANLZ0000.EXT.
QByteArray extBytes(const AnlzTrackData& data);
/// The bytes of ANLZ0000.2EX.
QByteArray twoExBytes(const AnlzTrackData& data);

/// The time in ms of a position: rounded, plus the timing offset, and not
/// less than 0. A position that is not valid or not finite gives 0.
quint32 msFromFrames(audio::FramePos position,
        audio::SampleRate sampleRate,
        int timingOffsetMs);

} // namespace anlz

/// Mix Two: writes ANLZ0000.DAT, ANLZ0000.EXT and ANLZ0000.2EX into `folder`
/// (created when it is missing). The three files are one set:
/// - The function writes all three to temporary files (QSaveFile) first and
///   renames them into place only when all three writes succeeded. On a
///   failure before that, nothing in `folder` changes (an old set stays).
/// - If a rename fails after another file is in place, the function removes
///   all three files, so a mixed set of new and old files never stays. The
///   old set of the track is lost in that case.
/// On a failure it returns false and sets *pErrorMessage (if it is not null)
/// to "Cannot write the analysis files: <file name>: <Qt error>".
bool writeAnlzFiles(const AnlzTrackData& data,
        const QString& folder,
        QString* pErrorMessage);

} // namespace rekordbox
} // namespace mixxx
