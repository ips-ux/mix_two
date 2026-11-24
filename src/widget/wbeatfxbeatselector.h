#pragma once

#include <QString>

#include "widget/wwidget.h"

class ControlProxy;

/// Mix Two: the BEAT fraction of a Beat FX slot, shown as "◀  1/2  ▶".
/// The arrows step the slot's first parameter (the Mix Two BEAT FX put
/// the beat fraction there, as the DDJ-800's BEAT buttons); a click on the
/// value opens the list of fractions. The list follows the loaded effect:
/// 9 steps (1/16 to 16 beats), 11 steps (1/16 to 64 beats) or 22 steps
/// (-64 to 64 beats, negative values with a leading minus).
class WBeatFxBeatSelector : public WWidget {
    Q_OBJECT
  public:
    explicit WBeatFxBeatSelector(QWidget* pParent = nullptr);

    void setup(const QDomNode& node, const SkinContext& context);

  protected:
    void paintEvent(QPaintEvent* pEvent) override;
    void mousePressEvent(QMouseEvent* pEvent) override;

  private slots:
    void slotValueChanged(double value);

  private:
    int stepCount() const;
    int index() const;
    void setIndex(int index);
    QString label(int index) const;

    ControlProxy* m_pBeat;
    double m_value;
};
