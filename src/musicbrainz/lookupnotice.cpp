// AI-generated (Claude Code). Start of AI-generated text.
#include "musicbrainz/lookupnotice.h"

#include <QCoreApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QWidget>

namespace mixxx {
namespace musicbrainz {

namespace {

const ConfigKey kNoticeAcceptedKey(
        QStringLiteral("[Library]"),
        QStringLiteral("OnlineLookupNoticeAccepted"));

QString tr(const char* text) {
    return QCoreApplication::translate("mixxx::musicbrainz::LookupNotice", text);
}

} // namespace

bool needsLookupNotice(const UserSettingsPointer& pConfig) {
    if (!pConfig) {
        return true;
    }
    return !pConfig->getValue<bool>(kNoticeAcceptedKey, false);
}

void acceptLookupNotice(const UserSettingsPointer& pConfig) {
    if (pConfig) {
        pConfig->setValue(kNoticeAcceptedKey, true);
    }
}

QStringList lookupNoticeNamedQueryKeys() {
    // Named in the text: the audio fingerprint, the duration and the client
    // key of Mix Two.
    return {QStringLiteral("fingerprint"),
            QStringLiteral("duration"),
            QStringLiteral("client")};
}

QStringList lookupNoticeOptionQueryKeys() {
    // Named in the text as fixed request options: the answer format and the
    // kind of data to return.
    return {QStringLiteral("format"), QStringLiteral("meta")};
}

QString lookupNoticeText() {
    return tr("Mix Two can find tag data for a track on the Internet. "
              "For this, Mix Two sends data to these services:") +
            QStringLiteral("\n\n") +
            tr("AcoustID (api.acoustid.org): the audio fingerprint of the "
               "track, the duration of the track in seconds, the client key of "
               "Mix Two, and two fixed request options (answer format and kind "
               "of data). The fingerprint is a short code that Mix Two "
               "calculates from the audio. The audio file does not leave your "
               "computer.") +
            QStringLiteral("\n\n") +
            tr("MusicBrainz (musicbrainz.org): the ID of each recording that "
               "AcoustID returns, and the name and version of Mix Two.") +
            QStringLiteral("\n\n") +
            tr("Cover Art Archive (coverartarchive.org): the ID of the release "
               "that you select, when you ask for cover art. Mix Two then "
               "downloads the image from the address that the service gives.") +
            QStringLiteral("\n\n") +
            tr("Each service also sees the Internet address of your computer. "
               "Mix Two does not send file names, tags or other library data.") +
            QStringLiteral("\n\n") +
            tr("Push Accept to allow online lookups. Push Cancel to send "
               "nothing. Mix Two shows this notice again at the next lookup.");
}

bool confirmOnlineLookup(
        const UserSettingsPointer& pConfig,
        const std::function<bool()>& askUser) {
    if (!needsLookupNotice(pConfig)) {
        return true;
    }
    if (askUser && askUser()) {
        acceptLookupNotice(pConfig);
        return true;
    }
    return false;
}

bool confirmOnlineLookup(const UserSettingsPointer& pConfig, QWidget* pParent) {
    return confirmOnlineLookup(pConfig, [pParent]() {
        QMessageBox box(pParent);
        box.setIcon(QMessageBox::Information);
        box.setWindowTitle(tr("Online lookup"));
        box.setText(lookupNoticeText());
        QPushButton* pAccept = box.addButton(tr("Accept"), QMessageBox::AcceptRole);
        QPushButton* pCancel = box.addButton(tr("Cancel"), QMessageBox::RejectRole);
        box.setDefaultButton(pCancel);
        box.setEscapeButton(pCancel);
        box.exec();
        return box.clickedButton() == pAccept;
    });
}

} // namespace musicbrainz
} // namespace mixxx

// End of AI-generated text.
