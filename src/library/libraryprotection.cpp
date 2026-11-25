// AI-generated (Claude Code). Start of AI-generated text.
#include "library/libraryprotection.h"

#include <QCoreApplication>
#include <QMessageBox>
#include <atomic>

#include "library/library_prefs.h"

namespace mixxx {
namespace library {

namespace {

std::atomic<bool> s_protectionOn{prefs::kLibraryProtectionDefault};

} // namespace

bool isActionAllowed(bool protectionOn, ProtectedAction action) {
    if (!protectionOn) {
        return true;
    }
    switch (action) {
    case ProtectedAction::EditPlayCount:
        return true;
    case ProtectedAction::EditTrackInfo:
    case ProtectedAction::RemoveTracks:
    case ProtectedAction::RenameTrackSet:
    case ProtectedAction::DeleteTrackSet:
    case ProtectedAction::MoveTrackSet:
    case ProtectedAction::RelocateTracks:
        return false;
    }
    return false;
}

bool isActionAllowed(ProtectedAction action) {
    return isActionAllowed(isLibraryProtectionOn(), action);
}

bool isLibraryProtectionOn() {
    return s_protectionOn.load(std::memory_order_relaxed);
}

void setLibraryProtectionOn(bool on) {
    s_protectionOn.store(on, std::memory_order_relaxed);
}

void loadLibraryProtection(const UserSettingsPointer& pConfig) {
    if (!pConfig) {
        return;
    }
    setLibraryProtectionOn(pConfig->getValue(
            prefs::kLibraryProtectionConfigKey, prefs::kLibraryProtectionDefault));
}

QString protectionMessageText() {
    return QCoreApplication::translate("LibraryProtection",
            "Library Protection is on. To do this, turn it off in "
            "Preferences > Library.");
}

void showProtectionMessage(QWidget* pParent) {
    QMessageBox::information(pParent,
            QCoreApplication::translate("LibraryProtection", "Library Protection"),
            protectionMessageText());
}

} // namespace library
} // namespace mixxx

// End of AI-generated text.
