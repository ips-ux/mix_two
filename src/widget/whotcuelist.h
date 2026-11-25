#pragma once

// AI-generated (Claude Code). Start of AI-generated text.
#include <QAbstractItemDelegate>
#include <QList>
#include <QString>
#include <QTableWidget>
#include <memory>
#include <vector>

#include "track/cue.h"
#include "track/track_decl.h"
#include "widget/wbasewidget.h"

class ControlProxy;
class QDomNode;
class SkinContext;

namespace mixxx {

/// The name of a hot cue as the pad shows it: A, B, C ... for the index 0, 1,
/// 2 ... After Z the list uses the number of the hot cue (index + 1).
/// Returns an empty string for a negative index.
QString formatHotCueName(int hotCueIndex);

} // namespace mixxx

/// The hot cues and hot loops of the track on a deck, in the order of the
/// pads (A, B, C ...): name with a colour swatch, time (mm:ss.mmm), loop
/// length and comment. It is modelled on WMemoryCueList and looks and
/// behaves like it. A click on the name, the time or the loop length goes to
/// the hot cue (hotcue_N_goto); a click on the comment does not. A right
/// click deletes the hot cue through hotcue_N_clear, the same control as the
/// CLEAR of the deck, and does not move the deck. A double click on the
/// comment edits it. The rows follow Track::cuesUpdated, but not while the
/// comment editor is open. GUI thread only.
class WHotCueList : public QTableWidget, public WBaseWidget {
    Q_OBJECT
  public:
    WHotCueList(const QString& group, QWidget* pParent = nullptr);
    ~WHotCueList() override;

    void setup(const QDomNode& node, const SkinContext& context);

    /// The hot cues as the rows show them (order of the pads).
    const QList<CuePointer>& hotCues() const {
        return m_hotCues;
    }
    /// Goes to the hot cue of this row (click).
    void activateHotCue(int row);
    /// Clears the hot cue of this row (right click). The deck stays.
    void deleteHotCue(int row);

  public slots:
    void slotTrackLoaded(TrackPointer pTrack);
    void slotLoadingTrack(TrackPointer pNewTrack, TrackPointer pOldTrack);

  protected:
    bool event(QEvent* pEvent) override;
    void mousePressEvent(QMouseEvent* pEvent) override;
    void closeEditor(QWidget* pEditor, QAbstractItemDelegate::EndEditHint hint) override;
    void contextMenuEvent(QContextMenuEvent* pEvent) override;

  private slots:
    void slotCuesUpdated();
    void slotCellClicked(int row, int column);
    void slotItemChanged(QTableWidgetItem* pItem);

  private:
    enum Column {
        kColumnName = 0,
        kColumnTime,
        kColumnLoop,
        kColumnComment,
        kColumnCount,
    };

    void setTrack(TrackPointer pTrack);
    double framesToSeconds(double frames) const;
    /// The control hotcue_N_<name> of this row, created when first used.
    ControlProxy* controlOfRow(int row, const QString& name);

    const QString m_group;
    TrackPointer m_pTrack;
    QList<CuePointer> m_hotCues;
    /// hotcue_N_goto and hotcue_N_clear by hot cue index, created when
    /// first used.
    std::vector<std::unique_ptr<ControlProxy>> m_gotoControls;
    std::vector<std::unique_ptr<ControlProxy>> m_clearControls;
    /// hotcues_load, created when first used.
    std::unique_ptr<ControlProxy> m_loadControl;
    /// True while the rows are rebuilt: item changes are not edits then.
    bool m_updating;
    /// True when cuesUpdated came while the comment editor was open.
    bool m_rebuildPending;
};
// End of AI-generated text.
