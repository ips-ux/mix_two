// AI-generated (Claude Code). Start of AI-generated text.
#include "library/consolidate/folderpattern.h"

#include <QRegularExpression>
#include <QStringList>

#include "track/trackrecord.h"

namespace mixxx {
namespace library {

namespace {

const QString kDefaultPattern = QStringLiteral("<artist>/<file>");
const QString kNoArtistFolder = QStringLiteral("Various-Uncategorized");
const QString kArtistToken = QStringLiteral("<artist>");
const QString kFileToken = QStringLiteral("<file>");
constexpr int kMaxPartLength = 120;

bool isReservedName(const QString& part) {
    // Windows reserves these names, also with an extension.
    const QString base = part.section(QLatin1Char('.'), 0, 0).trimmed().toUpper();
    if (base == QLatin1String("CON") || base == QLatin1String("PRN") ||
            base == QLatin1String("AUX") || base == QLatin1String("NUL")) {
        return true;
    }
    if (base.size() == 4 &&
            (base.startsWith(QLatin1String("COM")) || base.startsWith(QLatin1String("LPT")))) {
        const QChar digit = base.at(3);
        return digit >= QLatin1Char('1') && digit <= QLatin1Char('9');
    }
    return false;
}

QString replaceToken(const QString& text, const QString& token, const QString& value) {
    QString result = text;
    return result.replace(token, value, Qt::CaseInsensitive);
}

void cutTrailingDotsAndSpaces(QString* pText) {
    while (pText->endsWith(QLatin1Char('.')) || pText->endsWith(QLatin1Char(' '))) {
        pText->chop(1);
    }
}

} // namespace

QString defaultFolderPattern() {
    return kDefaultPattern;
}

QString sanitizePathPart(const QString& value) {
    QString result;
    result.reserve(value.size());
    for (const QChar c : value) {
        if (c.unicode() < 0x20 || c == QLatin1Char('\\') || c == QLatin1Char('/') ||
                c == QLatin1Char(':') || c == QLatin1Char('*') || c == QLatin1Char('?') ||
                c == QLatin1Char('"') || c == QLatin1Char('<') || c == QLatin1Char('>') ||
                c == QLatin1Char('|')) {
            result.append(QLatin1Char('_'));
        } else {
            result.append(c);
        }
    }
    result = result.trimmed();
    // Windows does not accept a name that ends with a space or a dot.
    cutTrailingDotsAndSpaces(&result);
    if (result.size() > kMaxPartLength) {
        result = result.left(kMaxPartLength);
        // Do not cut a surrogate pair in the middle.
        if (result.back().isHighSurrogate()) {
            result.chop(1);
        }
        cutTrailingDotsAndSpaces(&result);
    }
    if (!result.isEmpty() && isReservedName(result)) {
        result.prepend(QLatin1Char('_'));
    }
    return result;
}

QString libraryFolderFor(const TrackRecord& trackRecord, const QString& pattern) {
    const QString usedPattern = pattern.trimmed().isEmpty() ? kDefaultPattern : pattern;

    // The parts of the pattern, up to the file token.
    QStringList parts;
    static const QRegularExpression separator(QStringLiteral("[/\\\\]"));
    const QStringList rawParts = usedPattern.split(separator);
    for (const QString& rawPart : rawParts) {
        if (rawPart.trimmed().compare(kFileToken, Qt::CaseInsensitive) == 0) {
            break;
        }
        parts.append(rawPart);
    }

    const TrackMetadata& metadata = trackRecord.getMetadata();
    const QString artist = sanitizePathPart(metadata.getTrackInfo().getArtist());
    const QString album = sanitizePathPart(metadata.getAlbumInfo().getTitle());
    const QString year = sanitizePathPart(metadata.getTrackInfo().getYear());
    const QString genre = sanitizePathPart(metadata.getTrackInfo().getGenre());

    QStringList folders;
    for (const QString& part : std::as_const(parts)) {
        if (artist.isEmpty() && part.contains(kArtistToken, Qt::CaseInsensitive)) {
            // A track with no artist goes to one folder, whatever the pattern.
            return kNoArtistFolder;
        }
        QString folder = part;
        folder = replaceToken(folder, kArtistToken, artist);
        folder = replaceToken(folder, QStringLiteral("<album>"), album);
        folder = replaceToken(folder, QStringLiteral("<year>"), year);
        folder = replaceToken(folder, QStringLiteral("<genre>"), genre);
        folder = sanitizePathPart(folder);
        if (!folder.isEmpty()) {
            folders.append(folder);
        }
    }
    return folders.join(QLatin1Char('/'));
}

} // namespace library
} // namespace mixxx

// End of AI-generated text.
