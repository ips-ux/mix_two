#pragma once

#include <QSet>

#include "library/dao/playlistdao.h"
#include "library/trackset/tracksettablemodel.h"
#include "util/duration.h"

class PlaylistTableModel final : public TrackSetTableModel {
    Q_OBJECT

  public:
    PlaylistTableModel(QObject* parent,
            TrackCollectionManager* pTrackCollectionManager,
            const char* settingsNamespace,
            bool keepHiddenTracks = false);
    ~PlaylistTableModel() final = default;

    void selectPlaylist(int playlistId = -1 /* kInvalidPlaylistId */);
    int getPlaylist() const {
        return m_iPlaylistId;
    }

    // AI-generated (Claude Code). Start of AI-generated text.
    /// Shows every track of the playlists in the folder and below it, each
    /// track once. Choice: this view is read-only. A list that is made of
    /// several playlists has no order of its own, so it cannot be reordered,
    /// and a track cannot be removed through it.
    void selectPlaylistFolder(int folderId);
    bool isFolderView() const {
        return m_iFolderId != kInvalidPlaylistFolderId;
    }
    // End of AI-generated text.

    bool appendTrack(TrackId trackId);
    void moveTrack(const QModelIndex& sourceIndex, const QModelIndex& destIndex) override;
    void removeTrack(const QModelIndex& index);
    void shuffleTracks(const QModelIndexList& shuffle = QModelIndexList(),
            const QModelIndex& exclude = QModelIndex());
    void orderTracksByCurrPos();

    bool isColumnInternal(int column) final;
    bool isColumnHiddenByDefault(int column) final;
    /// This function should only be used by AUTODJ
    void removeTracks(const QModelIndexList& indices) final;
    /// Returns the number of successful additions.
    int addTracksWithTrackIds(const QModelIndex& index,
            const QList<TrackId>& trackIds,
            int* pOutInsertionPos) final;
    bool isLocked() final;

    /// Get the total duration of all tracks referenced by the given model indices
    mixxx::Duration getTotalDuration(const QModelIndexList& indices);
    const QList<int> getSelectedPositions(const QModelIndexList& indices) const override;

    Capabilities getCapabilities() const final;

    QString modelKey(bool noSearch) const override;

    // AI-generated (Claude Code). Start of AI-generated text.
    // Lets a test make the model hold a real playlist id in folder view.
    friend class PlaylistTableModelTestAccess;
    // End of AI-generated text.

  private slots:
    void playlistsChanged(const QSet<int>& playlistIds);
    void refreshFolderView();

  signals:
    void firstTrackChanged();

  private:
    void initSortColumnMapping() override;

    int m_iPlaylistId;
    int m_iFolderId;
    QSet<int> m_folderPlaylistIds;
    bool m_keepHiddenTracks;
    QHash<int, QString> m_searchTexts;
};
