#pragma once

#include <QFutureWatcher>
#include <atomic>

#include "library/libraryview.h"
#include "library/trackcollection.h"
#include "library/ui_dlgmissing.h"
#include "preferences/usersettings.h"

class WLibrary;
class WTrackTableView;
class MissingTableModel;
class QItemSelection;
class Library;
class KeyboardEventFilter;

class DlgMissing : public QWidget, public Ui::DlgMissing, public LibraryView {
    Q_OBJECT

  public:
    DlgMissing(WLibrary* parent, UserSettingsPointer pConfig,
               Library* pLibrary,
               KeyboardEventFilter* pKeyboard);
    ~DlgMissing() override;

    void onShow() override;
    bool hasFocus() const override;
    void setFocus() override;
    void onSearch(const QString& text) override;
    QString currentSearch();
    void saveCurrentViewState() override;
    bool restoreCurrentViewState() override;

  public slots:
    void selectAll();
    void selectionChanged(const QItemSelection&, const QItemSelection&);

    // AI-generated (Claude Code). Start of AI-generated text.
    void slotRelocate();
    void slotAutoRelocate();
    void slotAutoRelocateSearchFinished();
    // End of AI-generated text.

  signals:
    void trackSelected(TrackPointer pTrack);

  private:
    void activateButtons(bool enable);
    WTrackTableView* m_pTrackTableView;
    MissingTableModel* m_pMissingTableModel;
    // AI-generated (Claude Code). Start of AI-generated text.
    UserSettingsPointer m_pConfig;
    Library* m_pLibrary;
    QFutureWatcher<QList<mixxx::library::RelocateCandidate>> m_searchWatcher;
    std::atomic_bool m_cancelSearch{false};
    // End of AI-generated text.
};
