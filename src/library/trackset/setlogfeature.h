#pragma once

#include <QElapsedTimer>
#include <QPointer>
#include <map>
#include <memory>

#include "control/pollingcontrolproxy.h"
#include "library/trackset/baseplaylistfeature.h"
#include "library/trackset/playtimegate.h"
#include "preferences/usersettings.h"
#include "util/parented_ptr.h"

class Library;
class QAction;
class QTimer;

class SetlogFeature : public BasePlaylistFeature {
    Q_OBJECT

  public:
    SetlogFeature(Library* pLibrary,
            UserSettingsPointer pConfig);
    virtual ~SetlogFeature();

    QVariant title() override;

    void bindLibraryWidget(WLibrary* libraryWidget,
            KeyboardEventFilter* keyboard) override;
    void activatePlaylist(int playlistId) override;

  public slots:
    void onRightClick(const QPoint& globalPos) override;
    void onRightClickChild(const QPoint& globalPos, const QModelIndex& index) override;
    void slotJoinWithPrevious();
    void slotMarkAllTracksPlayed();
    void slotLockAllChildPlaylists();
    void slotUnlockAllChildPlaylists();
    void slotDeletePlaylist() override;
    void slotGetNewPlaylist();
    void activate() override;
    void activateChild(const QModelIndex& index) override;

  protected:
    QModelIndex constructChildModel(int selectedId);
    void decorateChild(TreeItem* pChild, int playlistId) override;

  private slots:
    // Adds the track to the history. It runs when the track played for 60 seconds.
    void slotPlayingTrackChanged(TrackPointer currentPlayingTrack);
    void slotCountPlayTime();
    void slotPlaylistTableChanged(int playlistId) override;
    void slotPlaylistContentOrLockChanged(const QSet<int>& playlistIds) override;
    void slotPlaylistTableRenamed(int playlistId, const QString& newName) override;
    void slotDeleteAllUnlockedChildPlaylists();

  private:
    void deleteAllUnlockedPlaylistsWithFewerTracks();
    void lockOrUnlockAllChildPlaylists(bool lock);
    QString getRootViewHtml() const override;

    std::list<TrackId> m_recentTracks;
    // AI-generated (Claude Code). Start of AI-generated text.
    PlayTimeGate m_playTimeGate;
    QElapsedTimer m_playTimeClock;
    parented_ptr<QTimer> m_pPlayTimeTimer;
    std::map<QString, std::unique_ptr<PollingControlProxy>> m_playControls;
    // End of AI-generated text.
    QAction* m_pJoinWithPreviousAction;
    QAction* m_pMarkTracksPlayedAction;
    QAction* m_pStartNewPlaylist;
    QAction* m_pLockAllChildPlaylists;
    QAction* m_pUnlockAllChildPlaylists;
    QAction* m_pDeleteAllChildPlaylists;

    int m_currentPlaylistId;
    int m_yearNodeId;
    Library* m_pLibrary;
    UserSettingsPointer m_pConfig;
};
