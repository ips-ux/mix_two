#pragma once
// AI-generated (Claude Code). Start of AI-generated text.
// Library Protection (rekordbox: Preferences > Extensions > Library Protection).
// When it is on, the user cannot change the library data by mistake: the track
// information, the tracks in the library and in playlists and crates, the
// names of playlists, crates and folders, and the file places of tracks.
//
// Every place that makes such a change asks isActionAllowed() first. This is
// the only place that decides.
//
// What stays possible with protection on, because a DJ does it in a set:
//  - load a track on a deck, a sampler or the preview deck,
//  - the play count and the played mark (also the table columns for them),
//  - the History list (it records on its own),
//  - cues, loops, the beat grid and the key that the user sets on a deck,
//  - adding tracks to playlists and crates, and the Auto DJ queue.

#include <QString>

#include "preferences/usersettings.h"

class QWidget;

namespace mixxx {
namespace library {

enum class ProtectedAction {
    /// Edit of the track data in the track list, the track info windows, the
    /// track menu (rating, colour, BPM, clear, import of tags) and the tag fetcher.
    EditTrackInfo,
    /// Remove tracks from the library (hide, purge, delete the file) or from
    /// a playlist or a crate.
    RemoveTracks,
    RenameTrackSet,
    DeleteTrackSet,
    /// Move a playlist or a folder into another folder.
    MoveTrackSet,
    /// Relocate, Auto Relocate and the move of a library folder: they change
    /// the file place of tracks.
    RelocateTracks,
    /// The play count and the played mark. Always allowed.
    EditPlayCount,
};

/// The decision. `protectionOn` is the state of the preference.
bool isActionAllowed(bool protectionOn, ProtectedAction action);

/// The decision with the state of the running program.
bool isActionAllowed(ProtectedAction action);

/// The state of the running program. The library and the Library preferences
/// set it. The default is off.
bool isLibraryProtectionOn();
void setLibraryProtectionOn(bool on);

/// Sets the state of the running program from the saved preference.
void loadLibraryProtection(const UserSettingsPointer& pConfig);

/// The text of the message that tells the user why the action did not run.
QString protectionMessageText();

/// Shows protectionMessageText() in a message box.
void showProtectionMessage(QWidget* pParent);

} // namespace library
} // namespace mixxx

// End of AI-generated text.
