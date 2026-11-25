// AI-generated (Claude Code). Start of AI-generated text.
#include "library/exportloop.h"

#include <sndfile.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <vector>

#include "engine/bufferscalers/enginebufferscalehalfbandlagrange.h"
#include "engine/readaheadmanager.h"
#include "sources/audiosource.h"
#include "sources/audiosourcestereoproxy.h"
#include "sources/soundsourceproxy.h"
#include "track/track.h"
#include "util/samplebuffer.h"

namespace mixxx {
namespace library {

namespace {

constexpr SINT kReadFrames = 4096;
constexpr SINT kWriteFrames = 1024;

/// Feeds the resampler with the frames of the loop, read from a decoded
/// source. The resampler needs the ReadAheadManager interface; the engine
/// uses the same scaler with the real one. Before the first frame and behind
/// the last frame of the loop the signal is silence, so the result depends on
/// the loop only.
class LoopFrameReader : public ReadAheadManager {
  public:
    LoopFrameReader(AudioSourcePointer pSource, SINT startFrame, SINT endFrame)
            : m_pSource(std::move(pSource)),
              m_nextFrame(startFrame),
              m_endFrame(endFrame),
              m_buffer(kReadFrames * 2) {
    }

    bool failed() const {
        return m_failed;
    }

    SINT getNextSamples(double,
            CSAMPLE* pBuffer,
            SINT requestedSamples,
            mixxx::audio::ChannelCount channelCount) override {
        DEBUG_ASSERT(channelCount == 2);
        const SINT requestedFrames = requestedSamples / 2;
        SINT done = 0;
        while (done < requestedFrames) {
            const SINT want = std::min({requestedFrames - done,
                    kReadFrames,
                    m_endFrame - m_nextFrame});
            if (want <= 0 || m_failed) {
                break;
            }
            const auto readFrames = m_pSource->readSampleFrames(
                    WritableSampleFrames(
                            IndexRange::forward(m_nextFrame, want),
                            SampleBuffer::WritableSlice(m_buffer.data(), want * 2)));
            if (readFrames.frameIndexRange().empty() ||
                    readFrames.frameIndexRange().start() != m_nextFrame) {
                m_failed = true;
                break;
            }
            const SINT got = readFrames.frameLength();
            std::copy_n(readFrames.readableData(), got * 2, pBuffer + done * 2);
            done += got;
            m_nextFrame += got;
        }
        // Silence outside the loop (and after a failure).
        std::fill(pBuffer + done * 2, pBuffer + requestedFrames * 2, 0.0f);
        return requestedFrames * 2;
    }

  private:
    AudioSourcePointer m_pSource;
    SINT m_nextFrame;
    SINT m_endFrame;
    SampleBuffer m_buffer;
    bool m_failed = false;
};

// libsndfile writes through a QFile, so a path with any character works on
// every system.
sf_count_t vioLength(void* pUser) {
    return static_cast<QFile*>(pUser)->size();
}
sf_count_t vioSeek(sf_count_t offset, int whence, void* pUser) {
    auto* pFile = static_cast<QFile*>(pUser);
    qint64 pos = offset;
    if (whence == SEEK_CUR) {
        pos += pFile->pos();
    } else if (whence == SEEK_END) {
        pos += pFile->size();
    }
    pFile->seek(pos);
    return pFile->pos();
}
sf_count_t vioRead(void* pData, sf_count_t count, void* pUser) {
    return static_cast<QFile*>(pUser)->read(static_cast<char*>(pData), count);
}
sf_count_t vioWrite(const void* pData, sf_count_t count, void* pUser) {
    const qint64 written =
            static_cast<QFile*>(pUser)->write(static_cast<const char*>(pData), count);
    return written < 0 ? 0 : written;
}
sf_count_t vioTell(void* pUser) {
    return static_cast<QFile*>(pUser)->pos();
}

std::filesystem::path toFsPath(const QString& path) {
#ifdef Q_OS_WIN
    return std::filesystem::path(path.toStdWString());
#else
    return std::filesystem::path(QFile::encodeName(path).toStdString());
#endif
}

} // namespace

SINT exportLoopFrameCount(SINT loopFrames, int sourceRate) {
    if (loopFrames <= 0 || sourceRate <= 0) {
        return 0;
    }
    return static_cast<SINT>(std::llround(
            static_cast<double>(loopFrames) * kExportLoopSampleRate / sourceRate));
}

ExportLoopResult exportLoopAsWav(
        const QString& sourcePath,
        SINT startFrame,
        SINT endFrame,
        const QString& targetPath) {
    ExportLoopResult result;
    if (startFrame < 0 || endFrame <= startFrame) {
        result.status = ExportLoopStatus::InvalidLoop;
        return result;
    }
    const QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.isFile()) {
        result.status = ExportLoopStatus::SourceMissing;
        return result;
    }
    const QFileInfo targetInfo(targetPath);
    const QString targetFolder = targetInfo.absoluteDir().canonicalPath();
    if (targetFolder.isEmpty()) {
        result.status = ExportLoopStatus::TargetFolderMissing;
        return result;
    }
    // Compare the real names (links resolved), also when the target does not
    // exist yet.
#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity kCase = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity kCase = Qt::CaseSensitive;
#endif
    const QString targetFull = targetFolder + QLatin1Char('/') + targetInfo.fileName();
    if (targetFull.compare(sourceInfo.canonicalFilePath(), kCase) == 0) {
        result.status = ExportLoopStatus::TargetIsSource;
        return result;
    }

    // The decoder opens the source for reading only. The track is a temporary
    // object that is not in the library, and nothing writes its tags.
    SoundSourceProxy proxy(Track::newTemporary(sourceInfo.absoluteFilePath()));
    AudioSource::OpenParams openParams;
    openParams.setChannelCount(audio::ChannelCount::stereo());
    AudioSourcePointer pSource = proxy.openAudioSource(openParams);
    if (!pSource) {
        result.status = ExportLoopStatus::SourceOpenFailed;
        return result;
    }
    if (pSource->getSignalInfo().getChannelCount() != audio::ChannelCount::stereo()) {
        pSource = AudioSourceStereoProxy::create(pSource, kReadFrames);
    }
    const int sourceRate = static_cast<int>(pSource->getSignalInfo().getSampleRate());
    const IndexRange audioRange = pSource->frameIndexRange();
    if (sourceRate <= 0 || startFrame < audioRange.start() || startFrame >= audioRange.end()) {
        result.status = ExportLoopStatus::InvalidLoop;
        return result;
    }
    endFrame = std::min(endFrame, audioRange.end());
    const SINT outFrames = exportLoopFrameCount(endFrame - startFrame, sourceRate);
    if (outFrames <= 0) {
        result.status = ExportLoopStatus::InvalidLoop;
        return result;
    }

    // The temporary file is in the folder of the target, so the final rename
    // stays on one volume.
    // The name is new and unique; "NewOnly" never opens an existing file.
    QFile tempFile(targetFolder + QStringLiteral("/.mixtwo-loop-") +
            QUuid::createUuid().toString(QUuid::Id128) + QStringLiteral(".tmp"));
    if (!tempFile.open(QIODevice::ReadWrite | QIODevice::NewOnly)) {
        result.status = ExportLoopStatus::WriteFailed;
        return result;
    }

    result.status = ExportLoopStatus::WriteFailed;
    bool written = false;
    {
        SF_VIRTUAL_IO vio;
        vio.get_filelen = vioLength;
        vio.seek = vioSeek;
        vio.read = vioRead;
        vio.write = vioWrite;
        vio.tell = vioTell;
        SF_INFO info = {};
        info.samplerate = kExportLoopSampleRate;
        info.channels = 2;
        info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
        SNDFILE* pSndFile = sf_open_virtual(&vio, SFM_WRITE, &info, &tempFile);
        if (pSndFile) {
            // Values above full scale clip instead of wrapping around.
            sf_command(pSndFile, SFC_SET_CLIPPING, nullptr, SF_TRUE);

            LoopFrameReader reader(pSource, startFrame, endFrame);
            // Choice: the converter is the varispeed scaler of the deck
            // engine (band-limited half-band filter with cubic Lagrange
            // interpolation) at the rate source rate / 48000 with tempo and
            // pitch 1.0, so the audio keeps the speed and the pitch of the
            // file. The scaler starts at the first loop frame: its output
            // frame k is the source at frame k * sourceRate / 48000 from the
            // loop in point. At 48 kHz the output equals the source.
            EngineBufferScaleHalfbandLagrange scaler(&reader);
            scaler.setSignal(audio::SampleRate(kExportLoopSampleRate),
                    audio::ChannelCount::stereo());
            scaler.clear();
            double tempo = 1.0;
            double pitch = 1.0;
            scaler.setScaleParameters(
                    static_cast<double>(sourceRate) / kExportLoopSampleRate,
                    &tempo,
                    &pitch);

            std::vector<CSAMPLE> buffer(kWriteFrames * 2);
            SINT remaining = outFrames;
            written = true;
            while (remaining > 0 && written) {
                const SINT frames = std::min(remaining, kWriteFrames);
                scaler.scaleBuffer(buffer.data(), frames * 2);
                if (reader.failed()) {
                    result.status = ExportLoopStatus::SourceReadFailed;
                    written = false;
                } else if (sf_writef_float(pSndFile, buffer.data(), frames) != frames) {
                    written = false;
                }
                remaining -= frames;
            }
            if (sf_close(pSndFile) != 0 && written) {
                written = false;
            }
        }
    }
    const bool complete = written && tempFile.flush() && tempFile.error() == QFileDevice::NoError;
    tempFile.close();
    if (!complete) {
        QFile::remove(tempFile.fileName());
        return result;
    }

    // The rename replaces an existing file in one step.
    std::error_code error;
    std::filesystem::rename(toFsPath(tempFile.fileName()), toFsPath(targetFull), error);
    if (error) {
        QFile::remove(tempFile.fileName());
        return result;
    }
    result.status = ExportLoopStatus::Exported;
    result.framesWritten = outFrames;
    return result;
}

QString exportLoopErrorText(ExportLoopStatus status) {
    switch (status) {
    case ExportLoopStatus::Exported:
        return QString();
    case ExportLoopStatus::InvalidLoop:
        return QCoreApplication::translate("ExportLoop", "The loop has no length.");
    case ExportLoopStatus::SourceMissing:
        return QCoreApplication::translate("ExportLoop", "The track file does not exist.");
    case ExportLoopStatus::SourceOpenFailed:
        return QCoreApplication::translate("ExportLoop", "Mix Two cannot open the track file.");
    case ExportLoopStatus::SourceReadFailed:
        return QCoreApplication::translate(
                "ExportLoop", "Mix Two cannot read the loop from the track file.");
    case ExportLoopStatus::TargetIsSource:
        return QCoreApplication::translate("ExportLoop",
                "The new file is the track file. Select a different file name.");
    case ExportLoopStatus::TargetFolderMissing:
        return QCoreApplication::translate("ExportLoop", "The folder does not exist.");
    case ExportLoopStatus::WriteFailed:
        return QCoreApplication::translate("ExportLoop", "Mix Two cannot write the WAV file.");
    }
    return QString();
}

} // namespace library
} // namespace mixxx
// End of AI-generated text.
