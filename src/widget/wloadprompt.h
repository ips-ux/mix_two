#pragma once

#include <QFrame>
#include <QList>

class LoadPrompt;
class QHBoxLayout;
class QPushButton;

/// On-screen "Load to the Deck" prompt for LoadPrompt: a small window over
/// the main window with one button per deck. The highlighted deck follows
/// the browse knob; clicking a deck loads the track there.
class WLoadPrompt : public QFrame {
    Q_OBJECT
  public:
    WLoadPrompt(LoadPrompt* pLoadPrompt, QWidget* pParent);

  public slots:
    /// Shows, hides and highlights according to the prompt's state.
    void slotUpdate();

  private:
    void rebuildDeckButtons(int numDecks);
    void centerOverParent();

    LoadPrompt* const m_pLoadPrompt;
    QHBoxLayout* m_pDeckLayout;
    QList<QPushButton*> m_deckButtons;
};
