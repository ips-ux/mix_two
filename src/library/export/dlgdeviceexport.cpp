#include "library/export/dlgdeviceexport.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSet>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QStyle>
#include <QVBoxLayout>

#include "library/export/devicelibraryplusexportrequest.h"
#include "library/export/trackselectionlists.h"
#include "library/rekordbox/devicelibraryplusstick.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "library/trackset/crate/crateid.h"
#include "moc_dlgdeviceexport.cpp"

namespace mixxx {

namespace {

/// True when two folder paths are the same folder (the file names of Windows
/// do not depend on the case).
bool isSamePath(const QString& a, const QString& b) {
#ifdef Q_OS_WIN
    const Qt::CaseSensitivity cs = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity cs = Qt::CaseSensitive;
#endif
    return QDir::cleanPath(a).compare(QDir::cleanPath(b), cs) == 0;
}

/// True for the volume of the operating system. The dialog never offers it as
/// a stick. On Windows this is also the drive that holds the Windows folder.
bool isSystemVolume(const QStorageInfo& volume) {
    if (volume.rootPath() == QStorageInfo::root().rootPath()) {
        return true;
    }
#ifdef Q_OS_WIN
    const QString windowsFolder = qEnvironmentVariable("SystemRoot");
    if (!windowsFolder.isEmpty() &&
            volume.rootPath() == QStorageInfo(windowsFolder).rootPath()) {
        return true;
    }
#endif
    return false;
}

QString fileSystemName(const QString& root) {
    return QString::fromLatin1(QStorageInfo(root).fileSystemType());
}

/// The text about a file system that players cannot read, else empty. The
/// dialog shows it with the stick, and the question before the export shows
/// it again.
QString fileSystemWarning(const rekordbox::DlpStickInfo& info) {
    if (info.fileSystem != rekordbox::DlpFileSystem::NotForPlayers) {
        return QString();
    }
    return rekordbox::dlpMessageFileSystemNotForPlayers(fileSystemName(info.root));
}

// AI-generated (Claude Code). Start of AI-generated text.
/// The size of the warning icon next to the text about the stick.
constexpr int kWarningIconSize = 24;
// End of AI-generated text.

QString toNativePath(const QString& path) {
    return QDir::toNativeSeparators(path);
}

} // namespace

DlgDeviceExport::DlgDeviceExport(
        QWidget* parent,
        UserSettingsPointer pConfig,
        TrackCollectionManager* pTrackCollectionManager)
        : QDialog(parent),
          m_pConfig{std::move(pConfig)},
          m_pTrackCollectionManager{pTrackCollectionManager},
          m_previousStickIndex(-1) {
    setWindowTitle(tr("Export to a USB stick"));

    // The stick
    m_pStickCombo = make_parented<QComboBox>(this);
    m_pStickCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_pStickInfoLabel = make_parented<QLabel>(this);
    m_pStickInfoLabel->setWordWrap(true);
    // AI-generated (Claude Code). Start of AI-generated text.
    // The warning icon for a file system that players cannot read. It shows
    // next to the text, and only when the text is about that file system.
    m_pStickWarningIcon = make_parented<QLabel>(this);
    m_pStickWarningIcon->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxWarning)
                                           .pixmap(kWarningIconSize, kWarningIconSize));
    m_pStickWarningIcon->setAlignment(Qt::AlignTop);
    m_pStickWarningIcon->setToolTip(
            tr("Players cannot read the file system of this stick."));
    m_pStickWarningIcon->setVisible(false);
    // End of AI-generated text.
    connect(m_pStickCombo,
            QOverload<int>::of(&QComboBox::activated),
            this,
            &DlgDeviceExport::stickComboActivated);
    auto pStickGroup = make_parented<QGroupBox>(tr("Stick"), this);
    auto pStickLayout = make_parented<QVBoxLayout>(pStickGroup.get());
    pStickLayout->addWidget(m_pStickCombo);
    auto pStickInfoLayout = std::make_unique<QHBoxLayout>();
    pStickInfoLayout->addWidget(m_pStickWarningIcon);
    pStickInfoLayout->addWidget(m_pStickInfoLabel, 1);
    pStickLayout->addLayout(pStickInfoLayout.release());

    // The tracks
    m_pWholeLibraryRadio = make_parented<QRadioButton>(tr("Whole library"), this);
    m_pWholeLibraryRadio->setChecked(true);
    m_pCratesAndPlaylistsRadio =
            make_parented<QRadioButton>(tr("Selected crates and playlists"), this);
    // AI-generated (Claude Code). Start of AI-generated text.
    // The mode for the "Export Track" item of the track menu. It shows only
    // when the dialog has tracks.
    m_pSelectedTracksRadio = make_parented<QRadioButton>(this);
    m_pSelectedTracksRadio->setVisible(false);
    // End of AI-generated text.
    auto pCratesLabel = make_parented<QLabel>(tr("Crates"), this);
    m_pCratesList = make_parented<QListWidget>(this);
    m_pCratesList->setSelectionMode(QListWidget::ExtendedSelection);
    auto pPlaylistsLabel = make_parented<QLabel>(tr("Playlists"), this);
    m_pPlaylistsList = make_parented<QListWidget>(this);
    m_pPlaylistsList->setSelectionMode(QListWidget::ExtendedSelection);
    connect(m_pWholeLibraryRadio,
            &QRadioButton::toggled,
            this,
            &DlgDeviceExport::updateModeAndFormat);
    auto pTracksGroup = make_parented<QGroupBox>(tr("Tracks"), this);
    auto pTracksLayout = make_parented<QGridLayout>(pTracksGroup.get());
    pTracksLayout->addWidget(m_pWholeLibraryRadio, 0, 0, 1, 2);
    pTracksLayout->addWidget(m_pSelectedTracksRadio, 1, 0, 1, 2);
    pTracksLayout->addWidget(m_pCratesAndPlaylistsRadio, 2, 0, 1, 2);
    pTracksLayout->addWidget(pCratesLabel, 3, 0);
    pTracksLayout->addWidget(pPlaylistsLabel, 3, 1);
    pTracksLayout->addWidget(m_pCratesList, 4, 0);
    pTracksLayout->addWidget(m_pPlaylistsList, 4, 1);

    // The format
    m_pPlusCheckBox = make_parented<QCheckBox>(tr("Device Library Plus"), this);
    m_pPlusCheckBox->setChecked(true);
    connect(m_pPlusCheckBox,
            &QCheckBox::toggled,
            this,
            &DlgDeviceExport::updateModeAndFormat);
    m_pLegacyCheckBox = make_parented<QCheckBox>(tr("Device Library"), this);
    m_pLegacyCheckBox->setChecked(false);
    m_pLegacyCheckBox->setEnabled(false);
    m_pLegacyCheckBox->setToolTip(tr("Mix Two cannot write this format at this time."));
    auto pFormatGroup = make_parented<QGroupBox>(tr("Format"), this);
    auto pFormatLayout = make_parented<QVBoxLayout>(pFormatGroup.get());
    pFormatLayout->addWidget(m_pPlusCheckBox);
    pFormatLayout->addWidget(m_pLegacyCheckBox);

    // The buttons
    m_pExportButton = make_parented<QPushButton>(tr("Export"), this);
    m_pExportButton->setDefault(true);
    connect(m_pExportButton, &QPushButton::clicked, this, &DlgDeviceExport::exportRequested);
    auto pCancelButton = make_parented<QPushButton>(tr("Cancel"), this);
    connect(pCancelButton, &QPushButton::clicked, this, &QDialog::reject);
    auto pButtonBarLayout = std::make_unique<QHBoxLayout>();
    pButtonBarLayout->addStretch(1);
    pButtonBarLayout->addWidget(m_pExportButton);
    pButtonBarLayout->addWidget(pCancelButton);

    auto pLayout = make_parented<QVBoxLayout>(this);
    pLayout->addWidget(pStickGroup);
    pLayout->addWidget(pTracksGroup, 1);
    pLayout->addWidget(pFormatGroup);
    pLayout->addLayout(pButtonBarLayout.release());

    updateModeAndFormat();
}

void DlgDeviceExport::refresh() {
    populateCrateList(m_pCratesList, *m_pTrackCollectionManager->internalCollection());
    populatePlaylistList(m_pPlaylistsList, *m_pTrackCollectionManager->internalCollection());
    fillStickCombo(currentStickRoot());
    updateModeAndFormat();
}

// AI-generated (Claude Code). Start of AI-generated text.
void DlgDeviceExport::setInitialTracks(const QList<TrackId>& trackIds) {
    m_trackIds.clear();
    QSet<TrackId> known;
    for (const TrackId& trackId : trackIds) {
        if (trackId.isValid() && !known.contains(trackId)) {
            known.insert(trackId);
            m_trackIds.append(trackId);
        }
    }
    clearCratesAndPlaylistsSelection();
    updateSelectedTracksRadio();
    if (m_trackIds.isEmpty()) {
        m_pWholeLibraryRadio->setChecked(true);
    } else {
        m_pSelectedTracksRadio->setChecked(true);
    }
    updateModeAndFormat();
}

void DlgDeviceExport::updateSelectedTracksRadio() {
    m_pSelectedTracksRadio->setVisible(!m_trackIds.isEmpty());
    m_pSelectedTracksRadio->setText(
            tr("Selected tracks (%n)", nullptr, static_cast<int>(m_trackIds.size())));
}

void DlgDeviceExport::clearCratesAndPlaylistsSelection() {
    m_pCratesList->clearSelection();
    m_pPlaylistsList->clearSelection();
}
// End of AI-generated text.

void DlgDeviceExport::setInitialSelection(
        std::optional<CrateId> crateId, std::optional<int> playlistId) {
    // AI-generated (Claude Code). Start of AI-generated text.
    // A choice of a crate or a playlist replaces the tracks.
    m_trackIds.clear();
    // End of AI-generated text.
    updateSelectedTracksRadio();
    selectInitialListItems(m_pCratesList, m_pPlaylistsList, crateId, playlistId);
    // Without a crate and a playlist the mode is "Whole library".
    if (crateId || playlistId) {
        m_pCratesAndPlaylistsRadio->setChecked(true);
    } else {
        m_pWholeLibraryRadio->setChecked(true);
    }
    updateModeAndFormat();
}

void DlgDeviceExport::fillStickCombo(const QString& wantedRoot) {
    m_pStickCombo->clear();
    const QList<QStorageInfo> volumes = QStorageInfo::mountedVolumes();
    for (const QStorageInfo& volume : volumes) {
        if (!volume.isValid() || !volume.isReady() || volume.isReadOnly() ||
                isSystemVolume(volume)) {
            continue;
        }
        const QString name = volume.name();
        const QString path = toNativePath(volume.rootPath());
        m_pStickCombo->addItem(name.isEmpty() ? path : QStringLiteral("%1 (%2)").arg(name, path),
                volume.rootPath());
    }

    // The folder that the user chose with "Browse..." stays in the list, unless
    // it is the root of a volume in the list.
    if (!m_browsedFolder.isEmpty()) {
        bool isListed = false;
        for (int i = 0; i < m_pStickCombo->count(); ++i) {
            if (isSamePath(m_pStickCombo->itemData(i).toString(), m_browsedFolder)) {
                isListed = true;
                break;
            }
        }
        if (!isListed) {
            m_pStickCombo->addItem(toNativePath(m_browsedFolder), m_browsedFolder);
        }
    }

    // The last item.
    m_pStickCombo->addItem(tr("Browse..."));

    int index = m_pStickCombo->count() > 1 ? 0 : -1;
    if (!wantedRoot.isEmpty()) {
        for (int i = 0; i < m_pStickCombo->count() - 1; ++i) {
            if (isSamePath(m_pStickCombo->itemData(i).toString(), wantedRoot)) {
                index = i;
                break;
            }
        }
    }
    m_pStickCombo->setCurrentIndex(index);
    m_previousStickIndex = index;
    updateStickInfo();
}

QString DlgDeviceExport::currentStickRoot() const {
    const int index = m_pStickCombo->currentIndex();
    // The last item is "Browse...".
    if (index < 0 || index >= m_pStickCombo->count() - 1) {
        return QString();
    }
    return m_pStickCombo->itemData(index).toString();
}

void DlgDeviceExport::stickComboActivated(int index) {
    if (index == m_pStickCombo->count() - 1) {
        browseForStick();
        return;
    }
    m_previousStickIndex = index;
    updateStickInfo();
}

void DlgDeviceExport::browseForStick() {
    QString startFolder = currentStickRoot();
    if (startFolder.isEmpty()) {
        startFolder = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    }
    const QString folder = QFileDialog::getExistingDirectory(
            this,
            tr("Choose the stick"),
            startFolder,
            QFileDialog::ShowDirsOnly);
    if (folder.isEmpty()) {
        m_pStickCombo->setCurrentIndex(m_previousStickIndex);
        return;
    }
    // The combo keeps the chosen folder and selects it.
    m_browsedFolder = folder;
    fillStickCombo(folder);
}

void DlgDeviceExport::updateStickInfo() {
    const QString root = currentStickRoot();
    if (root.isEmpty()) {
        m_pStickInfoLabel->clear();
        m_pStickWarningIcon->setVisible(false);
        return;
    }
    const rekordbox::DlpStickInfo info = rekordbox::dlpStickInfo(root);
    QStringList lines;
    if (!info.isReady || info.isReadOnly) {
        lines << rekordbox::dlpMessageCannotWrite();
    }
    const QString fileSystemText = fileSystemWarning(info);
    if (!fileSystemText.isEmpty()) {
        lines << fileSystemText;
    }
    m_pStickWarningIcon->setVisible(!fileSystemText.isEmpty());
    if (info.hasLegacyLibrary) {
        lines << rekordbox::dlpMessageHasLegacyLibrary();
    }
    if (info.hasPlusLibrary) {
        lines << tr("This stick has a Device Library Plus. Mix Two adds the tracks to it.");
    }
    m_pStickInfoLabel->setText(lines.join(QLatin1Char('\n')));
}

void DlgDeviceExport::updateModeAndFormat() {
    const bool selectionMode = m_pCratesAndPlaylistsRadio->isChecked();
    m_pCratesList->setEnabled(selectionMode);
    m_pPlaylistsList->setEnabled(selectionMode);
    // Without a format there is nothing to export.
    m_pExportButton->setEnabled(m_pPlusCheckBox->isChecked());
}

void DlgDeviceExport::exportRequested() {
    const QString root = currentStickRoot();
    if (root.isEmpty()) {
        QMessageBox::warning(this,
                tr("Export to a USB stick"),
                tr("Choose a stick."));
        return;
    }

    const bool selectionMode = m_pCratesAndPlaylistsRadio->isChecked();
    if (selectionMode && m_pCratesList->selectedItems().isEmpty() &&
            m_pPlaylistsList->selectedItems().isEmpty()) {
        QMessageBox::warning(this,
                tr("Export to a USB stick"),
                tr("Select one or more crates or playlists."));
        return;
    }

    // Read the stick again: it can be different from the time of the choice.
    const rekordbox::DlpStickInfo info = rekordbox::dlpStickInfo(root);
    const QString fileSystemText = fileSystemWarning(info);
    if (info.hasPlusLibrary || !fileSystemText.isEmpty()) {
        QString question;
        if (!fileSystemText.isEmpty()) {
            question = fileSystemText + QStringLiteral("\n\n");
        }
        if (info.hasPlusLibrary && m_pSelectedTracksRadio->isChecked()) {
            // AI-generated (Claude Code). Start of AI-generated text.
            // The mode "Selected tracks" writes no playlist.
            question += tr("Mix Two adds the selected tracks to the stick. "
                           "Do you want to continue?");
            // End of AI-generated text.
        } else if (info.hasPlusLibrary) {
            question += tr("Playlists on the stick that have the same name get the new "
                           "track list. Do you want to continue?");
        } else {
            question += tr("Do you want to continue?");
        }
        // AI-generated (Claude Code). Start of AI-generated text.
        // A file system that players cannot read gets the warning icon.
        QMessageBox box(fileSystemText.isEmpty() ? QMessageBox::Question : QMessageBox::Warning,
        // End of AI-generated text.
                tr("Export to a USB stick"),
                question,
                QMessageBox::Yes | QMessageBox::No,
                this);
        box.setDefaultButton(QMessageBox::No);
        if (box.exec() != QMessageBox::Yes) {
            return;
        }
    }

    emit startExport(makeRequest(root));
    accept();
}

// AI-generated (Claude Code). Start of AI-generated text.
QSharedPointer<DeviceLibraryPlusExportRequest> DlgDeviceExport::makeRequest(
        const QString& stickRoot) const {
    auto pRequest = QSharedPointer<DeviceLibraryPlusExportRequest>::create();
    pRequest->stickRoot = stickRoot;
    if (m_pSelectedTracksRadio->isChecked()) {
        for (const TrackId& trackId : m_trackIds) {
            pRequest->trackIds.insert(trackId.toVariant().toInt());
        }
    } else if (m_pCratesAndPlaylistsRadio->isChecked()) {
        for (auto* pItem : m_pCratesList->selectedItems()) {
            pRequest->crateIds.insert(CrateId{pItem->data(Qt::UserRole)});
        }
        for (auto* pItem : m_pPlaylistsList->selectedItems()) {
            pRequest->playlistIds.insert(pItem->data(Qt::UserRole).toInt());
        }
    }
    return pRequest;
}
// End of AI-generated text.

} // namespace mixxx
