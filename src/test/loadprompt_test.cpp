#include "library/loadprompt.h"

#include <gtest/gtest.h>

#include <QSignalSpy>
#include <memory>

#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "test/mixxxtest.h"

namespace {

class LoadPromptTest : public MixxxTest {
  protected:
    void SetUp() override {
        m_pNumDecks = std::make_unique<ControlObject>(ConfigKey("[App]", "num_decks"));
        m_pNumDecks->set(2);
        m_pPlay1 = std::make_unique<ControlObject>(ConfigKey("[Channel1]", "play"));
        m_pPlay2 = std::make_unique<ControlObject>(ConfigKey("[Channel2]", "play"));
        m_pPrompt = std::make_unique<LoadPrompt>(nullptr, 50);
        m_pOpen = std::make_unique<ControlProxy>("[Library]", "load_prompt");
        m_pDeck = std::make_unique<ControlProxy>("[Library]", "load_prompt_deck");
        m_pPush = std::make_unique<ControlProxy>("[Library]", "load_prompt_push");
        m_pSelect = std::make_unique<ControlProxy>("[Library]", "load_prompt_select");
        m_pLoad = std::make_unique<ControlProxy>("[Library]", "load_prompt_load");
        m_pCancel = std::make_unique<ControlProxy>("[Library]", "load_prompt_cancel");
    }

    void TearDown() override {
        // Proxies before the controls they point to.
        m_pOpen.reset();
        m_pDeck.reset();
        m_pPush.reset();
        m_pSelect.reset();
        m_pLoad.reset();
        m_pCancel.reset();
        m_pPrompt.reset();
    }

    void push() {
        m_pPush->set(1);
        m_pPush->set(0);
    }

    std::unique_ptr<ControlObject> m_pNumDecks;
    std::unique_ptr<ControlObject> m_pPlay1;
    std::unique_ptr<ControlObject> m_pPlay2;
    std::unique_ptr<LoadPrompt> m_pPrompt;
    std::unique_ptr<ControlProxy> m_pOpen;
    std::unique_ptr<ControlProxy> m_pDeck;
    std::unique_ptr<ControlProxy> m_pPush;
    std::unique_ptr<ControlProxy> m_pSelect;
    std::unique_ptr<ControlProxy> m_pLoad;
    std::unique_ptr<ControlProxy> m_pCancel;
};

TEST_F(LoadPromptTest, PushOpensOnFirstStoppedDeck) {
    m_pPlay1->set(1);
    push();
    EXPECT_TRUE(m_pOpen->toBool());
    EXPECT_EQ(2.0, m_pDeck->get());
}

TEST_F(LoadPromptTest, TurnAndPushLoadsHighlightedDeck) {
    QSignalSpy spy(m_pPrompt.get(), &LoadPrompt::loadRequested);
    push();
    EXPECT_EQ(1.0, m_pDeck->get());

    m_pSelect->set(1);
    EXPECT_EQ(2.0, m_pDeck->get());
    // Wraps around in both directions.
    m_pSelect->set(1);
    EXPECT_EQ(1.0, m_pDeck->get());
    m_pSelect->set(-1);
    EXPECT_EQ(2.0, m_pDeck->get());

    push();
    EXPECT_FALSE(m_pOpen->toBool());
    ASSERT_EQ(1, spy.count());
    EXPECT_EQ(2, spy.at(0).at(0).toInt());
}

TEST_F(LoadPromptTest, LoadButtonPicksDeck) {
    QSignalSpy spy(m_pPrompt.get(), &LoadPrompt::loadRequested);
    push();

    // There is no deck 3.
    m_pLoad->set(3);
    EXPECT_TRUE(m_pOpen->toBool());
    EXPECT_EQ(0, spy.count());

    m_pLoad->set(2);
    EXPECT_FALSE(m_pOpen->toBool());
    ASSERT_EQ(1, spy.count());
    EXPECT_EQ(2, spy.at(0).at(0).toInt());
}

TEST_F(LoadPromptTest, TurningWhileClosedDoesNothing) {
    m_pSelect->set(1);
    EXPECT_FALSE(m_pOpen->toBool());
    EXPECT_EQ(1.0, m_pDeck->get());
}

TEST_F(LoadPromptTest, CancelAndTimeoutClose) {
    QSignalSpy spy(m_pPrompt.get(), &LoadPrompt::loadRequested);
    push();
    m_pCancel->set(1);
    EXPECT_FALSE(m_pOpen->toBool());

    push();
    ASSERT_TRUE(m_pOpen->toBool());
    // Closes by itself after the (50 ms) timeout.
    QSignalSpy stateSpy(m_pPrompt.get(), &LoadPrompt::stateChanged);
    ASSERT_TRUE(stateSpy.wait(2000));
    EXPECT_FALSE(m_pOpen->toBool());
    EXPECT_EQ(0, spy.count());
}

} // namespace
