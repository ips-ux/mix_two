#include "library/export/trackselectionlists.h"

#include <QListWidget>
#include <memory>

#include "library/trackcollection.h"
#include "library/trackset/crate/crate.h"
#include "library/trackset/crate/cratestorage.h"

namespace mixxx {

void populateCrateList(QListWidget* pListWidget, const TrackCollection& trackCollection) {
    CrateSelectResult crates = trackCollection.crates().selectCrates();
    Crate crate;
    pListWidget->clear();
    while (crates.populateNext(&crate)) {
        auto pItem = std::make_unique<QListWidgetItem>(crate.getName());
        pItem->setData(Qt::UserRole, crate.getId().toVariant());
        pListWidget->addItem(pItem.release());
    }
}

void populatePlaylistList(QListWidget* pListWidget, TrackCollection& trackCollection) {
    const auto playlistIdAndNamePairs =
            trackCollection.getPlaylistDAO().getPlaylists(
                    PlaylistDAO::PLHT_NOT_HIDDEN);
    pListWidget->clear();
    for (const auto& [playlistId, playlistName] : playlistIdAndNamePairs) {
        auto pItem = std::make_unique<QListWidgetItem>(playlistName);
        pItem->setData(Qt::UserRole, playlistId);
        pListWidget->addItem(pItem.release());
    }
}

void selectInitialListItems(QListWidget* pCratesList,
        QListWidget* pPlaylistsList,
        std::optional<CrateId> crateId,
        std::optional<int> playlistId) {
    pCratesList->clearSelection();
    pPlaylistsList->clearSelection();

    if (crateId) {
        for (auto i = 0; i < pCratesList->count(); ++i) {
            auto* pItem = pCratesList->item(i);
            if (CrateId(pItem->data(Qt::UserRole)) == crateId) {
                pCratesList->setCurrentItem(pItem);
                pItem->setSelected(true);
                break;
            }
        }
    }

    if (playlistId) {
        for (auto i = 0; i < pPlaylistsList->count(); ++i) {
            auto* pItem = pPlaylistsList->item(i);
            if (pItem->data(Qt::UserRole).toInt() == playlistId) {
                pPlaylistsList->setCurrentItem(pItem);
                pItem->setSelected(true);
                break;
            }
        }
    }
}

} // namespace mixxx
