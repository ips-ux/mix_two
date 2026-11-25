#pragma once

// AI-generated (Claude Code). Start of AI-generated text.
#include <QString>
#include <QVariant>

namespace mixxx {

/// The lossless formats whose header tells the bit depth.
enum class BitDepthFileKind {
    Flac,
    Wav,
    Aiff,
};

/// Reads the bits per sample from the header of a file of the given kind.
/// Returns 0 if the file cannot be read or the header has no value.
int bitDepthFromHeader(const QString& filePath, BitDepthFileKind kind);

/// Reads the bit depth of a file. The file name suffix selects the kind.
/// Returns 0 (unknown) for lossy formats, for other formats and for files
/// that cannot be read.
int bitDepthFromFile(const QString& filePath);

/// The value that a library column shows for a stored bit depth. It is an
/// empty value for unknown (0 or null) and for values that are not valid.
QVariant bitDepthDisplayValue(const QVariant& stored);

} // namespace mixxx
// End of AI-generated text.
