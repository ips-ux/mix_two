#pragma once
// AI-generated (Claude Code). Start of AI-generated text.
// Export Loop As WAV: write the audio of a loop to a WAV file at 48 kHz and
// 16 bit, as rekordbox does with its "Export Loop As WAV" function.

#include <QString>

#include "util/types.h"

namespace mixxx {
namespace library {

/// The sample rate of the WAV file.
constexpr int kExportLoopSampleRate = 48000;

enum class ExportLoopStatus {
    /// The WAV file is written.
    Exported,
    /// The loop has no length or no valid position, or it starts after the end
    /// of the audio. Nothing is written.
    InvalidLoop,
    /// The source file does not exist.
    SourceMissing,
    /// No decoder could open the source file.
    SourceOpenFailed,
    /// The decoder failed while it read the loop. No file is left.
    SourceReadFailed,
    /// The target is the source file. Mix Two never writes to a source file.
    TargetIsSource,
    /// The folder of the target file does not exist.
    TargetFolderMissing,
    /// The loop is too long for a WAV file (more than 4 GB).
    TooLong,
    /// The WAV file could not be written or put in place. No file is left,
    /// and an existing file at the target stays as it was.
    WriteFailed,
};

struct ExportLoopResult {
    ExportLoopStatus status = ExportLoopStatus::InvalidLoop;
    /// The frames in the WAV file (set when `status` is `Exported`).
    SINT framesWritten = 0;
    /// True when the rate change used the fallback scaler of the deck engine
    /// (a build without FFmpeg). Tell the user.
    bool limitedQuality = false;
};

/// The frames that the WAV file holds for a loop of `loopFrames` source
/// frames at `sourceRate`: the loop length in seconds times 48000, rounded to
/// the nearest frame.
SINT exportLoopFrameCount(SINT loopFrames, int sourceRate);

/// True when a WAV file with that many frames (2 channels, 16 bit) has sizes
/// that fit in the 32 bit fields of the RIFF header.
bool exportLoopFitsInWav(SINT frames);

/// Writes the loop `[startFrame, endFrame)` of the file `sourcePath` to the
/// WAV file `targetPath`. The positions are frame indexes of the source file
/// at its own sample rate (the end is not part of the loop). An end behind the
/// end of the audio is cut to the end of the audio.
///
/// The audio has the speed and the pitch of the file: no tempo change, no key
/// lock, no effect. A source at 48 kHz is copied. Another rate goes through
/// libswresample (default filter) when the build has FFmpeg, else through the
/// scaler of the deck engine (`limitedQuality`). `allowFfmpeg` false forces
/// the scaler (for tests). The WAV file has two channels
/// (a mono source goes to both), 16 bit and 48 kHz.
///
/// The source file is only read. The function writes a temporary file in the
/// folder of the target and renames it when it is complete, so a failure
/// leaves no half file, and a file that exists at the target (the caller asks
/// the user before) stays as it was until the new file is ready.
///
/// The function does not use the GUI thread, the audio thread or the library:
/// call it in a worker thread.
ExportLoopResult exportLoopAsWav(
        const QString& sourcePath,
        SINT startFrame,
        SINT endFrame,
        const QString& targetPath,
        bool allowFfmpeg = true);

/// The text for the user about a rate change of limited quality.
QString exportLoopLimitedQualityText();

/// The text for the user that tells why an export failed (Simplified
/// Technical English). `Exported` gives an empty text.
QString exportLoopErrorText(ExportLoopStatus status);

} // namespace library
} // namespace mixxx
// End of AI-generated text.
