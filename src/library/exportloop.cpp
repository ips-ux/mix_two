// AI-generated (Claude Code). Start of AI-generated text.
#include "library/exportloop.h"

#include <sndfile.h>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <vector>

#ifdef Q_OS_WIN
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#ifdef __FFMPEG__
extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}
#endif

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

/// Reads the frames of the loop from a decoded source, in order.
class LoopSource {
  public:
    LoopSource(AudioSourcePointer pSource, SINT startFrame, SINT endFrame)
            : m_pSource(std::move(pSource)),
              m_nextFrame(startFrame),
              m_endFrame(endFrame),
              m_buffer(kReadFrames * 2) {
    }

    bool failed() const {
        return m_failed;
    }

    /// Copies up to `maxFrames` frames (at most kReadFrames) to `pOut`.
    /// Returns 0 at the end of the loop or after a failure.
    SINT read(CSAMPLE* pOut, SINT maxFrames) {
        const SINT want = std::min({maxFrames, kReadFrames, m_endFrame - m_nextFrame});
        if (want <= 0 || m_failed) {
            return 0;
        }
        const auto readFrames = m_pSource->readSampleFrames(
                WritableSampleFrames(
                        IndexRange::forward(m_nextFrame, want),
                        SampleBuffer::WritableSlice(m_buffer.data(), want * 2)));
        if (readFrames.frameIndexRange().empty() ||
                readFrames.frameIndexRange().start() != m_nextFrame) {
            m_failed = true;
            return 0;
        }
        const SINT got = readFrames.frameLength();
        std::copy_n(readFrames.readableData(), got * 2, pOut);
        m_nextFrame += got;
        return got;
    }

  private:
    AudioSourcePointer m_pSource;
    SINT m_nextFrame;
    SINT m_endFrame;
    SampleBuffer m_buffer;
    bool m_failed = false;
};

/// Feeds the fallback scaler of the deck engine with the frames of the loop.
/// The scaler needs the ReadAheadManager interface. The scaler calls only
/// getNextSamples() here (the rate is always positive, so it never rewinds);
/// the other members of the base class are never called, and they are null in
/// the "only for testing" constructor. Before the first frame and behind the
/// last frame of the loop the signal is silence.
class LoopFrameReader : public ReadAheadManager {
  public:
    explicit LoopFrameReader(LoopSource* pLoop)
            : m_pLoop(pLoop) {
    }

    SINT getNextSamples(double,
            CSAMPLE* pBuffer,
            SINT requestedSamples,
            mixxx::audio::ChannelCount channelCount) override {
        DEBUG_ASSERT(channelCount == 2);
        const SINT requestedFrames = requestedSamples / 2;
        SINT done = 0;
        while (done < requestedFrames) {
            const SINT got = m_pLoop->read(pBuffer + done * 2, requestedFrames - done);
            if (got <= 0) {
                break;
            }
            done += got;
        }
        std::fill(pBuffer + done * 2, pBuffer + requestedFrames * 2, 0.0f);
        return requestedFrames * 2;
    }

  private:
    LoopSource* m_pLoop;
};

/// Takes the converted frames. Returns false when the write failed.
using FrameSink = std::function<bool(const CSAMPLE*, SINT)>;

#ifdef __FFMPEG__
/// The rate change with libswresample and its default filter (a windowed
/// sinc with a low-pass at the Nyquist rate of the lower of the two rates).
/// The converter delays the signal. The frames that it reports as delay
/// before the first input are dropped from the start of the output, so that
/// output frame 0 belongs to the loop in point. The tail that comes out at
/// the flush is cut to the length rule (and a short tail is filled with
/// silence), so the WAV is exactly the loop.
ExportLoopStatus convertWithFfmpeg(
        LoopSource* pLoop, int sourceRate, SINT outFrames, const FrameSink& sink) {
    SwrContext* pSwr = nullptr;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 28, 100) // FFmpeg 5.1
    AVChannelLayout layout;
    av_channel_layout_default(&layout, 2);
    const int allocResult = swr_alloc_set_opts2(&pSwr,
            &layout,
            AV_SAMPLE_FMT_FLT,
            kExportLoopSampleRate,
            &layout,
            AV_SAMPLE_FMT_FLT,
            sourceRate,
            0,
            nullptr);
    av_channel_layout_uninit(&layout);
    if (allocResult != 0) {
        return ExportLoopStatus::WriteFailed;
    }
#else
    pSwr = swr_alloc_set_opts(nullptr,
            AV_CH_LAYOUT_STEREO,
            AV_SAMPLE_FMT_FLT,
            kExportLoopSampleRate,
            AV_CH_LAYOUT_STEREO,
            AV_SAMPLE_FMT_FLT,
            sourceRate,
            0,
            nullptr);
    if (!pSwr) {
        return ExportLoopStatus::WriteFailed;
    }
#endif
    if (swr_init(pSwr) < 0) {
        swr_free(&pSwr);
        return ExportLoopStatus::WriteFailed;
    }

    std::vector<CSAMPLE> in(kReadFrames * 2);
    std::vector<CSAMPLE> out;
    SINT toDrop = static_cast<SINT>(swr_get_delay(pSwr, kExportLoopSampleRate));
    SINT sent = 0;
    ExportLoopStatus status = ExportLoopStatus::Exported;

    const auto handleOutput = [&](int frames) {
        SINT first = 0;
        if (toDrop > 0) {
            first = std::min<SINT>(toDrop, frames);
            toDrop -= first;
        }
        if (frames > first && sent < outFrames) {
            const SINT n = std::min<SINT>(frames - first, outFrames - sent);
            if (!sink(out.data() + first * 2, n)) {
                status = ExportLoopStatus::WriteFailed;
                return;
            }
            sent += n;
        }
    };
    const auto outCapacity = [&](SINT inFrames) {
        return static_cast<int>(av_rescale_rnd(swr_get_delay(pSwr, sourceRate) + inFrames,
                                       kExportLoopSampleRate,
                                       sourceRate,
                                       AV_ROUND_UP) +
                256);
    };

    for (;;) {
        const SINT got = pLoop->read(in.data(), kReadFrames);
        if (got <= 0) {
            break;
        }
        const int capacity = outCapacity(got);
        out.resize(static_cast<size_t>(capacity) * 2);
        uint8_t* outPlanes[1] = {reinterpret_cast<uint8_t*>(out.data())};
        const uint8_t* inPlanes[1] = {reinterpret_cast<const uint8_t*>(in.data())};
        const int n = swr_convert(pSwr, outPlanes, capacity, inPlanes, static_cast<int>(got));
        if (n < 0) {
            status = ExportLoopStatus::WriteFailed;
            break;
        }
        handleOutput(n);
        if (status != ExportLoopStatus::Exported) {
            break;
        }
    }
    if (status == ExportLoopStatus::Exported && !pLoop->failed()) {
        // Flush the tail of the filter.
        for (int round = 0; round < 8 && sent < outFrames; ++round) {
            const int capacity = outCapacity(0);
            out.resize(static_cast<size_t>(capacity) * 2);
            uint8_t* outPlanes[1] = {reinterpret_cast<uint8_t*>(out.data())};
            const int n = swr_convert(pSwr, outPlanes, capacity, nullptr, 0);
            if (n <= 0) {
                break;
            }
            handleOutput(n);
            if (status != ExportLoopStatus::Exported) {
                break;
            }
        }
    }
    swr_free(&pSwr);
    if (pLoop->failed()) {
        return ExportLoopStatus::SourceReadFailed;
    }
    if (status != ExportLoopStatus::Exported) {
        return status;
    }
    // A short tail is filled with silence.
    const std::vector<CSAMPLE> zeros(kWriteFrames * 2, 0.0f);
    while (sent < outFrames) {
        const SINT n = std::min<SINT>(outFrames - sent, kWriteFrames);
        if (!sink(zeros.data(), n)) {
            return ExportLoopStatus::WriteFailed;
        }
        sent += n;
    }
    return ExportLoopStatus::Exported;
}
#endif

/// The fallback: the varispeed scaler of the deck engine, at the rate
/// source rate / 48000 with tempo and pitch 1.0. It is an interpolator
/// without a low-pass; the caller tells the user.
ExportLoopStatus convertWithScaler(
        LoopSource* pLoop, int sourceRate, SINT outFrames, const FrameSink& sink) {
    LoopFrameReader reader(pLoop);
    EngineBufferScaleHalfbandLagrange scaler(&reader);
    scaler.setSignal(audio::SampleRate(kExportLoopSampleRate), audio::ChannelCount::stereo());
    scaler.clear();
    double tempo = 1.0;
    double pitch = 1.0;
    scaler.setScaleParameters(
            static_cast<double>(sourceRate) / kExportLoopSampleRate, &tempo, &pitch);
    std::vector<CSAMPLE> buffer(kWriteFrames * 2);
    SINT remaining = outFrames;
    while (remaining > 0) {
        const SINT frames = std::min(remaining, kWriteFrames);
        scaler.scaleBuffer(buffer.data(), frames * 2);
        if (pLoop->failed()) {
            return ExportLoopStatus::SourceReadFailed;
        }
        if (!sink(buffer.data(), frames)) {
            return ExportLoopStatus::WriteFailed;
        }
        remaining -= frames;
    }
    return ExportLoopStatus::Exported;
}

/// A source at 48 kHz: the frames are copied, with no converter.
ExportLoopStatus copyFrames(LoopSource* pLoop, SINT outFrames, const FrameSink& sink) {
    std::vector<CSAMPLE> buffer(kReadFrames * 2);
    SINT done = 0;
    while (done < outFrames) {
        const SINT got = pLoop->read(buffer.data(), std::min(kReadFrames, outFrames - done));
        if (got <= 0) {
            break;
        }
        if (!sink(buffer.data(), got)) {
            return ExportLoopStatus::WriteFailed;
        }
        done += got;
    }
    return done == outFrames ? ExportLoopStatus::Exported : ExportLoopStatus::SourceReadFailed;
}

/// Writes the data of the file to the disk (the Qt flush only empties the Qt
/// buffer).
bool syncToDisk(QFile* pFile) {
    if (!pFile->flush()) {
        return false;
    }
#ifdef Q_OS_WIN
    return FlushFileBuffers(reinterpret_cast<HANDLE>(_get_osfhandle(pFile->handle()))) != 0;
#else
    return fsync(pFile->handle()) == 0;
#endif
}

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

bool exportLoopFitsInWav(SINT frames) {
    // 44 bytes of header and 4 bytes for each frame (2 channels, 16 bit); the
    // sizes in the RIFF header have 32 bits.
    return frames >= 0 && frames <= (static_cast<SINT>(0xFFFFFFFFu) - 44) / 4;
}

ExportLoopResult exportLoopAsWav(
        const QString& sourcePath,
        SINT startFrame,
        SINT endFrame,
        const QString& targetPath,
        bool allowFfmpeg) {
#ifndef __FFMPEG__
    Q_UNUSED(allowFfmpeg);
#endif
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
    // Checked before any file is made.
    if (!exportLoopFitsInWav(outFrames)) {
        result.status = ExportLoopStatus::TooLong;
        return result;
    }

    // The temporary file is in the folder of the target, so the final rename
    // stays on one volume. The name is new and unique; "NewOnly" never opens
    // an existing file.
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

            LoopSource loop(pSource, startFrame, endFrame);
            // Choice: at 48 kHz the frames are copied. At another rate the
            // converter is libswresample with its default filter when the
            // build has FFmpeg; without FFmpeg it is the varispeed scaler of
            // the deck engine, and the result tells the caller that the
            // quality is limited. Tempo and pitch stay at 1.0 in both: the
            // audio has the speed and the pitch of the file.
            const FrameSink sink = [pSndFile](const CSAMPLE* pFrames, SINT frames) {
                return sf_writef_float(pSndFile, pFrames, frames) == frames;
            };
            ExportLoopStatus status;
            if (sourceRate == kExportLoopSampleRate) {
                status = copyFrames(&loop, outFrames, sink);
            } else {
#ifdef __FFMPEG__
                if (allowFfmpeg) {
                    status = convertWithFfmpeg(&loop, sourceRate, outFrames, sink);
                } else
#endif
                {
                    result.limitedQuality = true;
                    status = convertWithScaler(&loop, sourceRate, outFrames, sink);
                }
            }
            written = status == ExportLoopStatus::Exported;
            if (!written) {
                result.status = status;
            }
            if (sf_close(pSndFile) != 0 && written) {
                written = false;
                result.status = ExportLoopStatus::WriteFailed;
            }
        }
    }
    const bool complete = written && syncToDisk(&tempFile) &&
            tempFile.error() == QFileDevice::NoError;
    tempFile.close();
    if (!complete) {
        QFile::remove(tempFile.fileName());
        if (result.status == ExportLoopStatus::Exported) {
            result.status = ExportLoopStatus::WriteFailed;
        }
        return result;
    }

    // The rename replaces an existing file in one step.
    std::error_code error;
    std::filesystem::rename(toFsPath(tempFile.fileName()), toFsPath(targetFull), error);
    if (error) {
        QFile::remove(tempFile.fileName());
        result.status = ExportLoopStatus::WriteFailed;
        return result;
    }
    result.status = ExportLoopStatus::Exported;
    result.framesWritten = outFrames;
    return result;
}

QString exportLoopLimitedQualityText() {
    return QCoreApplication::translate("ExportLoop",
            "This version of Mix Two has no FFmpeg converter. The quality of the rate change is "
            "limited. Sound above 12 kHz can change.");
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
    case ExportLoopStatus::TooLong:
        return QCoreApplication::translate(
                "ExportLoop", "The loop is too long for a WAV file. Make the loop shorter.");
    case ExportLoopStatus::WriteFailed:
        return QCoreApplication::translate("ExportLoop", "Mix Two cannot write the WAV file.");
    }
    return QString();
}

} // namespace library
} // namespace mixxx
// End of AI-generated text.
