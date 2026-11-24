// AI-generated (Claude Code). Start of AI-generated text.
#include <gtest/gtest.h>

#include "soundio/audiotargetmanager.h"
#include "soundio/soundmanagerconfig.h"
#include "test/mixxxtest.h"

namespace {

class AudioTargetManagerTest : public MixxxTest {
  protected:
    // The frames per buffer that a config gets with `rate` and `index`.
    static unsigned int framesFor(mixxx::audio::SampleRate rate, unsigned int index) {
        SoundManagerConfig config(nullptr);
        config.setSampleRate(rate);
        config.setAudioBufferSizeIndex(index);
        return config.getFramesPerBuffer();
    }
};

TEST_F(AudioTargetManagerTest, HardwareBufferIs256FramesAt44100) {
    const mixxx::audio::SampleRate rate(44100);
    const unsigned int index = AudioTargetManager::bufferSizeIndexFor(
            AudioTargetManager::BufferTarget::Hardware,
            rate,
            SoundManagerConfig::kDefaultAudioBufferSizeIndex);
    EXPECT_EQ(256u, framesFor(rate, index));
}

TEST_F(AudioTargetManagerTest, HardwareBufferIs256FramesAt48000) {
    const mixxx::audio::SampleRate rate(48000);
    const unsigned int index = AudioTargetManager::bufferSizeIndexFor(
            AudioTargetManager::BufferTarget::Hardware,
            rate,
            SoundManagerConfig::kDefaultAudioBufferSizeIndex);
    EXPECT_EQ(256u, framesFor(rate, index));
}

TEST_F(AudioTargetManagerTest, HardwareBufferIgnoresTheCurrentIndex) {
    const mixxx::audio::SampleRate rate(44100);
    for (unsigned int current = 1; current <= SoundManagerConfig::kMaxAudioBufferSizeIndex;
            ++current) {
        const unsigned int index = AudioTargetManager::bufferSizeIndexFor(
                AudioTargetManager::BufferTarget::Hardware, rate, current);
        EXPECT_EQ(256u, framesFor(rate, index)) << "current index " << current;
    }
}

TEST_F(AudioTargetManagerTest, PcBufferKeepsTheCurrentIndex) {
    for (unsigned int current = 1; current <= SoundManagerConfig::kMaxAudioBufferSizeIndex;
            ++current) {
        EXPECT_EQ(current,
                AudioTargetManager::bufferSizeIndexFor(
                        AudioTargetManager::BufferTarget::Pc,
                        mixxx::audio::SampleRate(48000),
                        current));
    }
}

TEST_F(AudioTargetManagerTest, HardwareBufferIs256FramesAt96000) {
    const mixxx::audio::SampleRate rate(96000);
    const unsigned int index = AudioTargetManager::bufferSizeIndexFor(
            AudioTargetManager::BufferTarget::Hardware,
            rate,
            SoundManagerConfig::kDefaultAudioBufferSizeIndex);
    EXPECT_EQ(256u, framesFor(rate, index));
}

} // namespace
// End of AI-generated text.
