#include <gtest/gtest.h>

#include <QFile>
#include <QStringList>
#include <QTextStream>
#include <vector>

#include "control/controlobject.h"
#include "test/signalpathtest.h"
#include "util/time.h"

namespace {

/// Mix Two: renders a scripted DDJ-800 jog movement through the real engine,
/// so the scratch sound can be compared with rekordbox
/// (tools/soundparity/render_scratch.py, SCRATCH-ENGINE.md).
///
/// Skipped unless these environment variables are set:
///   MIXTWO_RENDER_TRACK   the track to load into deck 1
///   MIXTWO_RENDER_JOG     the jog script: one DDJ-800 MIDI message per line,
///                         "<milliseconds> <status> <data1> <data2>" in hex
///                         bytes, e.g. "120.0 B0 22 43"; '#' starts a comment
///   MIXTWO_RENDER_OUT     the 32-bit float WAV to write (main output)
/// Optional:
///   MIXTWO_RENDER_ALGORITHM  scratch algorithm, 0 Mixxx, 1 Rekordbox (default)
///   MIXTWO_RENDER_START      start position in seconds of track (default 0)
///   MIXTWO_RENDER_SECONDS    length (default: last message + 2 s)
///   MIXTWO_RENDER_BUFFER     audio buffer in frames (default 128)
///   MIXTWO_RENDER_POSITIONS  optional CSV: per buffer the time (ms), the
///                            engine's exact track position (s),
///                            platter_state and the engine's speed, to
///                            compare with the jog ticks
///   MIXTWO_RENDER_JOG_RESOLUTION  jog steps per revolution (default 6480,
///                                 the mapping's jogResolution)
///
/// Deck 1 messages of the DDJ-800 are understood as its mapping
/// (res/controllers/Pioneer-DDJ-800-script.js, vinyl mode, scratch algorithm
/// "Rekordbox") turns them into controls: jog touch 90 36, jog turns B0 21 /
/// 22 / 23 (value - 0x40 steps, by default 6480 per revolution), PLAY 90 0B.
class PlatterRenderTest : public BaseSignalPathTest {
  protected:
    struct Message {
        double ms;
        int status;
        int data1;
        int data2;
    };

    static constexpr double kDefaultJogStepsPerRevolution = 6480.0;
    double m_jogStepsPerRevolution = kDefaultJogStepsPerRevolution;

    static QString env(const char* name) {
        return qEnvironmentVariable(name);
    }

    static bool parseJog(const QString& path, std::vector<Message>* pMessages) {
        QFile file(path);
        if (!file.open(QFile::ReadOnly | QFile::Text)) {
            return false;
        }
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine();
            const int comment = line.indexOf('#');
            if (comment >= 0) {
                line.truncate(comment);
            }
            const QStringList fields = line.simplified().split(' ', Qt::SkipEmptyParts);
            if (fields.isEmpty()) {
                continue;
            }
            if (fields.size() != 4) {
                return false;
            }
            bool ok[4];
            pMessages->push_back(Message{fields[0].toDouble(&ok[0]),
                    fields[1].toInt(&ok[1], 16),
                    fields[2].toInt(&ok[2], 16),
                    fields[3].toInt(&ok[3], 16)});
            if (!(ok[0] && ok[1] && ok[2] && ok[3])) {
                return false;
            }
        }
        return true;
    }

    /// What the DDJ-800 mapping does with one deck 1 message.
    void send(const Message& message) {
        const ConfigKey touchKey(m_sGroup1, QStringLiteral("platter_touch"));
        if (message.status == 0x90 && message.data1 == 0x36) {
            ControlObject::set(touchKey, message.data2 > 0 ? 1.0 : 0.0);
        } else if (message.status == 0x90 && message.data1 == 0x0B) {
            if (message.data2 > 0) {
                const ConfigKey playKey(m_sGroup1, QStringLiteral("play"));
                ControlObject::set(playKey, ControlObject::get(playKey) > 0 ? 0.0 : 1.0);
            }
        } else if (message.status == 0xB0 &&
                (message.data1 == 0x21 || message.data1 == 0x22 || message.data1 == 0x23)) {
            const int steps = message.data2 - 0x40;
            const bool toPlatter = ControlObject::get(touchKey) > 0 ||
                    ControlObject::get(ConfigKey(m_sGroup1, QStringLiteral("play"))) > 0 ||
                    ControlObject::get(ConfigKey(m_sGroup1, QStringLiteral("platter_state"))) > 0;
            if (toPlatter) {
                ControlObject::set(ConfigKey(m_sGroup1, QStringLiteral("platter_turn")),
                        steps / m_jogStepsPerRevolution);
            } else {
                ControlObject::set(ConfigKey(m_sGroup1, QStringLiteral("jog")), steps * 0.8);
            }
        }
    }

    static bool writeFloatWav(const QString& path,
            const std::vector<float>& samples,
            int channels,
            int sampleRate) {
        QFile file(path);
        if (!file.open(QFile::WriteOnly)) {
            return false;
        }
        const quint32 dataBytes = static_cast<quint32>(samples.size() * sizeof(float));
        QByteArray header;
        auto u32 = [&header](quint32 v) {
            header.append(reinterpret_cast<const char*>(&v), 4);
        };
        auto u16 = [&header](quint16 v) {
            header.append(reinterpret_cast<const char*>(&v), 2);
        };
        header.append("RIFF");
        u32(36 + dataBytes);
        header.append("WAVEfmt ");
        u32(16);
        u16(3); // IEEE float
        u16(static_cast<quint16>(channels));
        u32(static_cast<quint32>(sampleRate));
        u32(static_cast<quint32>(sampleRate * channels * sizeof(float)));
        u16(static_cast<quint16>(channels * sizeof(float)));
        u16(32);
        header.append("data");
        u32(dataBytes);
        static_assert(Q_BYTE_ORDER == Q_LITTLE_ENDIAN, "WAV is little-endian");
        return file.write(header) == header.size() &&
                file.write(reinterpret_cast<const char*>(samples.data()), dataBytes) ==
                dataBytes;
    }
};

TEST_F(PlatterRenderTest, RenderScriptedJog) {
    const QString trackPath = env("MIXTWO_RENDER_TRACK");
    const QString jogPath = env("MIXTWO_RENDER_JOG");
    const QString outPath = env("MIXTWO_RENDER_OUT");
    if (trackPath.isEmpty() || jogPath.isEmpty() || outPath.isEmpty()) {
        GTEST_SKIP() << "Set MIXTWO_RENDER_TRACK, MIXTWO_RENDER_JOG and MIXTWO_RENDER_OUT";
    }
    std::vector<Message> messages;
    ASSERT_TRUE(parseJog(jogPath, &messages)) << "Cannot read " << jogPath.toStdString();

    const QString algorithm = env("MIXTWO_RENDER_ALGORITHM");
    ControlObject::set(ConfigKey(QStringLiteral("[App]"), QStringLiteral("scratch_algorithm")),
            algorithm.isEmpty() ? 1.0 : algorithm.toDouble());

    loadTrack(m_pMixerDeck1.get(), TrackPointer(Track::newTemporary(trackPath)));
    // "duration" stays 0 here (temporary tracks are not parsed); the engine
    // knows the length.
    const double trackRate =
            ControlObject::get(ConfigKey(m_sGroup1, QStringLiteral("track_samplerate")));
    const double duration = trackRate > 0.0
            ? ControlObject::get(ConfigKey(m_sGroup1, QStringLiteral("track_samples"))) /
                    2.0 / trackRate
            : 0.0;
    ASSERT_GT(duration, 0.0) << "Cannot load " << trackPath.toStdString();
    const double start = env("MIXTWO_RENDER_START").toDouble();
    if (start > 0.0) {
        ControlObject::set(ConfigKey(m_sGroup1, QStringLiteral("playposition")),
                start / duration);
    }

    const int sampleRate = static_cast<int>(ControlObject::get(
            ConfigKey(QStringLiteral("[App]"), QStringLiteral("samplerate"))));
    ASSERT_GT(sampleRate, 0);
    const int bufferFrames = env("MIXTWO_RENDER_BUFFER").isEmpty()
            ? 128
            : env("MIXTWO_RENDER_BUFFER").toInt();
    ASSERT_GT(bufferFrames, 0);
    if (!env("MIXTWO_RENDER_JOG_RESOLUTION").isEmpty()) {
        m_jogStepsPerRevolution = env("MIXTWO_RENDER_JOG_RESOLUTION").toDouble();
    }
    ASSERT_GT(m_jogStepsPerRevolution, 0.0);
    double seconds = env("MIXTWO_RENDER_SECONDS").toDouble();
    if (seconds <= 0.0) {
        seconds = (messages.empty() ? 0.0 : messages.back().ms / 1000.0) + 2.0;
    }

    QFile positionsFile(env("MIXTWO_RENDER_POSITIONS"));
    QTextStream positions(&positionsFile);
    if (!positionsFile.fileName().isEmpty()) {
        ASSERT_TRUE(positionsFile.open(QFile::WriteOnly | QFile::Text))
                << "Cannot write " << positionsFile.fileName().toStdString();
        positions << "ms,position,platter_state,speed\n";
    }
    const ConfigKey stateKey(m_sGroup1, QStringLiteral("platter_state"));

    std::vector<float> out;
    out.reserve(static_cast<std::size_t>(seconds * sampleRate * 2) + bufferFrames * 2);
    std::size_t next = 0;
    double nowMs = 0.0;
    const double bufferMs = 1000.0 * bufferFrames / sampleRate;
    // The jog input stage times the turns by the clock (rekordbox's 1 ms jog
    // ticks), so the render runs on the test clock: each message arrives at
    // its own time, each buffer is processed at its start.
    mixxx::Time::setTestMode(true);
    double clockMs = 0.0;
    const auto advanceClockTo = [&clockMs](double ms) {
        if (ms > clockMs) {
            mixxx::Time::addTestTime(std::chrono::nanoseconds(std::llround((ms - clockMs) * 1e6)));
            clockMs = ms;
        }
    };
    while (nowMs < seconds * 1000.0) {
        // The messages that arrived during the previous buffer.
        while (next < messages.size() && messages[next].ms <= nowMs) {
            advanceClockTo(messages[next].ms);
            send(messages[next++]);
        }
        advanceClockTo(nowMs);
        m_pEngineMixer->process(bufferFrames * 2);
        const std::span<const CSAMPLE> buffer = m_pEngineMixer->getMainBuffer();
        out.insert(out.end(), buffer.begin(), buffer.begin() + bufferFrames * 2);
        if (positionsFile.isOpen()) {
            positions << QString::number(nowMs, 'f', 3) << ','
                      << QString::number(m_pChannel1->getEngineBuffer()->getPlayPos().value() /
                                         trackRate,
                                 'f',
                                 6)
                      << ',' << ControlObject::get(stateKey) << ','
                      << m_pChannel1->getEngineBuffer()->getSpeed() << '\n';
        }
        nowMs += bufferMs;
    }
    mixxx::Time::setTestMode(false);
    ASSERT_TRUE(writeFloatWav(outPath, out, 2, sampleRate))
            << "Cannot write " << outPath.toStdString();
}

} // namespace
