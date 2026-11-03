#pragma once

#include <QColor>
#include <QString>
#include <QTimer>
#include <memory>
#include <vector>

#include "control/controlproxy.h"
#include "widget/wwidget.h"

class QDomNode;
class SkinContext;

namespace mixxx {

// AI-generated (Claude Code). Start of AI-generated text.
/// The phase offset of a deck to the leader, as a fraction of a beat in
/// [-0.5, +0.5). Both arguments are beat distances in [0, 1). A positive
/// value means the deck is ahead of the leader, a negative value behind.
// End of AI-generated text.
double phaseOffset(double deckBeatDistance, double leaderBeatDistance);

} // namespace mixxx

// AI-generated (Claude Code). Start of AI-generated text.
/// A centred needle meter for the phase of a deck to the sync leader. The bar
/// grows to the left when the deck is behind and to the right when it is
/// ahead. It is blue inside the tolerance and red-orange outside it. Only the
/// centre line shows when there is no leader, when this deck is the leader or
/// when no track is loaded. It reads its controls on the GUI thread.
// End of AI-generated text.
class WPhaseMeter : public WWidget {
    Q_OBJECT
    Q_PROPERTY(QColor inPhaseColor MEMBER m_inPhaseColor DESIGNABLE true)
    Q_PROPERTY(QColor outOfPhaseColor MEMBER m_outOfPhaseColor DESIGNABLE true)
    Q_PROPERTY(QColor centerColor MEMBER m_centerColor DESIGNABLE true)

  public:
    WPhaseMeter(const QString& group, QWidget* pParent = nullptr);

    void setup(const QDomNode& node, const SkinContext& context);

  protected:
    void paintEvent(QPaintEvent* pEvent) override;

  private slots:
    void update30Hz();

  private:
    struct Deck {
        Deck(const QString& deckGroup, QObject* pParent);
        ControlProxy leader;
        ControlProxy beatDistance;
    };

    // Returns the offset to draw, or NaN when only the centre line shows.
    double currentOffset() const;

    const QString m_group;
    ControlProxy m_trackLoaded;
    ControlProxy m_beatDistance;
    ControlProxy m_clockLeader;
    ControlProxy m_clockBeatDistance;
    // Sized once in the constructor.
    std::vector<std::unique_ptr<Deck>> m_decks;
    QTimer m_timer;

    double m_tolerance;
    QColor m_inPhaseColor;
    QColor m_outOfPhaseColor;
    QColor m_centerColor;

    // What was drawn last: whether the needle shows, its length in pixels
    // (sign = side) and whether it is inside the tolerance.
    bool m_drawnActive;
    int m_drawnPixels;
    bool m_drawnInPhase;
};
