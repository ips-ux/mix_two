#pragma once

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

// AI-generated (Claude Code). Start of AI-generated text.
/// Formats a track position in seconds as mm:ss.mmm (minutes can go above 59).
/// Returns an empty string for a negative or invalid value.
// End of AI-generated text.
QString formatMemoryCueTime(double seconds);

} // namespace mixxx

// AI-generated (Claude Code). Start of AI-generated text.
/// The memory cues and memory loops of the track on a deck, in time order:
/// time (mm:ss.mmm) with a colour swatch, loop length and comment. A click calls the
/// memory cue (memory_cue_N_activate). A right click calls it and then
/// deletes it. A double click on the comment edits it. The rows follow
/// Track::cuesUpdated. GUI thread only.
// End of AI-generated text.
class WMemoryCueList : public QTableWidget, public WBaseWidget {
    Q_OBJECT
  public:
    WMemoryCueList(const QString& group, QWidget* pParent = nullptr);
    ~WMemoryCueList() override;

    void setup(const QDomNode& node, const SkinContext& context);

    /// The memory cues as the rows show them (time order).
    const QList<CuePointer>& memoryCues() const {
        return m_memoryCues;
    }
    /// Calls the memory cue of this row (click).
    void activateMemoryCue(int row);
    /// Calls the memory cue of this row, then deletes it (right click).
    void deleteMemoryCue(int row);

  public slots:
    void slotTrackLoaded(TrackPointer pTrack);
    void slotLoadingTrack(TrackPointer pNewTrack, TrackPointer pOldTrack);

  protected:
    bool event(QEvent* pEvent) override;
    void mousePressEvent(QMouseEvent* pEvent) override;
    void contextMenuEvent(QContextMenuEvent* pEvent) override;

  private slots:
    void slotCuesUpdated();
    void slotCellClicked(int row, int column);
    void slotItemChanged(QTableWidgetItem* pItem);

  private:
    enum Column {
        kColumnTime = 0,
        kColumnLoop,
        kColumnComment,
        kColumnCount,
    };

    void setTrack(TrackPointer pTrack);
    double framesToSeconds(double frames) const;

    const QString m_group;
    TrackPointer m_pTrack;
    QList<CuePointer> m_memoryCues;
    /// memory_cue_N_activate, created when first used.
    std::vector<std::unique_ptr<ControlProxy>> m_activateControls;
    /// True while the rows are rebuilt: item changes are not edits then.
    bool m_updating;
};
