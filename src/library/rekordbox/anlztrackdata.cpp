#include "library/rekordbox/anlztrackdata.h"

#include <QColor>
#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "library/rekordbox/anlzwriter.h"
#include "track/beats.h"
#include "track/cue.h"
#include "track/track.h"
#include "util/color/predefinedcolorpalettes.h"
#include "util/color/rgbcolor.h"
#include "waveform/waveform.h"

namespace mixxx {
namespace rekordbox {

namespace {

constexpr int kMaxHotCues = 8;
constexpr int kMaxMemoryCues = 10;
// Two times within this many ms are the same time.
constexpr qint64 kSameTimeMs = 1;
// The number of beats in a bar.
constexpr int kBeatsInBar = 4;

const QString kNoWaveformMessage =
        QStringLiteral("The track has no waveform. Analyse the track first.");
const QString kNoSampleRateMessage = QStringLiteral(
        "The track has no valid sample rate. Analyse the track again.");

QRgb rgbFrom(RgbColor color) {
    const RgbColor::code_t code = color;
    return qRgb(qRed(code), qGreen(code), qBlue(code));
}

/// The number of a beat in its bar (1-4) from the number of beats between the
/// beat and the first beat marker. Negative for a beat before the marker.
int beatInBarFrom(int beatsFromMarker) {
    return ((beatsFromMarker % kBeatsInBar) + kBeatsInBar) % kBeatsInBar + 1;
}

/// Fills `beats` and `beatInBar`: one beat from the first beat to the last
/// beat before the end of the track. The tempo of a beat is the one its length
/// gives (the beats of a segment are evenly spaced), so it needs no beat
/// marker; the last beat repeats the tempo before it. The beat at the first
/// beat marker (for a constant tempo, the anchor beat) is beat 1 of its bar;
/// the numbers count forward and backward from it.
void gridFrom(const BeatsPointer& pBeats,
        SINT frames,
        audio::SampleRate sampleRate,
        AnlzTrackData* pData) {
    if (!pBeats) {
        return;
    }
    const audio::FramePos firstBeat = pBeats->firstBeat();
    if (!firstBeat.isValid()) {
        return;
    }
    const Beats::ConstIterator marker = pBeats->cfirstmarker();
    const audio::FramePos end(static_cast<double>(frames));
    for (auto it = pBeats->iteratorFrom(firstBeat); *it < end; ++it) {
        auto next = it;
        ++next;
        const double beatLengthFrames = *next - *it;
        const int tempo = beatLengthFrames > 0
                ? static_cast<int>(std::lround(
                          60.0 * sampleRate.value() / beatLengthFrames * 100))
                : 0;
        pData->beats.append(GridBeat{*it, tempo});
        pData->beatInBar.append(beatInBarFrom(static_cast<int>(it - marker)));
    }
    if (pData->beats.size() > 1) {
        pData->beats.last().tempo = pData->beats[pData->beats.size() - 2].tempo;
    }
}

/// The end of a loop in ms, or nothing when the cue has no end or the end is
/// not after the start.
std::optional<quint32> loopEndMsOf(const Cue::StartAndEndPositions& positions,
        audio::SampleRate sampleRate,
        quint32 startMs) {
    if (!positions.endPosition.isValid()) {
        return std::nullopt;
    }
    const quint32 endMs = anlz::msFromFrames(positions.endPosition, sampleRate, 0);
    if (endMs <= startMs) {
        return std::nullopt;
    }
    return endMs;
}

/// Mix Two gives a new cue or loop its default colour, so a memory cue or
/// loop with that colour has no colour of its own.
bool hasDefaultColor(RgbColor color) {
    return color == PredefinedColorPalettes::kDefaultCueColor ||
            color == PredefinedColorPalettes::kDefaultLoopColor;
}

/// `hotCue` is the number 1-8 of a hot cue, or 0 for a memory cue or loop.
AnlzCue cueFrom(const Cue& cue, audio::SampleRate sampleRate, int hotCue) {
    const Cue::StartAndEndPositions positions = cue.getStartAndEndPosition();
    AnlzCue result;
    result.hotCue = hotCue;
    result.timeMs = anlz::msFromFrames(positions.startPosition, sampleRate, 0);
    result.loopEndMs = loopEndMsOf(positions, sampleRate, result.timeMs);
    result.comment = cue.getLabel();
    const RgbColor color = cue.getColor();
    if (hotCue != 0 || !hasDefaultColor(color)) {
        result.color = rgbFrom(color);
    }
    return result;
}

bool isSameTime(quint32 a, quint32 b) {
    return std::abs(static_cast<qint64>(a) - static_cast<qint64>(b)) <=
            kSameTimeMs;
}

/// Cuts the sorted memory cues to the limit. The cue at `mainCueMs` (the cue
/// point of the player) stays: when it is not among the first cues, it takes
/// the place of the last one that stays. Returns true when cues were cut.
bool limitMemoryCues(QList<AnlzCue>* pCues, std::optional<quint32> mainCueMs) {
    if (pCues->size() <= kMaxMemoryCues) {
        return false;
    }
    if (mainCueMs.has_value()) {
        const auto isMain = [&](const AnlzCue& cue) {
            return isSameTime(cue.timeMs, *mainCueMs);
        };
        const auto limit = pCues->cbegin() + kMaxMemoryCues;
        if (std::none_of(pCues->cbegin(), limit, isMain)) {
            const auto main = std::find_if(limit, pCues->cend(), isMain);
            if (main != pCues->cend()) {
                (*pCues)[kMaxMemoryCues - 1] = *main;
            }
        }
    }
    pCues->resize(kMaxMemoryCues);
    return true;
}

} // namespace

std::optional<AnlzTrackData> anlzTrackDataFrom(const Track& track,
        const ConstWaveformPointer& pWaveform,
        const QString& stickPath,
        int timingOffsetMs,
        QStringList* pWarnings,
        QString* pErrorMessage) {
    const auto fail = [&](const QString& message) {
        if (pErrorMessage) {
            *pErrorMessage = message;
        }
        return std::optional<AnlzTrackData>();
    };

    const audio::SampleRate sampleRate = track.getSampleRate();
    if (!sampleRate.isValid()) {
        return fail(kNoSampleRateMessage);
    }
    // The analyser attaches a waveform that is filled with zeros before it
    // has processed the track, so a waveform that is not complete is no
    // waveform.
    if (!pWaveform || pWaveform->getDataSize() == 0 ||
            pWaveform->getCompletion() < pWaveform->getDataSize()) {
        return fail(kNoWaveformMessage);
    }

    AnlzTrackData data;
    data.stickPath = stickPath;
    data.sampleRate = sampleRate;
    data.frames = static_cast<SINT>(std::lround(track.getDuration() * sampleRate.value()));
    if (data.frames <= 0) {
        return fail(kNoWaveformMessage);
    }
    data.timingOffsetMs = timingOffsetMs;
    data.waveforms = anlzWaveformsFrom(*pWaveform, data.frames, sampleRate);
    if (data.waveforms.detailEntries == 0) {
        return fail(kNoWaveformMessage);
    }
    gridFrom(track.getBeats(), data.frames, sampleRate, &data);

    bool hasHotCuesAboveH = false;
    std::optional<quint32> mainCueMs;
    // One snapshot of the cue list; each cue is read once.
    for (const CuePointer& pCue : track.getCuePoints()) {
        if (!pCue) {
            continue;
        }
        const CueType type = pCue->getType();
        if (type == CueType::MainCue) {
            const audio::FramePos position = pCue->getPosition();
            if (position.isValid()) {
                mainCueMs = anlz::msFromFrames(position, sampleRate, 0);
            }
            continue;
        }
        if (type != CueType::HotCue && type != CueType::Loop &&
                type != CueType::MemoryCue) {
            // Intro, Outro, N60dBSound, Beat, Jump.
            continue;
        }
        if (!pCue->getStartAndEndPosition().startPosition.isValid()) {
            // A cue with only an end has no time to write.
            continue;
        }
        const int hotCue = pCue->getHotCue();
        if (type != CueType::MemoryCue && hotCue != Cue::kNoHotCue) {
            // A loop in a hot cue slot is a hot cue with an end.
            if (hotCue >= kMaxHotCues) {
                hasHotCuesAboveH = true;
            } else {
                data.hotCues.append(cueFrom(*pCue, sampleRate, hotCue + 1));
            }
        } else if (type != CueType::HotCue) {
            data.memoryCues.append(cueFrom(*pCue, sampleRate, 0));
        }
    }

    // The cue point of the player: a memory cue without a comment or colour,
    // unless a memory cue is already there. A track that was never given a
    // main cue still reports position 0, so only a MainCue cue counts.
    if (mainCueMs.has_value()) {
        const bool alreadyThere = std::any_of(data.memoryCues.cbegin(),
                data.memoryCues.cend(),
                [&](const AnlzCue& cue) {
                    return isSameTime(cue.timeMs, *mainCueMs);
                });
        if (!alreadyThere) {
            AnlzCue mainCue;
            mainCue.timeMs = *mainCueMs;
            data.memoryCues.append(mainCue);
        }
    }

    std::stable_sort(data.hotCues.begin(),
            data.hotCues.end(),
            [](const AnlzCue& a, const AnlzCue& b) { return a.hotCue < b.hotCue; });
    std::stable_sort(data.memoryCues.begin(),
            data.memoryCues.end(),
            [](const AnlzCue& a, const AnlzCue& b) { return a.timeMs < b.timeMs; });

    const bool hasTooManyMemoryCues = limitMemoryCues(&data.memoryCues, mainCueMs);

    if (pWarnings) {
        if (hasHotCuesAboveH) {
            pWarnings->append(QStringLiteral(
                    "The track has hot cues above H. "
                    "Players show hot cues A to H only."));
        }
        if (hasTooManyMemoryCues) {
            pWarnings->append(QStringLiteral(
                    "The track has more than 10 memory cues. "
                    "The first 10 are written."));
        }
    }
    return data;
}

} // namespace rekordbox
} // namespace mixxx
