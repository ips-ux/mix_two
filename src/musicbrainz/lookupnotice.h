#pragma once
// AI-generated (Claude Code). Start of AI-generated text.

#include <QString>
#include <QStringList>
#include <functional>

#include "preferences/usersettings.h"

class QWidget;

namespace mixxx {
namespace musicbrainz {

// The one-time notice before the first online lookup (AcoustID, MusicBrainz,
// Cover Art Archive). No request leaves the computer before the user accepts.
// "Cancel" sends nothing and does not set the flag, so the notice comes again
// at the next lookup.

// True while the user has not accepted the notice.
bool needsLookupNotice(const UserSettingsPointer& pConfig);

// Remembers that the user accepted the notice.
void acceptLookupNotice(const UserSettingsPointer& pConfig);

// Withdraws the consent: the notice shows again at the next lookup.
void withdrawLookupNotice(const UserSettingsPointer& pConfig);

// The query items of the AcoustID request that the notice names as sent data.
QStringList lookupNoticeNamedQueryKeys();

// The query items of the AcoustID request that the notice describes as fixed
// request options.
QStringList lookupNoticeOptionQueryKeys();

// The full text of the notice.
QString lookupNoticeText();

// Decides whether a lookup may start. When the notice is needed, askUser
// shows it and returns true for "Accept". Returns true when the lookup may
// start. Every online lookup calls this before it sends a request.
bool confirmOnlineLookup(
        const UserSettingsPointer& pConfig,
        const std::function<bool()>& askUser);

// The same with the message box of the application.
bool confirmOnlineLookup(const UserSettingsPointer& pConfig, QWidget* pParent);

} // namespace musicbrainz
} // namespace mixxx

// End of AI-generated text.
