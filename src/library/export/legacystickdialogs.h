#pragma once

// AI-generated (Claude Code). Start of AI-generated text.

#include <QString>

#include "library/export/legacystickflow.h"
#include "preferences/usersettings.h"

class QWidget;

namespace mixxx {

/// Mix Two: the message boxes of the update of a stick that has only the
/// legacy device library. It reads and stores the preferences
/// `[Library] LegacyStickUpdateStopAsking` and `LegacyStickUpdateKeepLegacy`,
/// and the backups go to `<settings>/device-backups`, as for the export.
/// Call it on the main thread. Returns the outcome of runLegacyStickFlow().
LegacyStickFlowOutcome runLegacyStickUpdate(
        QWidget* pParent, UserSettingsPointer pConfig, const QString& stickRoot);

/// True when the DJ set "Stop asking in the future": then Mix Two updates a
/// legacy-only stick with no question.
bool legacyStickUpdateStopAsking(UserSettingsPointer pConfig);

} // namespace mixxx

// End of AI-generated text.
