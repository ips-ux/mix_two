// AI-generated (Claude Code)
#pragma once

#include <memory>

#include "widget/wpushbutton.h"

class AudioTargetManager;
class ControlProxy;
class QPoint;

/// Mix Two: one of the four WASAPI microphone squares (MicInputButton skin
/// tag, MicInput 1-4). The square shows the state of
/// PlayerManager::groupForMicrophone(), and [Skin],mic_selected marks the
/// selected one.
///
/// A left click selects the square. On a lit square it switches talk-over
/// off. Else, if the square has an input, it switches talk-over on; if not,
/// it opens the input menu. A right click selects the
/// square and opens the menu. Without an AudioTargetManager the menu does not
/// open.
///
/// The On state (QSS displayValue) needs the skin to give the widget
/// <NumberStates>2</NumberStates> and a <Connection> to
/// [MicrophoneN],talkover.
class WMicInputButton : public WPushButton {
    Q_OBJECT
    Q_PROPERTY(bool assigned READ isAssigned NOTIFY assignedChanged)
    Q_PROPERTY(bool selected READ isSelected NOTIFY selectedChanged)
  public:
    WMicInputButton(QWidget* pParent, AudioTargetManager* pTargets);
    ~WMicInputButton() override;

    void setup(const QDomNode& node, const SkinContext& context) override;

    bool isAssigned() const {
        return m_assigned;
    }
    bool isSelected() const {
        return m_selected;
    }

  signals:
    void assignedChanged(bool assigned);
    void selectedChanged(bool selected);

  protected:
    void mousePressEvent(QMouseEvent* pEvent) override;
    void mouseReleaseEvent(QMouseEvent* pEvent) override;

  private slots:
    void slotStateChanged();

  private:
    void showInputMenu(const QPoint& globalPos);
    bool deckIsPlaying() const;

    AudioTargetManager* m_pTargets;
    // 0..3 for MIC 1..4; -1 until setup() reads it.
    int m_index;
    bool m_assigned;
    bool m_selected;
    std::unique_ptr<ControlProxy> m_pInputConfigured;
    std::unique_ptr<ControlProxy> m_pTalkover;
    std::unique_ptr<ControlProxy> m_pSelectedMic;
};

// End of AI-generated text
