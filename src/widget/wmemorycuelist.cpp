#include "widget/wmemorycuelist.h"

#include <QContextMenuEvent>
#include <QEvent>
#include <QHeaderView>
#include <QMouseEvent>
#include <QIcon>
#include <QPixmap>
#include <QTableWidgetItem>
#include <algorithm>
#include <cmath>
#include <utility>

#include "control/controlproxy.h"
#include "moc_wmemorycuelist.cpp"
#include "track/track.h"
#include "util/color/rgbcolor.h"
#include "util/defs.h"

// AI-generated (Claude Code). Start of AI-generated text.
// WMemoryCueList: written by Claude Code. All comments in this file are
// AI-generated text.
// End of AI-generated text.

namespace {
const QString kActivateKey = QStringLiteral("memory_cue_%1_activate");
constexpr int kRowHeight = 16;
constexpr int kSwatchWidth = 4;
// Item data of the comment item: the address of its Cue, so that an edit
// writes to that cue even when the rows moved.
constexpr int kCueRole = Qt::UserRole + 1;
} // namespace

namespace mixxx {

QString formatMemoryCueTime(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0) {
        return QString();
    }
    const auto totalMillis = static_cast<qint64>(std::llround(seconds * 1000.0));
    const qint64 minutes = totalMillis / 60000;
    const qint64 secs = (totalMillis / 1000) % 60;
    const qint64 millis = totalMillis % 1000;
    return QStringLiteral("%1:%2.%3")
            .arg(minutes, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'))
            .arg(millis, 3, 10, QChar('0'));
}

} // namespace mixxx

WMemoryCueList::WMemoryCueList(const QString& group, QWidget* pParent)
        : QTableWidget(0, kColumnCount, pParent),
          WBaseWidget(this),
          m_group(group),
          m_activateControls(kMaxNumberOfMemoryCueMarks),
          m_updating(false),
          m_rebuildPending(false) {
    setHorizontalHeaderLabels({tr("TIME"), tr("LOOP"), tr("COMMENT")});
    verticalHeader()->hide();
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader()->setMinimumSectionSize(kRowHeight);
    verticalHeader()->setDefaultSectionSize(kRowHeight);
    setIconSize(QSize(kSwatchWidth, kRowHeight - 4));
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    horizontalHeader()->setSectionResizeMode(kColumnTime, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(kColumnLoop, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(kColumnComment, QHeaderView::Stretch);
    horizontalHeader()->setHighlightSections(false);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::DoubleClicked);
    setShowGrid(false);
    setWordWrap(false);
    setFocusPolicy(Qt::ClickFocus);

    connect(this, &QTableWidget::cellClicked, this, &WMemoryCueList::slotCellClicked);
    connect(this, &QTableWidget::itemChanged, this, &WMemoryCueList::slotItemChanged);
}

WMemoryCueList::~WMemoryCueList() = default;

void WMemoryCueList::setup(const QDomNode& node, const SkinContext& context) {
    Q_UNUSED(node);
    Q_UNUSED(context);
}

void WMemoryCueList::slotTrackLoaded(TrackPointer pTrack) {
    setTrack(std::move(pTrack));
}

void WMemoryCueList::slotLoadingTrack(TrackPointer pNewTrack, TrackPointer pOldTrack) {
    Q_UNUSED(pNewTrack);
    Q_UNUSED(pOldTrack);
    setTrack(TrackPointer());
}

void WMemoryCueList::setTrack(TrackPointer pTrack) {
    if (m_pTrack == pTrack) {
        slotCuesUpdated();
        return;
    }
    if (m_pTrack) {
        disconnect(m_pTrack.get(), nullptr, this, nullptr);
    }
    m_pTrack = std::move(pTrack);
    if (m_pTrack) {
        // Queued: a comment edit sets the label from slotItemChanged(), and
        // the rows must not be rebuilt inside that signal.
        connect(m_pTrack.get(),
                &Track::cuesUpdated,
                this,
                &WMemoryCueList::slotCuesUpdated,
                Qt::QueuedConnection);
    }
    slotCuesUpdated();
}

double WMemoryCueList::framesToSeconds(double frames) const {
    if (!m_pTrack) {
        return -1.0;
    }
    const auto sampleRate = m_pTrack->getSampleRate();
    if (!sampleRate.isValid()) {
        return -1.0;
    }
    return frames / sampleRate.value();
}

void WMemoryCueList::slotCuesUpdated() {
    // AI-generated (Claude Code). Start of AI-generated text.
    // Do not replace the items under an open comment editor: closeEditor()
    // rebuilds the rows after the edit.
    // End of AI-generated text.
    if (state() == QAbstractItemView::EditingState) {
        m_rebuildPending = true;
        return;
    }
    m_rebuildPending = false;
    // The same filter and order as the memory_cue_N_* controls.
    QList<CuePointer> memoryCues;
    if (m_pTrack) {
        const QList<CuePointer> cues = m_pTrack->getCuePoints();
        for (const CuePointer& pCue : cues) {
            if (pCue->getType() == mixxx::CueType::MemoryCue &&
                    pCue->getPosition().isValid()) {
                memoryCues.append(pCue);
            }
        }
        std::stable_sort(memoryCues.begin(),
                memoryCues.end(),
                [](const CuePointer& a, const CuePointer& b) {
                    return a->getPosition() < b->getPosition();
                });
    }
    m_memoryCues = memoryCues;

    m_updating = true;
    const int selected = currentRow();
    setRowCount(static_cast<int>(m_memoryCues.size()));
    for (int row = 0; row < m_memoryCues.size(); ++row) {
        const CuePointer& pCue = m_memoryCues.at(row);
        const Cue::StartAndEndPositions pos = pCue->getStartAndEndPosition();

        QPixmap swatch(kSwatchWidth, kRowHeight - 4);
        swatch.fill(mixxx::RgbColor::toQColor(pCue->getColor()));
        auto* pTimeItem = new QTableWidgetItem(QIcon(swatch),
                mixxx::formatMemoryCueTime(
                        framesToSeconds(pos.startPosition.value())));
        pTimeItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        setItem(row, kColumnTime, pTimeItem);

        QString loopText;
        if (pos.endPosition.isValid() && pos.endPosition > pos.startPosition) {
            loopText = mixxx::formatMemoryCueTime(
                    framesToSeconds(pos.endPosition - pos.startPosition));
        }
        auto* pLoopItem = new QTableWidgetItem(loopText);
        pLoopItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        setItem(row, kColumnLoop, pLoopItem);

        auto* pCommentItem = new QTableWidgetItem(pCue->getLabel());
        pCommentItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
        pCommentItem->setData(kCueRole,
                QVariant::fromValue(reinterpret_cast<quintptr>(pCue.get())));
        setItem(row, kColumnComment, pCommentItem);
    }
    if (selected >= 0 && selected < rowCount()) {
        selectRow(selected);
    }
    m_updating = false;
}

void WMemoryCueList::activateMemoryCue(int row) {
    if (row < 0 || row >= m_memoryCues.size() ||
            row >= static_cast<int>(m_activateControls.size())) {
        return;
    }
    auto& pControl = m_activateControls[row];
    if (!pControl) {
        pControl = std::make_unique<ControlProxy>(m_group, kActivateKey.arg(row + 1), this);
    }
    pControl->set(1.0);
    pControl->set(0.0);
}

void WMemoryCueList::deleteMemoryCue(int row) {
    if (!m_pTrack || row < 0 || row >= m_memoryCues.size()) {
        return;
    }
    // AI-generated (Claude Code). Start of AI-generated text.
    // Remove this cue directly. The deck does not go to it: no seek, no
    // change of the cue point, no loop. memory_cue_delete is not used because
    // it removes the memory cue at the play position.
    // End of AI-generated text.
    const CuePointer pCue = m_memoryCues.at(row);
    m_pTrack->removeCue(pCue);
}

void WMemoryCueList::slotCellClicked(int row, int column) {
    // AI-generated (Claude Code). Start of AI-generated text.
    // A click on the comment does not call the cue: the first click of a
    // double click to edit the comment must not move a playing deck.
    // End of AI-generated text.
    if (column == kColumnComment) {
        return;
    }
    activateMemoryCue(row);
}

void WMemoryCueList::slotItemChanged(QTableWidgetItem* pItem) {
    if (m_updating || !pItem || pItem->column() != kColumnComment) {
        return;
    }
    // AI-generated (Claude Code). Start of AI-generated text.
    // Find the cue of the item, not of the row index.
    // End of AI-generated text.
    const auto cueAddress = pItem->data(kCueRole).value<quintptr>();
    CuePointer pCue;
    for (const CuePointer& pMemoryCue : std::as_const(m_memoryCues)) {
        if (reinterpret_cast<quintptr>(pMemoryCue.get()) == cueAddress) {
            pCue = pMemoryCue;
            break;
        }
    }
    if (!pCue) {
        return;
    }
    const QString label = pItem->text().trimmed();
    if (pCue->getLabel() != label) {
        pCue->setLabel(label);
    }
}

void WMemoryCueList::mousePressEvent(QMouseEvent* pEvent) {
    if (pEvent->button() == Qt::RightButton) {
        const QModelIndex index = indexAt(pEvent->position().toPoint());
        if (index.isValid()) {
            selectRow(index.row());
            deleteMemoryCue(index.row());
        }
        pEvent->accept();
        return;
    }
    QTableWidget::mousePressEvent(pEvent);
}

void WMemoryCueList::closeEditor(
        QWidget* pEditor, QAbstractItemDelegate::EndEditHint hint) {
    // The delegate commits the data before it closes the editor.
    QTableWidget::closeEditor(pEditor, hint);
    if (m_rebuildPending) {
        QMetaObject::invokeMethod(
                this, &WMemoryCueList::slotCuesUpdated, Qt::QueuedConnection);
    }
}

void WMemoryCueList::contextMenuEvent(QContextMenuEvent* pEvent) {
    // The right button deletes in mousePressEvent(): no menu.
    pEvent->accept();
}

bool WMemoryCueList::event(QEvent* pEvent) {
    if (pEvent->type() == QEvent::ToolTip) {
        updateTooltip();
    }
    return QTableWidget::event(pEvent);
}
