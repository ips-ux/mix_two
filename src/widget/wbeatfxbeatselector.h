#pragma once

#include <QString>

#include "widget/wwidget.h"

class ControlProxy;

/// Mix Two: the BEAT fraction of a Beat FX slot, shown as "◀  1/2  ▶".
/// The arrows step the slot's first parameter (the Mix Two BEAT FX put
/// the beat fraction there, 9 steps from 1/16 to 16, as the DDJ-800's
/// BEAT buttons); a click on the value opens the list of fractions.
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
    int index() const;
    void setIndex(int index);
    QString label(int index) const;

    ControlProxy* m_pBeat;
    double m_value;
};
