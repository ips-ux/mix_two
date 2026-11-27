// AI-generated (Claude Code). Start of AI-generated text.

#include "library/export/legacystickdialogs.h"

#include <QCheckBox>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>
#include <QRandomGenerator>
#include <QStorageInfo>

#include "library/rekordbox/devicelibraryplusstick.h"

namespace mixxx {

namespace {

const QString kGroup = QStringLiteral("[Library]");
const ConfigKey kStopAskingKey(kGroup, QStringLiteral("LegacyStickUpdateStopAsking"));
const ConfigKey kKeepLegacyKey(kGroup, QStringLiteral("LegacyStickUpdateKeepLegacy"));
// The same library ID as the export, so that both write the same rows.
const ConfigKey kMasterDbIdKey(kGroup, QStringLiteral("DeviceLibraryPlusMasterDbId"));

QString title() {
    return QObject::tr("Update the stick");
}

qint64 masterDbIdOf(UserSettingsPointer pConfig) {
    qint64 id = pConfig->getValue<int>(kMasterDbIdKey, 0);
    if (id <= 0) {
        id = QRandomGenerator::global()->bounded(1, 0x7fffffff);
        pConfig->setValue<int>(kMasterDbIdKey, static_cast<int>(id));
        pConfig->save();
    }
    return id;
}

class MessageBoxUi : public LegacyStickUi {
  public:
    MessageBoxUi(QWidget* pParent, const QString& stickRoot)
            : m_pParent(pParent), m_stickRoot(stickRoot) {
    }

    bool askUpdate(bool* pStopAsking) override {
        QMessageBox box(QMessageBox::Question,
                title(),
                QObject::tr("The stick %1 has only the older device library (export.pdb). "
                            "Mix Two can add a Device Library Plus with the same tracks and "
                            "playlists. Do you want to update the stick?")
                        .arg(QDir::toNativeSeparators(m_stickRoot)),
                QMessageBox::NoButton,
                m_pParent);
        QPushButton* pUpdate = box.addButton(QObject::tr("Update"), QMessageBox::AcceptRole);
        box.addButton(QObject::tr("Not now"), QMessageBox::RejectRole);
        box.setDefaultButton(pUpdate);
        auto* pCheckBox = new QCheckBox(QObject::tr("Stop asking in the future"), &box);
        box.setCheckBox(pCheckBox);
        box.exec();
        *pStopAsking = pCheckBox->isChecked();
        return box.clickedButton() == pUpdate;
    }

    bool askKeepLegacy(bool initial) override {
        QString text = QObject::tr(
                "Do you want to keep the older device library (export.pdb) on the stick? "
                "Older players can read only the older device library.");
        const QString ext = rekordbox::dlpFindPathNoCase(
                m_stickRoot, QStringLiteral("PIONEER/rekordbox/exportExt.pdb"));
        if (!ext.isEmpty()) {
            text += QStringLiteral("\n\n") +
                    QObject::tr("If you remove it, the stick loses the My Tag data of "
                                "exportExt.pdb. The backup on this computer keeps a copy.");
        }
        QMessageBox box(QMessageBox::Question, title(), text, QMessageBox::NoButton, m_pParent);
        QPushButton* pKeep = box.addButton(QObject::tr("Keep"), QMessageBox::AcceptRole);
        QPushButton* pRemove = box.addButton(QObject::tr("Remove"), QMessageBox::DestructiveRole);
        box.setDefaultButton(initial ? pKeep : pRemove);
        box.exec();
        return box.clickedButton() != pRemove;
    }

    bool showSummary(const rekordbox::LegacyToDlpResult& result) override {
        QString text;
        if (result.ok) {
            text = QObject::tr("Mix Two updated the stick. The Device Library Plus has %1 "
                               "tracks, %2 playlists and %3 histories.")
                           .arg(result.trackCount)
                           .arg(result.playlistCount)
                           .arg(result.historyCount);
            text += QLatin1Char(' ') +
                    (result.legacyKept
                                    ? QObject::tr("The older device library stays on the stick.")
                                    : QObject::tr("Mix Two removed the older device library "
                                                  "from the stick."));
            if (!result.errorMessage.isEmpty()) {
                text += QStringLiteral("\n\n") + result.errorMessage;
            }
        } else {
            text = result.errorMessage;
        }
        if (!result.backupFolder.isEmpty()) {
            text += QStringLiteral("\n\n") +
                    QObject::tr("Backup: %1").arg(QDir::toNativeSeparators(result.backupFolder));
        }
        QMessageBox box(result.ok ? QMessageBox::Information : QMessageBox::Warning,
                title(),
                text,
                QMessageBox::NoButton,
                m_pParent);
        QPushButton* pBack = box.addButton(QObject::tr("Back"), QMessageBox::ActionRole);
        QPushButton* pDone = box.addButton(QObject::tr("Done"), QMessageBox::AcceptRole);
        box.setDefaultButton(pDone);
        box.exec();
        return box.clickedButton() == pBack;
    }

  private:
    QWidget* const m_pParent;
    const QString m_stickRoot;
};

} // namespace

bool legacyStickUpdateStopAsking(UserSettingsPointer pConfig) {
    return pConfig->getValue<bool>(kStopAskingKey, false);
}

LegacyStickFlowOutcome runLegacyStickUpdate(
        QWidget* pParent, UserSettingsPointer pConfig, const QString& stickRoot) {
    LegacyStickPrefs prefs;
    prefs.stopAsking = legacyStickUpdateStopAsking(pConfig);
    prefs.keepLegacy = pConfig->getValue<bool>(kKeepLegacyKey, true);

    rekordbox::LegacyToDlpOptions options;
    options.backupBaseDir = pConfig->getSettingsPath() + QStringLiteral("/device-backups");
    const QString label = QStorageInfo(stickRoot).name();
    options.deviceName = label.isEmpty() ? QStringLiteral("Mix Two") : label;
    options.now = QDateTime::currentDateTime();
    options.masterDbId = masterDbIdOf(pConfig);

    MessageBoxUi ui(pParent, stickRoot);
    const LegacyStickFlowOutcome outcome =
            runLegacyStickFlow(prefs, ui, makeLegacyStickApply(stickRoot, options));
    if (outcome.prefsToStore) {
        pConfig->setValue<bool>(kStopAskingKey, outcome.prefsToStore->stopAsking);
        pConfig->setValue<bool>(kKeepLegacyKey, outcome.prefsToStore->keepLegacy);
        pConfig->save();
    }
    return outcome;
}

} // namespace mixxx

// End of AI-generated text.
