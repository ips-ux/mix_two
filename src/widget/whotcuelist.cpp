// AI-generated (Claude Code). Start of AI-generated text.
// WHotCueList: written by Claude Code, modelled on WMemoryCueList. All
// comments in this file are AI-generated text.
// End of AI-generated text.
#include "widget/whotcuelist.h"

#include <QContextMenuEvent>
#include <QEvent>
#include <QHeaderView>
#include <QIcon>
#include <QMouseEvent>
#include <QPixmap>
#include <QTableWidgetItem>
#include <algorithm>
#include <utility>

#include "control/controlproxy.h"
#include "moc_whotcuelist.cpp"
#include "track/track.h"
#include "util/color/rgbcolor.h"
#include "util/defs.h"
#include "widget/wmemorycuelist.h"

namespace {
constexpr int kRowHeight = 16;
constexpr int kSwatchWidth = 4;
// Item data of the comment item: the address of its Cue, so that an edit
// writes to that cue even when the rows moved.
constexpr int kCueRole = Qt::UserRole + 1;
constexpr int kLetterCount = 26;
} // namespace

namespace mixxx {

QString formatHotCueName(int hotCueIndex) {
    if (hotCueIndex < 0) {
        return QString();
    }
    if (hotCueIndex < kLetterCount) {
        return QString(QChar('A' + hotCueIndex));
    }
    return QString::number(hotCueIndex + 1);
}

} // namespace mixxx

WHotCueList::WHotCueList(const QString& group, QWidget* pParent)
        : QTableWidget(0, kColumnCount, pParent),
          WBaseWidget(this),
          m_group(group),
          m_gotoControls(kMaxNumberOfHotcues),
          m_clearControls(kMaxNumberOfHotcues),
          m_updating(false),
          m_rebuildPending(false) {
    setHorizontalHeaderLabels({tr("HOT CUE"), tr("TIME"), tr("LOOP"), tr("COMMENT")});
    verticalHeader()->hide();
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader()->setMinimumSectionSize(kRowHeight);
    verticalHeader()->setDefaultSectionSize(kRowHeight);
    setIconSize(QSize(kSwatchWidth, kRowHeight - 4));
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    horizontalHeader()->setSectionResizeMode(kColumnName, QHeaderView::ResizeToContents);
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

    connect(this, &QTableWidget::cellClicked, this, &WHotCueList::slotCellClicked);
    connect(this, &QTableWidget::itemChanged, this, &WHotCueList::slotItemChanged);
}

WHotCueList::~WHotCueList() = default;

void WHotCueList::setup(const QDomNode& node, const SkinContext& context) {
    Q_UNUSED(node);
    Q_UNUSED(context);
}

void WHotCueList::slotTrackLoaded(TrackPointer pTrack) {
    setTrack(std::move(pTrack));
}

void WHotCueList::slotLoadingTrack(TrackPointer pNewTrack, TrackPointer pOldTrack) {
    Q_UNUSED(pNewTrack);
    Q_UNUSED(pOldTrack);
    setTrack(TrackPointer());
}

void WHotCueList::setTrack(TrackPointer pTrack) {
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
                &WHotCueList::slotCuesUpdated,
                Qt::QueuedConnection);
    }
    slotCuesUpdated();
}

double WHotCueList::framesToSeconds(double frames) const {
    if (!m_pTrack) {
        return -1.0;
    }
    const auto sampleRate = m_pTrack->getSampleRate();
    if (!sampleRate.isValid()) {
        return -1.0;
    }
    return frames / sampleRate.value();
}

void WHotCueList::slotCuesUpdated() {
    // Do not replace the items under an open comment editor: closeEditor()
    // rebuilds the rows after the edit.
    if (state() == QAbstractItemView::EditingState) {
        m_rebuildPending = true;
        return;
    }
    m_rebuildPending = false;
    // The hot cues and hot loops that a pad can hold, in the order of the
    // pads.
    QList<CuePointer> hotCues;
    if (m_pTrack) {
        const QList<CuePointer> cues = m_pTrack->getCuePoints();
        for (const CuePointer& pCue : cues) {
            const mixxx::CueType type = pCue->getType();
            if ((type == mixxx::CueType::HotCue || type == mixxx::CueType::Loop) &&
                    pCue->getHotCue() >= 0 &&
                    pCue->getHotCue() < kMaxNumberOfHotcues &&
                    pCue->getPosition().isValid()) {
                hotCues.append(pCue);
            }
        }
        std::stable_sort(hotCues.begin(),
                hotCues.end(),
                [](const CuePointer& a, const CuePointer& b) {
                    return a->getHotCue() < b->getHotCue();
                });
    }
    m_hotCues = hotCues;

    m_updating = true;
    const int selected = currentRow();
    setRowCount(static_cast<int>(m_hotCues.size()));
    for (int row = 0; row < m_hotCues.size(); ++row) {
        const CuePointer& pCue = m_hotCues.at(row);
        const Cue::StartAndEndPositions pos = pCue->getStartAndEndPosition();

        QPixmap swatch(kSwatchWidth, kRowHeight - 4);
        swatch.fill(mixxx::RgbColor::toQColor(pCue->getColor()));
        auto* pNameItem = new QTableWidgetItem(
                QIcon(swatch), mixxx::formatHotCueName(pCue->getHotCue()));
        pNameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        setItem(row, kColumnName, pNameItem);

        auto* pTimeItem = new QTableWidgetItem(
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

ControlProxy* WHotCueList::controlOfRow(int row, const QString& name) {
    if (row < 0 || row >= m_hotCues.size()) {
        return nullptr;
    }
    const int index = m_hotCues.at(row)->getHotCue();
    if (index < 0 || index >= kMaxNumberOfHotcues) {
        return nullptr;
    }
    auto& controls = name == QStringLiteral("clear") ? m_clearControls : m_gotoControls;
    auto& pControl = controls[index];
    if (!pControl) {
        // The hot cue number of the controls is the index plus one.
        pControl = std::make_unique<ControlProxy>(
                m_group, QStringLiteral("hotcue_%1_%2").arg(index + 1).arg(name), this);
    }
    return pControl.get();
}

void WHotCueList::activateHotCue(int row) {
    ControlProxy* pControl = controlOfRow(row, QStringLiteral("goto"));
    if (!pControl) {
        return;
    }
    pControl->set(1.0);
    pControl->set(0.0);
}

void WHotCueList::deleteHotCue(int row) {
    // The same control as the CLEAR of the deck: the engine removes the cue,
    // detaches the pad and resets the focus. The deck does not go to the cue.
    ControlProxy* pControl = controlOfRow(row, QStringLiteral("clear"));
    if (!pControl) {
        return;
    }
    pControl->set(1.0);
    pControl->set(0.0);
}

void WHotCueList::slotCellClicked(int row, int column) {
    // A click on the comment does not go to the cue: the first click of a
    // double click to edit the comment must not move a playing deck.
    if (column == kColumnComment) {
        return;
    }
    activateHotCue(row);
}

void WHotCueList::slotItemChanged(QTableWidgetItem* pItem) {
    if (m_updating || !pItem || pItem->column() != kColumnComment) {
        return;
    }
    // Find the cue of the item, not of the row index.
    const auto cueAddress = pItem->data(kCueRole).value<quintptr>();
    CuePointer pCue;
    for (const CuePointer& pHotCue : std::as_const(m_hotCues)) {
        if (reinterpret_cast<quintptr>(pHotCue.get()) == cueAddress) {
            pCue = pHotCue;
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

void WHotCueList::mousePressEvent(QMouseEvent* pEvent) {
    if (pEvent->button() == Qt::RightButton) {
        const QModelIndex index = indexAt(pEvent->position().toPoint());
        if (index.isValid()) {
            selectRow(index.row());
            deleteHotCue(index.row());
        }
        pEvent->accept();
        return;
    }
    QTableWidget::mousePressEvent(pEvent);
}

void WHotCueList::closeEditor(
        QWidget* pEditor, QAbstractItemDelegate::EndEditHint hint) {
    // The delegate commits the data before it closes the editor.
    QTableWidget::closeEditor(pEditor, hint);
    if (m_rebuildPending) {
        QMetaObject::invokeMethod(
                this, &WHotCueList::slotCuesUpdated, Qt::QueuedConnection);
    }
}

void WHotCueList::contextMenuEvent(QContextMenuEvent* pEvent) {
    // The right button deletes in mousePressEvent(): no menu.
    pEvent->accept();
}

bool WHotCueList::event(QEvent* pEvent) {
    if (pEvent->type() == QEvent::ToolTip) {
        updateTooltip();
    }
    return QTableWidget::event(pEvent);
}
// End of AI-generated text.
