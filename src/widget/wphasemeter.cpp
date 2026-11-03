#include "widget/wphasemeter.h"

#include <QPainter>
#include <QStyleOption>
#include <QStylePainter>
#include <algorithm>
#include <cmath>
#include <limits>

#include "moc_wphasemeter.cpp"
#include "skin/legacy/skincontext.h"

namespace {
constexpr double kDefaultTolerance = 0.02;
constexpr int kUpdateIntervalMs = 33; // about 30 Hz
const QString kAppGroup = QStringLiteral("[App]");
const QString kClockGroup = QStringLiteral("[InternalClock]");
const QString kSyncLeader = QStringLiteral("sync_leader");
const QString kBeatDistance = QStringLiteral("beat_distance");
} // namespace

namespace mixxx {

double phaseOffset(double deckBeatDistance, double leaderBeatDistance) {
    double offset = deckBeatDistance - leaderBeatDistance;
    // Wrap into [-0.5, +0.5).
    offset -= std::floor(offset + 0.5);
    return offset;
}

} // namespace mixxx

WPhaseMeter::Deck::Deck(const QString& deckGroup, QObject* pParent)
        : leader(deckGroup, kSyncLeader, pParent),
          beatDistance(deckGroup, kBeatDistance, pParent) {
}

WPhaseMeter::WPhaseMeter(const QString& group, QWidget* pParent)
        : WWidget(pParent),
          m_group(group),
          m_trackLoaded(group, QStringLiteral("track_loaded"), this),
          m_beatDistance(group, kBeatDistance, this),
          m_clockLeader(kClockGroup, kSyncLeader, this),
          m_clockBeatDistance(kClockGroup, kBeatDistance, this),
          m_tolerance(kDefaultTolerance),
          m_inPhaseColor(QStringLiteral("#2f8cff")),
          m_outOfPhaseColor(QStringLiteral("#ff5a2a")),
          m_centerColor(QStringLiteral("#666666")),
          m_drawnActive(false),
          m_drawnPixels(0),
          m_drawnInPhase(true) {
    // The skin is rebuilt when the number of decks changes, so one read is
    // enough.
    const ControlProxy numDecks(kAppGroup, QStringLiteral("num_decks"));
    const int count = static_cast<int>(numDecks.get());
    m_decks.reserve(count);
    for (int i = 1; i <= count; ++i) {
        m_decks.push_back(std::make_unique<Deck>(
                QStringLiteral("[Channel%1]").arg(i), this));
    }
    connect(&m_timer, &QTimer::timeout, this, &WPhaseMeter::update30Hz);
    m_timer.start(kUpdateIntervalMs);
}

void WPhaseMeter::setup(const QDomNode& node, const SkinContext& context) {
    m_tolerance = context.selectDouble(node, QStringLiteral("Tolerance"), kDefaultTolerance);
}

double WPhaseMeter::currentOffset() const {
    constexpr double kNone = std::numeric_limits<double>::quiet_NaN();
    if (!m_trackLoaded.toBool()) {
        return kNone;
    }
    for (const auto& pDeck : m_decks) {
        if (pDeck->leader.get() > 0.0) {
            if (pDeck->leader.getKey().group == m_group) {
                return kNone; // this deck is the leader
            }
            return mixxx::phaseOffset(m_beatDistance.get(), pDeck->beatDistance.get());
        }
    }
    if (m_clockLeader.get() > 0.0) {
        return mixxx::phaseOffset(m_beatDistance.get(), m_clockBeatDistance.get());
    }
    return kNone;
}

void WPhaseMeter::update30Hz() {
    const double offset = currentOffset();
    const bool active = !std::isnan(offset);
    int pixels = 0;
    bool inPhase = true;
    if (active) {
        pixels = static_cast<int>(std::lround(offset * width()));
        inPhase = std::abs(offset) <= m_tolerance;
    }
    if (active != m_drawnActive || pixels != m_drawnPixels || inPhase != m_drawnInPhase) {
        m_drawnActive = active;
        m_drawnPixels = pixels;
        m_drawnInPhase = inPhase;
        update();
    }
}

void WPhaseMeter::paintEvent(QPaintEvent* pEvent) {
    Q_UNUSED(pEvent);
    QStyleOption option;
    option.initFrom(this);
    QStylePainter p(this);
    p.drawPrimitive(QStyle::PE_Widget, option);

    const int w = width();
    const int h = height();
    const int centerX = w / 2;
    if (m_drawnActive && m_drawnPixels != 0) {
        const QColor color = m_drawnInPhase ? m_inPhaseColor : m_outOfPhaseColor;
        const int barH = std::max(2, h - 6);
        const int barY = (h - barH) / 2;
        const int x = m_drawnPixels > 0 ? centerX : centerX + m_drawnPixels;
        p.fillRect(x, barY, std::abs(m_drawnPixels), barH, color);
    }
    p.fillRect(centerX, 1, 1, std::max(1, h - 2), m_centerColor);
}
