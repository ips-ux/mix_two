#pragma once

#include "library/trackset/crate/crateid.h"
#include "util/optional.h"

class QListWidget;
class TrackCollection;

namespace mixxx {

/// Mix Two: the lists of crates and playlists that the export dialogs show
/// (DlgLibraryExport for Engine DJ and DlgDeviceExport for a stick). The
/// functions exist in every build: they do not depend on __ENGINEPRIME__.
///
/// The item data (`Qt::UserRole`) of a crate is `CrateId::toVariant()`. The
/// item data of a playlist is its ID as an int.

/// Fills the list with all crates of the collection.
void populateCrateList(QListWidget* pListWidget, const TrackCollection& trackCollection);

/// Fills the list with all playlists of the collection that are not hidden.
void populatePlaylistList(QListWidget* pListWidget, TrackCollection& trackCollection);

/// Clears the selection of both lists, then selects the crate and the playlist
/// (the current item too). A crate or a playlist that is not in its list is
/// ignored.
void selectInitialListItems(QListWidget* pCratesList,
        QListWidget* pPlaylistsList,
        std::optional<CrateId> crateId,
        std::optional<int> playlistId);

} // namespace mixxx
