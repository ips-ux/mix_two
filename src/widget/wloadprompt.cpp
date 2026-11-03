#include "widget/wloadprompt.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "control/controlobject.h"
#include "library/loadprompt.h"
#include "moc_wloadprompt.cpp"

WLoadPrompt::WLoadPrompt(LoadPrompt* pLoadPrompt, QWidget* pParent)
        : QFrame(pParent,
                  Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus),
          m_pLoadPrompt(pLoadPrompt),
          m_pDeckLayout(new QHBoxLayout()) {
    setObjectName(QStringLiteral("LoadPrompt"));
    // The DJ keeps working on the controller; do not take the focus.
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFrameShape(QFrame::StyledPanel);
    // Default look; skins can override it via #LoadPrompt.
    setStyleSheet(QStringLiteral(
            "#LoadPrompt { background: #1b1b1b; border: 1px solid #555; }"
            "#LoadPrompt QLabel { color: #e6e6e6; font-weight: bold; }"
            "#LoadPrompt QPushButton { color: #e6e6e6; background: #2c2c2c;"
            "  border: 1px solid #444; padding: 10px 22px; }"
            "#LoadPrompt QPushButton:checked { background: #0f6fd1;"
            "  border-color: #3d9bff; }"));

    auto* pLayout = new QVBoxLayout(this);
    auto* pTitle = new QLabel(tr("Load to the Deck"), this);
    pTitle->setAlignment(Qt::AlignHCenter);
    pLayout->addWidget(pTitle);
    pLayout->addLayout(m_pDeckLayout);

    connect(m_pLoadPrompt, &LoadPrompt::stateChanged, this, &WLoadPrompt::slotUpdate);
    hide();
}

void WLoadPrompt::slotUpdate() {
    if (!m_pLoadPrompt->isOpen()) {
        hide();
        return;
    }
    const int numDecks = m_pLoadPrompt->numDecks();
    if (m_deckButtons.size() != numDecks) {
        rebuildDeckButtons(numDecks);
    }
    const int selectedDeck = m_pLoadPrompt->selectedDeck();
    for (int i = 0; i < m_deckButtons.size(); ++i) {
        m_deckButtons.at(i)->setChecked(i + 1 == selectedDeck);
    }
    if (!isVisible()) {
        adjustSize();
        centerOverParent();
        show();
        raise();
    }
}

void WLoadPrompt::rebuildDeckButtons(int numDecks) {
    qDeleteAll(m_deckButtons);
    m_deckButtons.clear();
    for (int deckNumber = 1; deckNumber <= numDecks; ++deckNumber) {
        // rekordbox's deck names
        auto* pButton = new QPushButton(tr("Deck%1").arg(deckNumber), this);
        pButton->setCheckable(true);
        pButton->setFocusPolicy(Qt::NoFocus);
        connect(pButton, &QPushButton::clicked, this, [deckNumber] {
            ControlObject::set(
                    ConfigKey(QStringLiteral("[Library]"), QStringLiteral("load_prompt_load")),
                    deckNumber);
        });
        m_pDeckLayout->addWidget(pButton);
        m_deckButtons.append(pButton);
    }
}

void WLoadPrompt::centerOverParent() {
    const QWidget* pParent = parentWidget();
    if (!pParent) {
        return;
    }
    const QRect parentRect = pParent->window()->frameGeometry();
    move(parentRect.center() - rect().center());
}
