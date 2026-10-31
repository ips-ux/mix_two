#pragma once

#include "widget/wpushbutton.h"

class EffectsManager;
class QPoint;

/// Mix Two: one of the four Sound Color FX buttons ([SoundColorFX],cfxN).
/// A left click works as on any push button. A right click opens a menu to
/// select the QuickEffect chain preset that the button loads.
class WSoundColorFxButton : public WPushButton {
    Q_OBJECT
  public:
    WSoundColorFxButton(QWidget* pParent, EffectsManager* pEffectsManager);

    void setup(const QDomNode& node, const SkinContext& context) override;

  protected:
    void mousePressEvent(QMouseEvent* pEvent) override;
    void mouseReleaseEvent(QMouseEvent* pEvent) override;

  private:
    void showPresetMenu(const QPoint& globalPos);

    EffectsManager* m_pEffectsManager;
    // 0..3 = DUB ECHO, SWEEP, NOISE, FILTER; -1 until setup() reads it.
    int m_index;
};
