#pragma once

// AI-generated (Claude Code). Start of AI-generated text.

#include <functional>
#include <memory>
#include <optional>

#include "library/rekordbox/legacytodlp.h"

namespace mixxx {

/// Mix Two: the questions for a stick that has only the legacy device
/// library, without widgets, so that a test can give the answers. The
/// stick watcher and the export dialog give a real implementation with
/// message boxes.

/// The choices of the DJ.
struct LegacyStickChoices {
    /// True: give the stick a Device Library Plus.
    bool update = false;
    /// True: `export.pdb` stays on the stick.
    bool keepLegacy = true;
};

/// The stored answers ("Stop asking in the future").
struct LegacyStickPrefs {
    /// True: update every legacy-only stick with no question.
    bool stopAsking = false;
    /// The choice for the legacy library that the automatic update uses.
    bool keepLegacy = true;
};

class LegacyStickUi {
  public:
    virtual ~LegacyStickUi() = default;
    /// "This stick has no Device Library Plus. Update the stick?" Returns
    /// true for Update. *pStopAsking is the check box "Stop asking in the
    /// future"; it starts cleared.
    virtual bool askUpdate(bool* pStopAsking) = 0;
    /// "Keep the older device library on the stick?" `initial` is the
    /// choice that shows first. Returns true for Keep.
    virtual bool askKeepLegacy(bool initial) = 0;
    /// Shows what the update did. Returns true when the DJ pushes Back to
    /// change the choices, false for Done.
    virtual bool showSummary(const rekordbox::LegacyToDlpResult& result) = 0;
};

/// Does the change on the stick for the choices, from the state that the
/// last call left. A call with `update` false after an update undoes it.
using LegacyStickApply =
        std::function<rekordbox::LegacyToDlpResult(const LegacyStickChoices& choices)>;

struct LegacyStickFlowOutcome {
    /// The choices that are on the stick at the end.
    LegacyStickChoices choices;
    /// The result of the last apply call; `ok` is false when there was none.
    rekordbox::LegacyToDlpResult lastResult;
    /// The number of apply calls.
    int applyCount = 0;
    /// Set when the caller must store new preferences.
    std::optional<LegacyStickPrefs> prefsToStore;
};

/// True when the stick has only the legacy device library.
bool isLegacyOnlyStick(bool hasLegacy, bool hasPlus);

/// Runs the questions: update?, keep legacy?, apply, summary; Back in the
/// summary asks again with the last choices as the first answers. With
/// `prefs.stopAsking` it asks nothing, applies the stored choice and shows
/// the summary only when the update fails. "Stop asking in the future" is
/// stored only after an update that succeeds.
LegacyStickFlowOutcome runLegacyStickFlow(
        const LegacyStickPrefs& prefs, LegacyStickUi& ui, const LegacyStickApply& apply);

/// The apply function for a real stick. The first call with `update` set
/// runs updateLegacyStickToDlp() with `options` and the choice for the
/// legacy library. Later calls change the stick from the state that the last
/// call left: Remove takes `export.pdb` and `exportExt.pdb` off the stick,
/// Keep restores them from the backup, and a call with `update` cleared
/// removes `exportLibrary.db` and restores the legacy files that are gone.
LegacyStickApply makeLegacyStickApply(
        const QString& stickRoot, const rekordbox::LegacyToDlpOptions& options);

} // namespace mixxx

// End of AI-generated text.
