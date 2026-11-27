// AI-generated (Claude Code). Start of AI-generated text.

#include "library/export/legacystickflow.h"

#include "library/rekordbox/devicelibraryplusstick.h"

namespace mixxx {

using rekordbox::LegacyToDlpResult;

bool isLegacyOnlyStick(bool hasLegacy, bool hasPlus) {
    return hasLegacy && !hasPlus;
}

LegacyStickFlowOutcome runLegacyStickFlow(
        const LegacyStickPrefs& prefs, LegacyStickUi& ui, const LegacyStickApply& apply) {
    LegacyStickFlowOutcome outcome;
    const auto applyChoices = [&](const LegacyStickChoices& choices) {
        outcome.lastResult = apply(choices);
        ++outcome.applyCount;
        if (outcome.lastResult.ok) {
            outcome.choices = choices;
        }
        return outcome.lastResult.ok;
    };

    if (prefs.stopAsking) {
        if (!applyChoices(LegacyStickChoices{true, prefs.keepLegacy})) {
            ui.showSummary(outcome.lastResult);
        }
        return outcome;
    }

    bool keepLegacy = prefs.keepLegacy;
    for (;;) {
        bool stopAsking = false;
        if (!ui.askUpdate(&stopAsking)) {
            // Not now. After Back, this undoes the update.
            outcome.prefsToStore.reset();
            if (outcome.choices.update) {
                applyChoices(LegacyStickChoices{false, true});
            }
            return outcome;
        }
        keepLegacy = ui.askKeepLegacy(keepLegacy);
        const bool ok = applyChoices(LegacyStickChoices{true, keepLegacy});
        outcome.prefsToStore.reset();
        if (ok && stopAsking) {
            outcome.prefsToStore = LegacyStickPrefs{true, keepLegacy};
        }
        if (!ui.showSummary(outcome.lastResult)) {
            return outcome;
        }
    }
}

namespace {

/// What the apply function of a real stick knows between its calls.
struct StickState {
    QString stickRoot;
    rekordbox::LegacyToDlpOptions options;
    bool updated = false;
    bool legacyOnStick = true;
    /// The result of the update, for the counts of the summary.
    LegacyToDlpResult update;
};

LegacyToDlpResult changeUpdatedStick(StickState& state, const LegacyStickChoices& choices) {
    LegacyToDlpResult result = state.update;
    result.writtenFiles.clear();
    result.removedFiles.clear();
    result.errorMessage.clear();
    QString error;
    bool ok = true;
    if (!choices.update) {
        ok = rekordbox::removePlusLibraryFile(state.stickRoot, &result.removedFiles, &error);
        if (ok) {
            state.updated = false;
        }
    }
    const bool wantLegacy = !choices.update || choices.keepLegacy;
    if (ok && wantLegacy && !state.legacyOnStick) {
        ok = rekordbox::dlpRestoreLegacyLibrary(
                state.update.backupFolder, state.stickRoot, &result.writtenFiles, &error);
        state.legacyOnStick = ok;
    } else if (ok && !wantLegacy && state.legacyOnStick) {
        ok = rekordbox::removeLegacyLibraryFiles(state.stickRoot, &result.removedFiles, &error);
        if (ok) {
            state.legacyOnStick = false;
        }
    }
    result.ok = ok;
    result.errorMessage = error;
    result.legacyKept = state.legacyOnStick;
    if (!choices.update && ok) {
        result.trackCount = 0;
        result.playlistCount = 0;
        result.historyCount = 0;
    }
    return result;
}

} // namespace

LegacyStickApply makeLegacyStickApply(
        const QString& stickRoot, const rekordbox::LegacyToDlpOptions& options) {
    auto pState = std::make_shared<StickState>();
    pState->stickRoot = stickRoot;
    pState->options = options;
    return [pState](const LegacyStickChoices& choices) {
        if (pState->updated) {
            return changeUpdatedStick(*pState, choices);
        }
        if (!choices.update) {
            LegacyToDlpResult result;
            result.ok = true;
            return result;
        }
        rekordbox::LegacyToDlpOptions options = pState->options;
        options.keepLegacy = choices.keepLegacy;
        LegacyToDlpResult result = rekordbox::updateLegacyStickToDlp(pState->stickRoot, options);
        if (result.ok) {
            pState->updated = true;
            pState->legacyOnStick = result.legacyKept;
            pState->update = result;
        }
        return result;
    };
}

} // namespace mixxx

// End of AI-generated text.
