#pragma once

#include <QObject>
#include <QTimer>
#include <memory>

class ControlObject;
class ControlPushButton;
class ControlProxy;

/// The "Load to the Deck" prompt of rekordbox's "Rotary Selector (Push)"
/// load method: pushing the browse knob on a track asks which deck to load
/// it into. The deck is picked with a deck's LOAD button, by clicking it in
/// the prompt, or by turning the knob and pushing it again.
///
/// Controls in [Library]:
/// - load_prompt (read-only): 1 while the prompt is open
/// - load_prompt_deck (read-only): the highlighted deck, 1-based
/// - load_prompt_push: opens the prompt, or loads into the highlighted deck
///   if it is open
/// - load_prompt_select: moves the highlight by the value (e.g. +1 / -1)
/// - load_prompt_load: loads into the deck with this number and closes
/// - load_prompt_cancel: closes the prompt
class LoadPrompt : public QObject {
    Q_OBJECT
  public:
    /// The prompt closes after this long without input.
    static constexpr int kDefaultTimeoutMillis = 5000;

    explicit LoadPrompt(QObject* pParent = nullptr, int timeoutMillis = kDefaultTimeoutMillis);
    ~LoadPrompt() override;

    bool isOpen() const;
    int selectedDeck() const;
    int numDecks() const;

  signals:
    /// Load the selected library track into the deck (1-based).
    void loadRequested(int deckNumber);
    void stateChanged();

  private slots:
    void slotPush(double value);
    void slotSelect(double value);
    void slotLoad(double value);
    void slotCancel(double value);

  private:
    void open();
    void close();
    void setSelectedDeck(int deckNumber);
    /// The first deck that is not playing, else deck 1.
    int defaultDeck() const;

    std::unique_ptr<ControlObject> m_pOpen;
    std::unique_ptr<ControlObject> m_pDeck;
    std::unique_ptr<ControlPushButton> m_pPush;
    std::unique_ptr<ControlObject> m_pSelect;
    std::unique_ptr<ControlObject> m_pLoad;
    std::unique_ptr<ControlPushButton> m_pCancel;
    std::unique_ptr<ControlProxy> m_pNumDecks;
    QTimer m_timeout;
};
