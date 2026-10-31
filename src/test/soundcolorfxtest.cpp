// Mix Two: the Sound Color FX press rules and the combined QuickEffect chain
// (EffectsManager::pressSoundColorFx()).
#include <gtest/gtest.h>

#include <QCoreApplication>

#include "control/controlobject.h"
#include "effects/chains/quickeffectchain.h"
#include "effects/effectsmanager.h"
#include "engine/channelhandle.h"
#include "test/mixxxtest.h"

namespace {

const QString kGroup = QStringLiteral("[SoundColorFX]");
const QString kDeck = QStringLiteral("[Channel1]");

class SoundColorFxTest : public MixxxTest {
  protected:
    void SetUp() override {
        auto pChannelHandleFactory = std::make_shared<ChannelHandleFactory>();
        m_pEffectsManager = std::make_shared<EffectsManager>(config(), pChannelHandleFactory);
        const QString mainGroup = QStringLiteral("[MasterOutput]");
        m_pEffectsManager->registerInputChannel(ChannelHandleAndGroup(
                pChannelHandleFactory->getOrCreateHandle(mainGroup), mainGroup));
        const QString mainOut = QStringLiteral("[Master]");
        m_pEffectsManager->registerOutputChannel(ChannelHandleAndGroup(
                pChannelHandleFactory->getOrCreateHandle(mainOut), mainOut));
        const ChannelHandleAndGroup deck(pChannelHandleFactory->getOrCreateHandle(kDeck), kDeck);
        m_pEffectsManager->registerInputChannel(deck);
        m_pEffectsManager->addDeck(deck);
        m_pEffectsManager->setup();
    }

    void TearDown() override {
        m_pEffectsManager.reset();
    }

    QString state() const {
        QString result;
        for (int i = 1; i <= EffectsManager::kNumSoundColorFx; ++i) {
            result += ControlObject::get(ConfigKey(kGroup, QStringLiteral("cfx%1").arg(i))) > 0
                    ? QChar('1')
                    : QChar('0');
        }
        return result;
    }

    void press(int index, bool shift) {
        m_pEffectsManager->pressSoundColorFx(index, shift);
        QCoreApplication::processEvents();
    }

    double quickEffectEnabled() const {
        return ControlObject::get(ConfigKey(
                QuickEffectChain::formatEffectChainGroup(kDeck), QStringLiteral("enabled")));
    }

    std::shared_ptr<EffectsManager> m_pEffectsManager;
};

TEST_F(SoundColorFxTest, PressRules) {
    press(0, false); // DUB ECHO
    EXPECT_EQ(QStringLiteral("1000"), state());
    press(1, true); // SHIFT + SWEEP: both
    EXPECT_EQ(QStringLiteral("1100"), state());
    press(0, true); // SHIFT + DUB ECHO: off, SWEEP stays
    EXPECT_EQ(QStringLiteral("0100"), state());
    press(2, false); // NOISE: only NOISE
    EXPECT_EQ(QStringLiteral("0010"), state());
    press(3, true); // SHIFT + FILTER: NOISE and FILTER
    EXPECT_EQ(QStringLiteral("0011"), state());
    press(2, false); // NOISE again (on), no SHIFT: all off
    EXPECT_EQ(QStringLiteral("0000"), state());
    EXPECT_EQ(0.0, quickEffectEnabled());
}

TEST_F(SoundColorFxTest, SeveralEffectsShareTheChain) {
    m_pEffectsManager->setSoundColorFxPreset(2, QStringLiteral("White Noise"));
    m_pEffectsManager->setSoundColorFxPreset(3, QStringLiteral("Filter"));
    press(2, false);
    press(3, true);
    EXPECT_EQ(1.0, quickEffectEnabled());
    const auto pChain = m_pEffectsManager->getQuickEffectChain(kDeck);
    ASSERT_TRUE(pChain);
    EXPECT_EQ(QStringLiteral("NOISE + FILTER"), pChain->presetName());
    press(3, true); // SHIFT + FILTER off: NOISE alone, its own preset
    EXPECT_EQ(QStringLiteral("White Noise"), pChain->presetName());
    EXPECT_EQ(1.0, quickEffectEnabled());
}

} // namespace
