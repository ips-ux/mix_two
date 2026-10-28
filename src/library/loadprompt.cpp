#include "library/loadprompt.h"

#include <algorithm>

#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "control/controlpushbutton.h"
#include "mixer/playermanager.h"
#include "moc_loadprompt.cpp"

namespace {

const QString kGroup = QStringLiteral("[Library]");

} // namespace

LoadPrompt::LoadPrompt(QObject* pParent, int timeoutMillis)
        : QObject(pParent),
          m_pOpen(std::make_unique<ControlObject>(ConfigKey(kGroup, "load_prompt"))),
          m_pDeck(std::make_unique<ControlObject>(ConfigKey(kGroup, "load_prompt_deck"))),
          m_pPush(std::make_unique<ControlPushButton>(ConfigKey(kGroup, "load_prompt_push"))),
          m_pSelect(std::make_unique<ControlObject>(ConfigKey(kGroup, "load_prompt_select"))),
          m_pLoad(std::make_unique<ControlObject>(ConfigKey(kGroup, "load_prompt_load"))),
          m_pCancel(std::make_unique<ControlPushButton>(
                  ConfigKey(kGroup, "load_prompt_cancel"))),
          m_pNumDecks(std::make_unique<ControlProxy>(
                  ConfigKey(QStringLiteral("[App]"), QStringLiteral("num_decks")),
                  this,
                  ControlFlag::NoAssertIfMissing)),
          m_timeout(this) {
    m_pOpen->setReadOnly();
    m_pDeck->setReadOnly();
    m_pDeck->forceSet(1);

    connect(m_pPush.get(), &ControlObject::valueChanged, this, &LoadPrompt::slotPush);
    connect(m_pSelect.get(), &ControlObject::valueChanged, this, &LoadPrompt::slotSelect);
    connect(m_pLoad.get(), &ControlObject::valueChanged, this, &LoadPrompt::slotLoad);
    connect(m_pCancel.get(), &ControlObject::valueChanged, this, &LoadPrompt::slotCancel);

    m_timeout.setSingleShot(true);
    m_timeout.setInterval(timeoutMillis);
    connect(&m_timeout, &QTimer::timeout, this, &LoadPrompt::close);
}

LoadPrompt::~LoadPrompt() = default;

bool LoadPrompt::isOpen() const {
    return m_pOpen->toBool();
}

int LoadPrompt::selectedDeck() const {
    return static_cast<int>(m_pDeck->get());
}

int LoadPrompt::numDecks() const {
    return std::max(1, static_cast<int>(m_pNumDecks->get()));
}

void LoadPrompt::slotPush(double value) {
    if (value <= 0) {
        return;
    }
    if (isOpen()) {
        const int deckNumber = selectedDeck();
        close();
        emit loadRequested(deckNumber);
    } else {
        open();
    }
}

void LoadPrompt::slotSelect(double value) {
    // Relative control: act on the value, then reset it.
    const int steps = static_cast<int>(value);
    if (steps == 0) {
        return;
    }
    m_pSelect->set(0);
    if (!isOpen()) {
        return;
    }
    const int count = numDecks();
    // Wrap around in both directions.
    const int index = ((selectedDeck() - 1 + steps) % count + count) % count;
    setSelectedDeck(index + 1);
    m_timeout.start();
}

void LoadPrompt::slotLoad(double value) {
    const int deckNumber = static_cast<int>(value);
    if (deckNumber <= 0) {
        return;
    }
    m_pLoad->set(0);
    if (deckNumber > numDecks()) {
        return;
    }
    close();
    emit loadRequested(deckNumber);
}

void LoadPrompt::slotCancel(double value) {
    if (value > 0) {
        close();
    }
}

void LoadPrompt::open() {
    setSelectedDeck(defaultDeck());
    m_pOpen->forceSet(1);
    m_timeout.start();
    emit stateChanged();
}

void LoadPrompt::close() {
    m_timeout.stop();
    if (!isOpen()) {
        return;
    }
    m_pOpen->forceSet(0);
    emit stateChanged();
}

void LoadPrompt::setSelectedDeck(int deckNumber) {
    if (deckNumber == selectedDeck()) {
        return;
    }
    m_pDeck->forceSet(deckNumber);
    emit stateChanged();
}

int LoadPrompt::defaultDeck() const {
    for (int i = 0; i < numDecks(); ++i) {
        const ConfigKey playKey(PlayerManager::groupForDeck(i), QStringLiteral("play"));
        if (ControlObject::exists(playKey) && !ControlObject::get(playKey)) {
            return i + 1;
        }
    }
    return 1;
}
