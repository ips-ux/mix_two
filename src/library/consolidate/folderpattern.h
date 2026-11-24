#pragma once
// AI-generated (Claude Code). Start of AI-generated text.
// The folder pattern of the library: where a consolidated track goes.

#include <QString>

namespace mixxx {

class TrackRecord;

namespace library {

/// The folder pattern that Mix Two uses when the user sets none:
/// `<artist>/<file>`. The file name of a track is never changed; the pattern
/// only says in which folder below the library folder the file goes.
QString defaultFolderPattern();

/// The folder (relative to the library folder, with `/` between the parts)
/// for a track, from a pattern.
///
/// The pattern is a list of folder names separated by `/` (or `\`). A name
/// can hold these tokens: `<artist>`, `<album>`, `<year>`, `<genre>`. The
/// token `<file>` marks the place of the file name; it is the last part and
/// the function leaves it out (and ignores everything after it).
///
/// - An empty or blank pattern is the default pattern.
/// - A track with no artist goes to `Various-Uncategorized`, whatever the
///   rest of the pattern says (when the pattern uses `<artist>`).
/// - A part that is empty after the tokens are filled in is left out.
/// - The characters that no file system accepts in a folder name become `_`.
///
/// Choice: the spelling of the tokens, and the rule that an empty album,
/// year or genre leaves its folder out, are Mix Two choices.
QString libraryFolderFor(const TrackRecord& trackRecord, const QString& pattern);

/// One value as a safe folder or file name part: the characters
/// `\ / : * ? " < > |` and control characters become `_`, space and dot at
/// the end are cut, and a name that Windows reserves (`CON`, `NUL`, ...) gets
/// a `_` in front. A blank value gives an empty string.
QString sanitizePathPart(const QString& value);

} // namespace library
} // namespace mixxx

// End of AI-generated text.
