#include "preferences/dialog/dlgpreferences.h"

#include <QDialog>
#include <QEvent>
#include <QFile>
#include <QFontMetrics>
#include <QFrame>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QStackedWidget>
#include <QStyle>
#include <QTabWidget>
#include <QTimer>
#include <QTreeWidgetItemIterator>
#include <QVariantAnimation>
#include <QWindow>
#include <QtGlobal>

#include "controllers/controller.h"
#include "controllers/controllermanager.h"
#include "controllers/dlgprefcontrollers.h"
#include "library/library.h"
#include "library/trackcollectionmanager.h"
#include "moc_dlgpreferences.cpp"
#include "preferences/dialog/dlgpreflibrary.h"
#include "preferences/dialog/dlgprefsound.h"
#include "preferences/dialog/prefraildelegate.h"
#include "util/desktophelper.h"
#include "util/widgethelper.h"

#ifdef __VINYLCONTROL__
#include "preferences/dialog/dlgprefvinyl.h"
#endif // __VINYLCONTROL__

#include "preferences/dialog/dlgprefautodj.h"
#include "preferences/dialog/dlgprefcolors.h"
#include "preferences/dialog/dlgprefcontributions.h"
#include "preferences/dialog/dlgprefdeck.h"
#include "preferences/dialog/dlgprefeffects.h"
#include "preferences/dialog/dlgprefinterface.h"
#include "preferences/dialog/dlgprefmixer.h"
#include "preferences/dialog/dlgprefwaveform.h"
#include "util/cmdlineargs.h"
#include "waveform/waveformwidgetfactory.h"

#ifdef __BROADCAST__
#include "preferences/dialog/dlgprefbroadcast.h"
#endif // __BROADCAST__

#include "preferences/dialog/dlgprefbeats.h"
#include "preferences/dialog/dlgprefkey.h"
#include "preferences/dialog/dlgprefrecord.h"
#include "preferences/dialog/dlgprefreplaygain.h"

#ifdef __MODPLUG__
#include "preferences/dialog/dlgprefmodplug.h"
#endif // __MODPLUG__

// AI-generated (Claude Code). Start of AI-generated text.
namespace {

using namespace mixxx::preferences;

constexpr int kRailExpandedWidth = 224;
constexpr int kRailCollapsedWidth = 62;
constexpr int kRailAnimationMs = 160;
constexpr QSize kMinimumDialogSize(760, 520);
// The window has no frame. These margins hold the painted shadow.
constexpr int kShadowSize = 16;
constexpr qreal kWindowRadius = 16.0;
constexpr int kLayoutMargin = 14;
// The width of the band along the window edge that resizes the window.
constexpr int kResizeGrip = 8;
const ConfigKey kRailCollapsedCfgKey("[Preferences]", "rail_collapsed");
const QString kControllersIcon = QStringLiteral("ic_preferences_controllers.svg");
const QString kContributionsIcon = QStringLiteral("ic_preferences_contributions.svg");

/// The window is always dark. Pages paint some items from the palette, so the
/// palette must agree with the style sheet.
QPalette glassPalette() {
    const QColor pane = QColor::fromRgb(glass::kPane);
    const QColor text = QColor::fromRgb(glass::kText);
    const QColor accent = QColor::fromRgb(glass::kAccent);
    const QColor disabled(0x6b, 0x7c, 0x82);
    QPalette palette;
    palette.setColor(QPalette::Window, pane);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, QColor(0x16, 0x1d, 0x22));
    palette.setColor(QPalette::AlternateBase, QColor(0x20, 0x29, 0x2f));
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, QColor(0x2f, 0x3b, 0x44));
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::ToolTipBase, QColor(0x2a, 0x35, 0x3d));
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::Highlight, accent);
    palette.setColor(QPalette::HighlightedText, QColor(0x06, 0x30, 0x2c));
    palette.setColor(QPalette::PlaceholderText, disabled);
    palette.setColor(QPalette::Link, accent);
    palette.setColor(QPalette::LinkVisited, accent);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
    palette.setColor(QPalette::Disabled, QPalette::Text, disabled);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
    return palette;
}

QString loadGlassStyleSheet() {
    QFile file(QStringLiteral(":/preferences/glass.qss"));
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to load the preferences style sheet";
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

} // namespace
// End of AI-generated text.

DlgPreferences::DlgPreferences(
        std::shared_ptr<mixxx::ScreensaverManager> pScreensaverManager,
        std::shared_ptr<mixxx::skin::SkinLoader> pSkinLoader,
        std::shared_ptr<SoundManager> pSoundManager,
        std::shared_ptr<ControllerManager> pControllerManager,
        std::shared_ptr<VinylControlManager> pVCManager,
        std::shared_ptr<EffectsManager> pEffectsManager,
        std::shared_ptr<SettingsManager> pSettingsManager,
        std::shared_ptr<Library> pLibrary,
        bool includeWaveformPreferences,
        QWidget* pParent)
        : QDialog(pParent),
          m_allPages(),
          m_pConfig(pSettingsManager->settings()),
          m_pControllerManager(pControllerManager),
          m_pageSizeHint(QSize(0, 0)) {
    setupUi(this);
    // AI-generated (Claude Code). Start of AI-generated text.
    // Before the pages are constructed, because they read the palette to
    // select their icons and link colors.
    setPalette(glassPalette());
    setStyleSheet(loadGlassStyleSheet());
    setMinimumSize(kMinimumDialogSize);
    // The window belongs to the main window: it stays in front of it, has no
    // taskbar entry and blocks the main window while it is open. It has no
    // frame; paintEvent() paints the rounded body and its shadow.
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    if (pParent) {
        setWindowModality(Qt::WindowModal);
    }
    setMouseTracking(true);
    shellLayout->setContentsMargins(kShadowSize + kLayoutMargin,
            kShadowSize + kLayoutMargin,
            kShadowSize + kLayoutMargin,
            kShadowSize + kLayoutMargin);
    closeButton->setIcon(QIcon(QStringLiteral(":/preferences/glass/close.svg")));
    closeButton->setToolTip(tr("Close (Esc)"));
    closeButton->setAccessibleName(tr("Close"));
    closeButton->setAutoRaise(true);
    connect(closeButton, &QToolButton::clicked, this, &DlgPreferences::reject);
    // End of AI-generated text.

    // Add '&' to default button labels to always have Alt shortcuts, indpependent
    // of operating system.
    //: Preferences standard buttons: consider the other buttons to choose a unique Alt hotkey (&)
    helpButton->setText(tr("&Help"));
    //: Preferences standard buttons: consider the other buttons to choose a unique Alt hotkey (&)
    buttonBox->button(QDialogButtonBox::RestoreDefaults)->setText(tr("&Restore Defaults"));
    //: Preferences standard buttons: consider the other buttons to choose a unique Alt hotkey (&)
    buttonBox->button(QDialogButtonBox::Apply)->setText(tr("&Apply"));
    //: Preferences standard buttons: consider the other buttons to choose a unique Alt hotkey (&)
    buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("&Close"));
    //: Preferences standard buttons: consider the other buttons to choose a unique Alt hotkey (&)
    buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Apply and cl&ose"));

    connect(buttonBox,
            QOverload<QAbstractButton*>::of(&QDialogButtonBox::clicked),
            this,
            &DlgPreferences::slotButtonPressed);
    connect(helpButton, &QPushButton::clicked, this, &DlgPreferences::slotHelpClicked);

    connect(contentsTreeWidget,
            &QTreeWidget::currentItemChanged,
            this,
            &DlgPreferences::changePage);

    setupRail();

    while (pagesWidget->count() > 0) {
        pagesWidget->removeWidget(pagesWidget->currentWidget());
    }

    // AI-generated (Claude Code). Start of AI-generated text.
    // The window is always dark, so it uses the light icons.
    m_iconsPath.setPath(":/images/preferences/light/");
    // End of AI-generated text.

    // Construct page widgets and associated sidebar items
    m_pSoundDlg = std::make_unique<DlgPrefSound>(this, pSoundManager, m_pConfig);
    m_soundPage = PreferencesPage(
            m_pSoundDlg.get(),
            new QTreeWidgetItem(contentsTreeWidget, QTreeWidgetItem::Type));
    addPageWidget(m_soundPage,
            tr("Sound Hardware"),
            "ic_preferences_soundhardware.svg");

    DlgPrefLibrary* plibraryPage = new DlgPrefLibrary(this, m_pConfig, pLibrary);
    connect(plibraryPage,
            &DlgPrefLibrary::scanLibrary,
            pLibrary->trackCollectionManager(),
            &TrackCollectionManager::startLibraryScan);
    addPageWidget(PreferencesPage(plibraryPage,
                          new QTreeWidgetItem(contentsTreeWidget, QTreeWidgetItem::Type)),
            tr("Library"),
            "ic_preferences_library.svg");

    m_pControllersRootItem =
            new QTreeWidgetItem(contentsTreeWidget, QTreeWidgetItem::Type);
    m_pControllersDlg = new DlgPrefControllers(
            this, m_pConfig, pControllerManager, m_pControllersRootItem);
    addPageWidget(PreferencesPage(m_pControllersDlg,
                          m_pControllersRootItem),
            tr("Controllers"),
            "ic_preferences_controllers.svg");

#ifdef __VINYLCONTROL__
    // It's important for this to be before the connect for wsound.
    // TODO(rryan) determine why/if this is still true
    addPageWidget(PreferencesPage(
                          new DlgPrefVinyl(this, pVCManager, m_pConfig),
                          new QTreeWidgetItem(contentsTreeWidget, QTreeWidgetItem::Type)),
            tr("Vinyl Control"),
            "ic_preferences_vinyl.svg");
#endif // __VINYLCONTROL__

#ifdef MIXXX_USE_QML
    if (!CmdlineArgs::Instance().isQml())
#endif
    {
        DlgPrefInterface* pInterfacePage = new DlgPrefInterface(this,
                pScreensaverManager,
                pSkinLoader,
                m_pConfig);
        connect(pInterfacePage,
                &DlgPrefInterface::tooltipModeChanged,
                this,
                &DlgPreferences::tooltipModeChanged);
        connect(pInterfacePage,
                &DlgPrefInterface::reloadUserInterface,
                this,
                &DlgPreferences::reloadUserInterface,
                Qt::DirectConnection);
        connect(pInterfacePage,
                &DlgPrefInterface::menuBarAutoHideChanged,
                this,
                &DlgPreferences::menuBarAutoHideChanged,
                Qt::DirectConnection);
        addPageWidget(PreferencesPage(pInterfacePage,
                              new QTreeWidgetItem(
                                      contentsTreeWidget, QTreeWidgetItem::Type)),
                tr("Interface"),
                "ic_preferences_interface.svg");
    }

    // The shared native page is used by both legacy and LateNightQML. In QML
    // mode QmlApplication initializes the factory as the preferences backend
    // before constructing this dialog.
    if (includeWaveformPreferences && WaveformWidgetFactory::isCreated()) {
        addPageWidget(PreferencesPage(
                              new DlgPrefWaveform(this, m_pConfig, pLibrary),
                              new QTreeWidgetItem(contentsTreeWidget, QTreeWidgetItem::Type)),
                tr("Waveforms"),
                "ic_preferences_waveforms.svg");
    }

    addPageWidget(PreferencesPage(
                          new DlgPrefColors(this, m_pConfig, pLibrary),
                          new QTreeWidgetItem(contentsTreeWidget, QTreeWidgetItem::Type)),
            tr("Colors"),
            "ic_preferences_colors.svg");

    addPageWidget(PreferencesPage(
                          new DlgPrefDeck(this, m_pConfig),
                          new QTreeWidgetItem(contentsTreeWidget, QTreeWidgetItem::Type)),
            tr("Decks"),
            "ic_preferences_decks.svg");

    addPageWidget(PreferencesPage(
                          new DlgPrefMixer(this, pEffectsManager, m_pConfig),
                          new QTreeWidgetItem(contentsTreeWidget, QTreeWidgetItem::Type)),
            tr("Mixer"),
            "ic_preferences_crossfader.svg");

    addPageWidget(PreferencesPage(
                          new DlgPrefEffects(this, m_pConfig, pEffectsManager),
                          new QTreeWidgetItem(contentsTreeWidget, QTreeWidgetItem::Type)),
            tr("Effects"),
            "ic_preferences_effects.svg");

    addPageWidget(PreferencesPage(
                          new DlgPrefAutoDJ(this, m_pConfig),
                          new QTreeWidgetItem(contentsTreeWidget, QTreeWidgetItem::Type)),
            tr("Auto DJ"),
            "ic_preferences_autodj.svg");

#ifdef __BROADCAST__
    addPageWidget(PreferencesPage(
                          new DlgPrefBroadcast(this, pSettingsManager->broadcastSettings()),
                          new QTreeWidgetItem(contentsTreeWidget, QTreeWidgetItem::Type)),
            tr("Live Broadcasting"),
            "ic_preferences_broadcast.svg");
#endif // __BROADCAST__

    addPageWidget(PreferencesPage(
                          new DlgPrefRecord(this, m_pConfig),
                          new QTreeWidgetItem(contentsTreeWidget, QTreeWidgetItem::Type)),
            tr("Recording"),
            "ic_preferences_recording.svg");

    addPageWidget(PreferencesPage(
                          new DlgPrefBeats(this, m_pConfig),
                          new QTreeWidgetItem(contentsTreeWidget, QTreeWidgetItem::Type)),
            tr("Beat Detection"),
            "ic_preferences_bpmdetect.svg");

    addPageWidget(PreferencesPage(
                          new DlgPrefKey(this, m_pConfig),
                          new QTreeWidgetItem(contentsTreeWidget, QTreeWidgetItem::Type)),
            tr("Key Detection"),
            "ic_preferences_keydetect.svg");
    addPageWidget(PreferencesPage(
                          new DlgPrefReplayGain(this, m_pConfig),
                          new QTreeWidgetItem(contentsTreeWidget, QTreeWidgetItem::Type)),
            tr("Normalization"),
            "ic_preferences_replaygain.svg");

#ifdef __MODPLUG__
    addPageWidget(PreferencesPage(
                          new DlgPrefModplug(this, m_pConfig),
                          new QTreeWidgetItem(contentsTreeWidget, QTreeWidgetItem::Type)),
            tr("Modplug Decoder"),
            "ic_preferences_modplug.svg");
#endif // __MODPLUG__

    // AI-generated (Claude Code). Start of AI-generated text.
    addPageWidget(PreferencesPage(
                          new DlgPrefContributions(this),
                          new QTreeWidgetItem(contentsTreeWidget, QTreeWidgetItem::Type)),
            tr("Contributions"),
            kContributionsIcon);
    // End of AI-generated text.

    buildRailGroups();

    // Find accept and apply buttons
    const auto buttons = buttonBox->buttons();
    for (QAbstractButton* button : buttons) {
        QDialogButtonBox::ButtonRole role = buttonBox->buttonRole(button);
        if (role == QDialogButtonBox::ButtonRole::ApplyRole) {
            m_pApplyButton = button;
        } else if (role == QDialogButtonBox::ButtonRole::AcceptRole) {
            m_pAcceptButton = button;
            // AI-generated (Claude Code). Start of AI-generated text.
            // The style sheet is already set, so the style must read the property again.
            m_pAcceptButton->setProperty("primary", true);
            m_pAcceptButton->style()->unpolish(m_pAcceptButton);
            m_pAcceptButton->style()->polish(m_pAcceptButton);
            // End of AI-generated text.
        }
    }

    labelWarning->hide();
    labelWarningIcon->hide();
    labelWarning->setText(tr(
            "<font color='#ff8a80'><b>Some preferences pages have errors. "
            "To apply the changes please first fix the issues.</b></font>"));
    QIcon icon = style()->standardIcon(QStyle::SP_MessageBoxWarning);
    labelWarningIcon->setPixmap(icon.pixmap(16));

    // Install event handler to generate closeDlg signal
    installEventFilter(this);

    // If we don't call this explicitly, then we default to showing the sound
    // hardware page but the tree item is not selected.
    showSoundHardwarePage();

    connect(m_pControllerManager.get(),
            &ControllerManager::devicesChanged,
            this,
            &DlgPreferences::updateDeviceStatus);
    updateDeviceStatus();
    setRailCollapsed(m_pConfig->getValue(kRailCollapsedCfgKey, false), false);
    contentsTreeWidget->setFocus();
}

DlgPreferences::~DlgPreferences() {
    // store last geometry in mixxx.cfg
    if (m_geometry.size() == 4) {
        m_pConfig->set(ConfigKey("[Preferences]", "geometry"),
                m_geometry.join(","));
    }

    // When DlgPrefControllers is deleted it manually deletes the controller tree items,
    // which makes QTreeWidgetItem trigger this signal. Currently PreferencesPage
    // instances in m_allPages are destroyed along with pDlg objects. This disconnect
    // is kept as a safety measure.
    disconnect(contentsTreeWidget, &QTreeWidget::currentItemChanged, this, &DlgPreferences::changePage);
    // Need to explicitly delete rather than relying on child auto-deletion
    // because otherwise the QStackedWidget will delete the controller
    // preference pages (and DlgPrefControllers dynamically generates and
    // deletes them).
    delete m_pControllersDlg;
}

void DlgPreferences::changePage(QTreeWidgetItem* pCurrent, QTreeWidgetItem* pPrevious) {
    if (!pCurrent) {
        pCurrent = pPrevious;
    }

    // AI-generated (Claude Code). Start of AI-generated text.
    // m_pControllersDlg handles the clicks on its own items.
    // End of AI-generated text.
    if (!m_pControllersDlg->handleTreeItemClick(pCurrent)) {
        for (const PreferencesPage& page : std::as_const(m_allPages)) {
            if (pCurrent == page.pTreeItem) {
                switchToPage(pCurrent->text(0), page.pDlg);
                break;
            }
        }
    }
    updateControllerRowsExpansion();
    updateRailBridge();
    showSearchMatchOnCurrentPage();
}

void DlgPreferences::showSoundHardwarePage(
        std::optional<mixxx::preferences::SoundHardwareTab> tab) {
    switchToPage(m_soundPage.pTreeItem->text(0), m_soundPage.pDlg);
    contentsTreeWidget->setCurrentItem(m_soundPage.pTreeItem);
    if (tab.has_value()) {
        m_pSoundDlg->selectIOTab(*tab);
    }
}

void DlgPreferences::showSoundHardwareInputPage() {
    showSoundHardwarePage(mixxx::preferences::SoundHardwareTab::Input);
}

bool DlgPreferences::eventFilter(QObject* o, QEvent* e) {
    // AI-generated (Claude Code). Start of AI-generated text.
    if (o == searchEdit && e->type() == QEvent::KeyPress) {
        const int key = static_cast<QKeyEvent*>(e)->key();
        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            // The dialog must not get this key: it would push the default button.
            selectFirstSearchMatch();
            return true;
        }
        if (key == Qt::Key_Escape && !searchEdit->text().isEmpty()) {
            searchEdit->clear();
            return true;
        }
        if (key == Qt::Key_Down) {
            contentsTreeWidget->setFocus();
            return true;
        }
    }
    // End of AI-generated text.

    if (o == this) {
        // Send a close signal if dialog is closing
        if (e->type() == QEvent::Hide) {
            onHide();
        }

        if (e->type() == QEvent::Show) {
            onShow();
        }
    }

    // Standard event processing
    return QWidget::eventFilter(o, e);
}

void DlgPreferences::onHide() {
    // Notify children that we are about to hide.
    emit closeDlg();
}

void DlgPreferences::onShow() {
    // init m_geometry
    if (m_geometry.length() < 4) {
        // load default values (optimum size)
        m_geometry = m_pConfig->getValue(
                                      ConfigKey("[Preferences]", "geometry"))
                             .split(",");
        if (m_geometry.length() < 4) {
            // Warning! geometry does NOT include the frame/title.
            QRect defaultGeometry = getDefaultGeometry();
            m_geometry.clear();
            m_geometry.append(QString::number(defaultGeometry.left()));
            m_geometry.append(QString::number(defaultGeometry.top()));
            m_geometry.append(QString::number(defaultGeometry.width()));
            m_geometry.append(QString::number(defaultGeometry.height()));
        }
    }
    int newX = m_geometry[0].toInt();
    int newY = m_geometry[1].toInt();
    int newWidth = m_geometry[2].toInt();
    int newHeight = m_geometry[3].toInt();

    const QScreen* const pScreen =
            mixxx::widgethelper::getScreenForWidgetOrApplication(*this);
    QRect screenAvailableGeometry;
    VERIFY_OR_DEBUG_ASSERT(pScreen) {
        qWarning() << "Assuming screen size of 800x600px.";
        screenAvailableGeometry = QRect(0, 0, 800, 600);
    }
    else {
        screenAvailableGeometry = pScreen->availableGeometry();
    }

    // Make sure the entire window is visible on screen and is not occluded by taskbar
    // Note: Window geometry excludes window decoration
    int windowDecorationWidth = frameGeometry().width() - geometry().width();
    int windowDecorationHeight = frameGeometry().height() - geometry().height();
    if (windowDecorationWidth <= 0) {
        windowDecorationWidth = 2;
    }
    if (windowDecorationHeight <= 0) {
        windowDecorationHeight = 30;
    }
    int availableWidth = screenAvailableGeometry.width() - windowDecorationWidth;
    int availableHeight = screenAvailableGeometry.height() - windowDecorationHeight;
    newWidth = std::min(newWidth, availableWidth);
    newHeight = std::min(newHeight, availableHeight);
    int minX = screenAvailableGeometry.x();
    int minY = screenAvailableGeometry.y();
    int maxX = screenAvailableGeometry.x() + availableWidth - newWidth;
    int maxY = screenAvailableGeometry.y() + availableHeight - newHeight;
    newX = std::clamp(newX, minX, maxX);
    newY = std::clamp(newY, minY, maxY);
    m_geometry[0] = QString::number(newX);
    m_geometry[1] = QString::number(newY);
    m_geometry[2] = QString::number(newWidth);
    m_geometry[3] = QString::number(newHeight);

    // Update geometry with last values
#ifdef __WINDOWS__
    resize(m_geometry[2].toInt(), m_geometry[3].toInt());
#else  // __WINDOWS__
    // On linux, when the window is opened for the first time by the window manager,
    // QT does not have information about the frame size so the offset is zero.
    // As such, the first time it opens the window does not include the offset,
    // so it is moved from the last position it had.
    // Excluding the offset from the saved value tries to fix that.
    int offsetX = geometry().left() - frameGeometry().left();
    int offsetY = geometry().top() - frameGeometry().top();
    newX += offsetX;
    newY += offsetY;
    setGeometry(newX,   // x position
            newY,       // y position
            newWidth,   // width
            newHeight); // height
#endif // __LINUX__ / __MACOS__
    // Move is also needed on linux.
    move(newX, newY);

    // Notify children that we are about to show.
    emit showDlg();

    // AI-generated (Claude Code). Start of AI-generated text.
    updateDeviceStatus();
    // The layout is not final before the window shows.
    QTimer::singleShot(0, this, &DlgPreferences::updateRailBridge);
    // The pages fill their inputs in slotUpdate(), which the showDlg signal
    // starts. The snapshot must come after that.
    QTimer::singleShot(0, this, &DlgPreferences::captureAppliedState);
    // End of AI-generated text.
}

void DlgPreferences::slotButtonPressed(QAbstractButton* pButton) {
    QDialogButtonBox::ButtonRole role = buttonBox->buttonRole(pButton);
    DlgPreferencePage* pCurrentPage = currentPage();
    switch (role) {
    case QDialogButtonBox::ResetRole:
        // Only reset to defaults on the current page.
        if (pCurrentPage) {
            pCurrentPage->slotResetToDefaults();
        }
        break;
    case QDialogButtonBox::ApplyRole:
        emit applyPreferences();
        if (!pendingConfigValidOnAllPages()) {
            return;
        }
        // The pages can change their inputs while they apply.
        QTimer::singleShot(0, this, &DlgPreferences::captureAppliedState);
        break;
    case QDialogButtonBox::AcceptRole:
        emit applyPreferences();
        if (!pendingConfigValidOnAllPages()) {
            return;
        }
        accept();
        // Same as Apply but close the dialog
        break;
    case QDialogButtonBox::RejectRole:
        // reject() asks about changes that are not applied.
        reject();
        break;
    default:
        break;
    }
}

// AI-generated (Claude Code). Start of AI-generated text.
void DlgPreferences::slotHelpClicked() {
    DlgPreferencePage* pCurrentPage = currentPage();
    if (pCurrentPage) {
        QUrl helpUrl = pCurrentPage->helpUrl();
        DEBUG_ASSERT(helpUrl.isValid());
        mixxx::DesktopHelper::openUrl(helpUrl);
    }
}
// End of AI-generated text.

bool DlgPreferences::pendingConfigValidOnAllPages() {
    for (const PreferencesPage& page : std::as_const(m_allPages)) {
        if (page.pDlg && !page.pDlg->okayToClose()) {
            // If any page is not okay to close, eg. with an invalid sound config,
            // switch to it and don't accept.
            // Fixes https://github.com/mixxxdj/mixxx/issues/6077
            // and may help with other pages in the future.
            contentsTreeWidget->setCurrentItem(page.pTreeItem);
            return false;
        }
    }
    return true;
}

void DlgPreferences::addPageWidget(const PreferencesPage& page,
        const QString& pageTitle,
        const QString& iconFile) {
    PreferencesPage pageCopy = page;
    pageCopy.iconFile = iconFile;
    pageCopy.description = descriptionForIcon(iconFile);
    // Configure the tree button linked to the page
    pageCopy.pTreeItem->setIcon(0, QIcon(m_iconsPath.filePath(iconFile)));
    pageCopy.pTreeItem->setText(0, pageTitle);
    pageCopy.pTreeItem->setToolTip(0, pageTitle);
    pageCopy.pTreeItem->setTextAlignment(0, Qt::AlignLeft | Qt::AlignVCenter);
    pageCopy.pTreeItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    connect(this, &DlgPreferences::showDlg, pageCopy.pDlg, &DlgPreferencePage::slotShow);
    connect(this, &DlgPreferences::closeDlg, pageCopy.pDlg, &DlgPreferencePage::slotHide);
    connect(this, &DlgPreferences::showDlg, pageCopy.pDlg, &DlgPreferencePage::slotUpdate);

    connect(this, &DlgPreferences::applyPreferences, pageCopy.pDlg, &DlgPreferencePage::slotApply);
    connect(this,
            &DlgPreferences::cancelPreferences,
            pageCopy.pDlg,
            &DlgPreferencePage::slotCancel);
    connect(this,
            &DlgPreferences::resetToDefaults,
            pageCopy.pDlg,
            &DlgPreferencePage::slotResetToDefaults);

    // Add a new scroll area to the stacked pages widget containing the page
    QScrollArea* sa = new QScrollArea(pagesWidget);
    sa->setWidgetResizable(true);

    // AI-generated (Claude Code). Start of AI-generated text.
    sa->setFrameShape(QFrame::NoFrame);
    sa->setWidget(pageCopy.pDlg);
    // The pane shows through. setWidget() sets the auto fill of the page.
    sa->viewport()->setAutoFillBackground(false);
    pageCopy.pDlg->setAutoFillBackground(false);
    // End of AI-generated text.
    pagesWidget->addWidget(sa);
    m_allPages.append(pageCopy);
    m_searchIndexDirty = true;

    int iframe = 2 * sa->frameWidth();
    m_pageSizeHint = m_pageSizeHint.expandedTo(
            pageCopy.pDlg->sizeHint() + QSize(iframe, iframe));
}

DlgPreferencePage* DlgPreferences::currentPage() {
    QObject* pObject = pagesWidget->currentWidget();
    for (int i = 0; i < 2; ++i) {
        if (pObject == nullptr) {
            return nullptr;
        }
        QObjectList children = pObject->children();
        if (children.isEmpty()) {
            return nullptr;
        }
        pObject = children[0];
    }
    return qobject_cast<DlgPreferencePage*>(pObject);
}

void DlgPreferences::removePageWidget(DlgPreferencePage* pWidget) {
    QWidget* pParent = pWidget->parentWidget();
    VERIFY_OR_DEBUG_ASSERT(pParent) {
        return;
    }

    QWidget* pScrollArea = pParent->parentWidget();
    VERIFY_OR_DEBUG_ASSERT(pScrollArea) {
        return;
    }

    const int index = pagesWidget->indexOf(pScrollArea);
    VERIFY_OR_DEBUG_ASSERT(index >= 0 && index < m_allPages.size()) {
        return;
    }

    m_allPages.removeAt(index);
    m_searchIndexDirty = true;
    pagesWidget->removeWidget(pScrollArea);
    delete pScrollArea;
}

void DlgPreferences::expandTreeItem(QTreeWidgetItem* pItem) {
    contentsTreeWidget->expandItem(pItem);
}

void DlgPreferences::switchToPage(const QString& pageTitle, DlgPreferencePage* pWidget) {
#ifdef __APPLE__
    // According to Apple's Human Interface Guidelines, settings dialogs have to
    // "Update the window’s title to reflect the currently visible pane."
    // This also solves the problem of the changed in terminology, Settings instead
    // of Preferences, since macOS Ventura.
    setWindowTitle(pageTitle);
#endif
    pagesWidget->setCurrentWidget(pWidget->parentWidget()->parentWidget());

    pageTitleLabel->setText(pageTitle);
    for (const PreferencesPage& page : std::as_const(m_allPages)) {
        if (page.pDlg == pWidget) {
            pageDescriptionLabel->setText(page.description);
            pageIconLabel->setPixmap(tintedIconPixmap(
                    QIcon(m_iconsPath.filePath(page.iconFile)),
                    pageIconLabel->size(),
                    devicePixelRatioF(),
                    QColor::fromRgb(glass::kAccent)));
            break;
        }
    }

    helpButton->setVisible(pWidget->helpUrl().isValid());
}

// AI-generated (Claude Code). Start of AI-generated text.
void DlgPreferences::setupRail() {
    m_pRailDelegate = new PrefRailDelegate(contentsTreeWidget);
    contentsTreeWidget->setItemDelegate(m_pRailDelegate);
    contentsTreeWidget->setHeaderHidden(true);
    contentsTreeWidget->setMouseTracking(true);
    contentsTreeWidget->viewport()->setAttribute(Qt::WA_Hover);
    contentsTreeWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    contentsTreeWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // These buttons must not take the place of the default button of the dialog.
    railToggleButton->setAutoDefault(false);
    deviceStatusButton->setAutoDefault(false);
    helpButton->setAutoDefault(false);
    deviceStatusButton->setIcon(QIcon(tintedIconPixmap(
            QIcon(QStringLiteral(":/images/preferences/light/") + kControllersIcon),
            deviceStatusButton->iconSize(),
            devicePixelRatioF(),
            QColor::fromRgb(glass::kAccent))));

    m_pRailBridge = new QFrame(this);
    m_pRailBridge->setObjectName(QStringLiteral("railBridge"));
    m_pRailBridge->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_pRailBridge->hide();

    m_pRailAnimation = new QVariantAnimation(this);
    m_pRailAnimation->setDuration(kRailAnimationMs);
    m_pRailAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_pRailAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        railWidget->setFixedWidth(value.toInt());
        updateRailBridge();
    });

    connect(railToggleButton, &QPushButton::clicked, this, [this]() {
        const bool collapsed = !m_railCollapsed;
        m_pConfig->setValue(kRailCollapsedCfgKey, collapsed);
        setRailCollapsed(collapsed, true);
    });
    connect(deviceStatusButton,
            &QPushButton::clicked,
            this,
            &DlgPreferences::slotDeviceStatusClicked);

    // The rows move when the rail scrolls or when items are added or removed.
    connect(contentsTreeWidget->verticalScrollBar(),
            &QScrollBar::valueChanged,
            this,
            &DlgPreferences::updateRailBridge);
    connect(contentsTreeWidget->model(),
            &QAbstractItemModel::rowsInserted,
            this,
            &DlgPreferences::updateRailBridge,
            Qt::QueuedConnection);
    connect(contentsTreeWidget->model(),
            &QAbstractItemModel::rowsRemoved,
            this,
            &DlgPreferences::updateRailBridge,
            Qt::QueuedConnection);

    searchEdit->setPlaceholderText(tr("Find a setting"));
    searchEdit->setClearButtonEnabled(true);
    searchEdit->addAction(QIcon(tintedIconPixmap(
                                  QIcon(QStringLiteral(":/images/ic_magnifier.svg")),
                                  QSize(16, 16),
                                  devicePixelRatioF(),
                                  QColor::fromRgb(glass::kTextDim))),
            QLineEdit::LeadingPosition);
    searchEdit->installEventFilter(this);
    searchEmptyLabel->setText(tr("No setting found."));
    searchEmptyLabel->hide();
    connect(searchEdit, &QLineEdit::textChanged, this, &DlgPreferences::filterRail);

    auto* pFindShortcut = new QShortcut(QKeySequence::Find, this);
    connect(pFindShortcut, &QShortcut::activated, this, [this]() {
        if (m_railCollapsed) {
            m_pConfig->setValue(kRailCollapsedCfgKey, false);
            setRailCollapsed(false, true);
        }
        searchEdit->setFocus();
        searchEdit->selectAll();
    });
}

QString DlgPreferences::descriptionForIcon(const QString& iconFile) const {
    const QHash<QString, QString> descriptions = {
            {QStringLiteral("ic_preferences_soundhardware.svg"),
                    tr("Set the audio device, sample rate and outputs.")},
            {QStringLiteral("ic_preferences_crossfader.svg"),
                    tr("Set the crossfader and EQ behavior.")},
            {QStringLiteral("ic_preferences_effects.svg"),
                    tr("Set the effect units and the effects that show.")},
            {QStringLiteral("ic_preferences_replaygain.svg"),
                    tr("Set the track volume normalization.")},
            {QStringLiteral("ic_preferences_decks.svg"),
                    tr("Set the deck display and playback.")},
            {kControllersIcon, tr("Connect and set your controllers.")},
            {QStringLiteral("ic_preferences_vinyl.svg"), tr("Set the timecode vinyl inputs.")},
            {QStringLiteral("ic_preferences_autodj.svg"),
                    tr("Set the automatic track changes.")},
            {QStringLiteral("ic_preferences_library.svg"),
                    tr("Set the music folders and the track table.")},
            {QStringLiteral("ic_preferences_bpmdetect.svg"),
                    tr("Set how the app finds the beats.")},
            {QStringLiteral("ic_preferences_keydetect.svg"),
                    tr("Set how the app finds the key.")},
            {QStringLiteral("ic_preferences_modplug.svg"),
                    tr("Set the decoder for tracker files.")},
            {QStringLiteral("ic_preferences_interface.svg"),
                    tr("Set the skin, the scale and the screen behavior.")},
            {QStringLiteral("ic_preferences_waveforms.svg"),
                    tr("Set the waveform type and colors.")},
            {QStringLiteral("ic_preferences_colors.svg"),
                    tr("Set the hot cue and track colors.")},
            {QStringLiteral("ic_preferences_recording.svg"),
                    tr("Set the file format and the folder.")},
            {QStringLiteral("ic_preferences_broadcast.svg"), tr("Set the stream servers.")},
            {kContributionsIcon, tr("The Mixxx team made the base of Mix Two. Support their work.")},
    };
    // The pages of the single controllers have their own icons.
    return descriptions.value(iconFile, tr("Set the mapping for this controller."));
}

void DlgPreferences::buildRailGroups() {
    // The page creation order stays as it is, because it sets the apply order.
    // Only the order of the rows changes here.
    QHash<QString, QTreeWidgetItem*> itemsByIcon;
    for (const PreferencesPage& page : std::as_const(m_allPages)) {
        if (!page.pTreeItem->parent()) {
            itemsByIcon.insert(page.iconFile, page.pTreeItem);
        }
    }
    while (contentsTreeWidget->topLevelItemCount() > 0) {
        contentsTreeWidget->takeTopLevelItem(0);
    }

    const QList<QPair<QString, QStringList>> groups = {
            {tr("Audio"),
                    {QStringLiteral("ic_preferences_soundhardware.svg"),
                            QStringLiteral("ic_preferences_crossfader.svg"),
                            QStringLiteral("ic_preferences_effects.svg"),
                            QStringLiteral("ic_preferences_replaygain.svg")}},
            {tr("Decks and hardware"),
                    {QStringLiteral("ic_preferences_decks.svg"),
                            kControllersIcon,
                            QStringLiteral("ic_preferences_vinyl.svg"),
                            QStringLiteral("ic_preferences_autodj.svg")}},
            {tr("Library and analysis"),
                    {QStringLiteral("ic_preferences_library.svg"),
                            QStringLiteral("ic_preferences_bpmdetect.svg"),
                            QStringLiteral("ic_preferences_keydetect.svg"),
                            QStringLiteral("ic_preferences_modplug.svg")}},
            {tr("Display"),
                    {QStringLiteral("ic_preferences_interface.svg"),
                            QStringLiteral("ic_preferences_waveforms.svg"),
                            QStringLiteral("ic_preferences_colors.svg")}},
            {tr("Record and stream"),
                    {QStringLiteral("ic_preferences_recording.svg"),
                            QStringLiteral("ic_preferences_broadcast.svg")}},
            {tr("About"), {kContributionsIcon}},
    };
    for (const auto& group : groups) {
        QTreeWidgetItem* pHeading = nullptr;
        for (const QString& iconFile : group.second) {
            QTreeWidgetItem* pItem = itemsByIcon.take(iconFile);
            if (!pItem) {
                // This page is not in this build.
                continue;
            }
            if (!pHeading) {
                pHeading = new QTreeWidgetItem(contentsTreeWidget, QTreeWidgetItem::Type);
                pHeading->setText(0, group.first);
                pHeading->setData(0, kRailHeadingRole, true);
                pHeading->setFlags(Qt::NoItemFlags);
            }
            contentsTreeWidget->addTopLevelItem(pItem);
        }
    }
    // A page without a group must stay reachable.
    DEBUG_ASSERT(itemsByIcon.isEmpty());
    for (QTreeWidgetItem* pItem : std::as_const(itemsByIcon)) {
        contentsTreeWidget->addTopLevelItem(pItem);
    }
}

void DlgPreferences::updateControllerRowsExpansion() {
    // A computer can show many HID devices as controllers, so the rows of the
    // controllers show only when they are necessary.
    const QTreeWidgetItem* pCurrent = contentsTreeWidget->currentItem();
    bool expand = pCurrent &&
            (pCurrent == m_pControllersRootItem || pCurrent->parent() == m_pControllersRootItem);
    if (!m_searchWords.isEmpty()) {
        for (int i = 0; i < m_pControllersRootItem->childCount(); ++i) {
            expand = expand || !m_pControllersRootItem->child(i)->isHidden();
        }
    }
    m_pControllersRootItem->setExpanded(expand);
}

void DlgPreferences::setRailCollapsed(bool collapsed, bool animate) {
    m_railCollapsed = collapsed;
    m_pRailDelegate->setCollapsed(collapsed);
    if (collapsed) {
        searchEdit->clear();
    }
    searchEdit->setVisible(!collapsed);
    contentsTreeWidget->doItemsLayout();

    railToggleButton->setIcon(QIcon(collapsed
                    ? QStringLiteral(":/preferences/glass/rail_expand.svg")
                    : QStringLiteral(":/preferences/glass/rail_collapse.svg")));
    const QString toggleText = collapsed ? tr("Show the page names") : tr("Hide the page names");
    railToggleButton->setToolTip(toggleText);
    railToggleButton->setAccessibleName(toggleText);
    showDeviceStatusText();

    const int targetWidth = collapsed ? kRailCollapsedWidth : kRailExpandedWidth;
    m_pRailAnimation->stop();
    if (animate) {
        m_pRailAnimation->setStartValue(railWidget->width());
        m_pRailAnimation->setEndValue(targetWidth);
        m_pRailAnimation->start();
    } else {
        railWidget->setFixedWidth(targetWidth);
        updateRailBridge();
    }
}

void DlgPreferences::updateRailBridge() {
    QTreeWidgetItem* pCurrent = contentsTreeWidget->currentItem();
    if (!pCurrent || pCurrent->isHidden()) {
        m_pRailBridge->hide();
        return;
    }
    QWidget* pViewport = contentsTreeWidget->viewport();
    QRect row = contentsTreeWidget->visualItemRect(pCurrent);
    row.setLeft(pViewport->width() - 1);
    row.setRight(pViewport->width() - 1);
    // A row that is partly scrolled away must not paint outside of the rail.
    row = row.adjusted(0, 1, 0, -1).intersected(pViewport->rect());
    if (row.isEmpty()) {
        m_pRailBridge->hide();
        return;
    }
    const QPoint topLeft = pViewport->mapTo(this, row.topLeft());
    // Goes over the border of the pane too, so that no line separates them.
    const int right = paneWidget->geometry().left();
    m_pRailBridge->setGeometry(QRect(topLeft, QPoint(right, topLeft.y() + row.height() - 1)));
    m_pRailBridge->show();
    m_pRailBridge->raise();
}

void DlgPreferences::updateDeviceStatus() {
    for (const QMetaObject::Connection& connection : std::as_const(m_deviceStatusConnections)) {
        disconnect(connection);
    }
    m_deviceStatusConnections.clear();

    const QList<Controller*> controllers = m_pControllerManager->getControllerList(false, true);
    Controller* pShown = nullptr;
    for (Controller* pController : controllers) {
        m_deviceStatusConnections.append(connect(pController,
                &Controller::openChanged,
                this,
                &DlgPreferences::updateDeviceStatus,
                Qt::QueuedConnection));
        if (!pShown && pController->isOpen()) {
            pShown = pController;
        }
    }

    if (pShown) {
        m_deviceStatusName = pShown->getName();
        m_deviceStatusState = tr("Connected");
    } else {
        // The list also has keyboards and mice, so do not name a closed device.
        m_deviceStatusName.clear();
        m_deviceStatusState = tr("No controller enabled");
    }
    showDeviceStatusText();
}

void DlgPreferences::showDeviceStatusText() {
    const QString tooltip = m_deviceStatusName.isEmpty()
            ? m_deviceStatusState
            : m_deviceStatusName + QStringLiteral(": ") + m_deviceStatusState;
    deviceStatusButton->setToolTip(tooltip);
    deviceStatusButton->setAccessibleName(tooltip);
    if (m_railCollapsed) {
        deviceStatusButton->setText(QString());
        return;
    }
    if (m_deviceStatusName.isEmpty()) {
        deviceStatusButton->setText(m_deviceStatusState);
        return;
    }
    // The space for the text: the width of the rail without the icon and the padding.
    constexpr int kTextWidth = kRailExpandedWidth - 64;
    const QString name = deviceStatusButton->fontMetrics().elidedText(
            m_deviceStatusName, Qt::ElideRight, kTextWidth);
    deviceStatusButton->setText(name + QLatin1Char('\n') + m_deviceStatusState);
}

void DlgPreferences::slotDeviceStatusClicked() {
    searchEdit->clear();
    QTreeWidgetItem* pTarget = m_pControllersRootItem;
    for (int i = 0; i < m_pControllersRootItem->childCount(); ++i) {
        QTreeWidgetItem* pChild = m_pControllersRootItem->child(i);
        if (pChild->text(0) == m_deviceStatusName) {
            pTarget = pChild;
            break;
        }
    }
    contentsTreeWidget->setCurrentItem(pTarget);
}

void DlgPreferences::filterRail(const QString& query) {
    m_searchWords = prefSearchQueryWords(query);
    if (m_searchIndexDirty && !m_searchWords.isEmpty()) {
        m_searchIndex.clear();
        for (const PreferencesPage& page : std::as_const(m_allPages)) {
            m_searchIndex.insert(page.pDlg, collectPrefSearchEntries(page.pDlg));
        }
        m_searchIndexDirty = false;
    }

    bool anyMatch = false;
    for (const PreferencesPage& page : std::as_const(m_allPages)) {
        const bool match = m_searchWords.isEmpty() || pageMatchesSearch(page);
        page.pTreeItem->setHidden(!match);
        anyMatch = anyMatch || match;
    }
    // A controller row needs its parent row and each row needs its heading.
    for (int i = 0; i < m_pControllersRootItem->childCount(); ++i) {
        if (!m_pControllersRootItem->child(i)->isHidden()) {
            m_pControllersRootItem->setHidden(false);
        }
    }
    QTreeWidgetItem* pHeading = nullptr;
    bool headingHasRows = false;
    for (int i = 0; i < contentsTreeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem* pItem = contentsTreeWidget->topLevelItem(i);
        if (pItem->data(0, kRailHeadingRole).toBool()) {
            if (pHeading) {
                pHeading->setHidden(!headingHasRows);
            }
            pHeading = pItem;
            headingHasRows = false;
        } else if (!pItem->isHidden()) {
            headingHasRows = true;
        }
    }
    if (pHeading) {
        pHeading->setHidden(!headingHasRows);
    }

    searchEmptyLabel->setVisible(!anyMatch);
    updateControllerRowsExpansion();
    updateRailBridge();
}

bool DlgPreferences::pageMatchesSearch(const PreferencesPage& page) const {
    return prefSearchTextMatches(
                   normalizePrefSearchText(page.pTreeItem->text(0)), m_searchWords) ||
            prefSearchTextMatches(normalizePrefSearchText(page.description), m_searchWords) ||
            firstMatchingPrefSearchEntry(m_searchIndex.value(page.pDlg), m_searchWords);
}

void DlgPreferences::selectFirstSearchMatch() {
    // A page with the query in its title comes before a page that only has it
    // in a setting.
    QTreeWidgetItem* pFirstMatch = nullptr;
    QTreeWidgetItemIterator it(contentsTreeWidget,
            QTreeWidgetItemIterator::Selectable | QTreeWidgetItemIterator::NotHidden);
    for (; *it; ++it) {
        if (prefSearchTextMatches(normalizePrefSearchText((*it)->text(0)), m_searchWords)) {
            pFirstMatch = *it;
            break;
        }
        if (!pFirstMatch) {
            pFirstMatch = *it;
        }
    }
    if (!pFirstMatch) {
        return;
    }
    if (contentsTreeWidget->currentItem() == pFirstMatch) {
        showSearchMatchOnCurrentPage();
    } else {
        contentsTreeWidget->setCurrentItem(pFirstMatch);
    }
}

void DlgPreferences::showSearchMatchOnCurrentPage() {
    DlgPreferencePage* pPage = currentPage();
    if (!pPage || m_searchWords.isEmpty()) {
        return;
    }
    const QList<PrefSearchEntry> entries = m_searchIndex.value(pPage);
    const PrefSearchEntry* pEntry = firstMatchingPrefSearchEntry(entries, m_searchWords);
    if (!pEntry || !pEntry->pWidget) {
        return;
    }
    QPointer<QWidget> pTarget = pEntry->pWidget;
    // Open the tabs or stacked pages that hold the match.
    for (QWidget* pChild = pTarget; pChild && pChild != pPage; pChild = pChild->parentWidget()) {
        auto* pStack = qobject_cast<QStackedWidget*>(pChild->parentWidget());
        if (!pStack) {
            continue;
        }
        if (auto* pTabs = qobject_cast<QTabWidget*>(pStack->parentWidget())) {
            pTabs->setCurrentWidget(pChild);
        } else {
            pStack->setCurrentWidget(pChild);
        }
    }
    // The page must do its layout before it can scroll to the widget.
    QPointer<QWidget> pPagePointer = pPage;
    QTimer::singleShot(0, this, [pPagePointer, pTarget]() {
        if (!pPagePointer || !pTarget) {
            return;
        }
        // Each page is in a viewport in a QScrollArea, see addPageWidget().
        auto* pScrollArea = qobject_cast<QScrollArea*>(
                pPagePointer->parentWidget()->parentWidget());
        if (pScrollArea) {
            pScrollArea->ensureWidgetVisible(pTarget);
        }
    });
}
// End of AI-generated text.

void DlgPreferences::moveEvent(QMoveEvent* e) {
    if (m_geometry.length() == 4) {
#ifdef __WINDOWS__
        Q_UNUSED(e);
        m_geometry[0] = QString::number(frameGeometry().left());
        m_geometry[1] = QString::number(frameGeometry().top());
#else
        // Warning! geometry does NOT include the frame/title.
        int offsetX = geometry().left() - frameGeometry().left();
        int offsetY = geometry().top() - frameGeometry().top();
        m_geometry[0] = QString::number(e->pos().x() - offsetX);
        m_geometry[1] = QString::number(e->pos().y() - offsetY);
#endif
    }
}

// AI-generated (Claude Code). Start of AI-generated text.
void DlgPreferences::captureAppliedState() {
    QList<QWidget*> roots;
    for (const PreferencesPage& page : std::as_const(m_allPages)) {
        roots.append(page.pDlg);
    }
    m_appliedState.capture(roots);
}

bool DlgPreferences::hasUnappliedChanges() const {
    return m_appliedState.differsFromCurrent();
}

void DlgPreferences::reject() {
    // Esc, the close button, Close and Alt+F4 come here.
    if (hasUnappliedChanges()) {
        QMessageBox box(this);
        box.setIcon(QMessageBox::Question);
        box.setWindowTitle(tr("Preferences"));
        box.setText(tr("You changed settings and did not apply them."));
        box.setInformativeText(tr("Do you want to apply the changes?"));
        QPushButton* pApply = box.addButton(tr("Apply and close"), QMessageBox::AcceptRole);
        QPushButton* pDiscard =
                box.addButton(tr("Close without applying"), QMessageBox::DestructiveRole);
        QPushButton* pKeep = box.addButton(tr("Keep editing"), QMessageBox::RejectRole);
        box.setDefaultButton(pApply);
        box.setEscapeButton(pKeep);
        // The style sheet is already set, so the style must read the property again.
        pApply->setProperty("primary", true);
        pApply->style()->unpolish(pApply);
        pApply->style()->polish(pApply);
        box.exec();
        if (box.clickedButton() == pApply) {
            emit applyPreferences();
            if (pendingConfigValidOnAllPages()) {
                accept();
            }
            return;
        }
        if (box.clickedButton() != pDiscard) {
            return;
        }
    }
    closeWithoutPrompt();
}

void DlgPreferences::closeWithoutPrompt() {
    emit cancelPreferences();
    QDialog::reject();
}

QRect DlgPreferences::bodyRect() const {
    return rect().adjusted(kShadowSize, kShadowSize, -kShadowSize, -kShadowSize);
}

void DlgPreferences::paintEvent(QPaintEvent* pEvent) {
    Q_UNUSED(pEvent);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF body = QRectF(bodyRect()).adjusted(0.5, 0.5, -0.5, -0.5);

    // A soft shadow: rings that get lighter to the outside.
    painter.setPen(Qt::NoPen);
    for (int i = kShadowSize; i > 0; --i) {
        const qreal strength = 1.0 - static_cast<qreal>(i) / kShadowSize;
        painter.setBrush(QColor(0, 0, 0, static_cast<int>(10 * strength * strength)));
        painter.drawRoundedRect(body.adjusted(-i, -i + 2, i, i + 2),
                kWindowRadius + i,
                kWindowRadius + i);
    }

    QRadialGradient gradient(body.left() + body.width() * 0.15,
            body.top() + body.height() * 0.1,
            std::max(body.width(), body.height()) * 1.25);
    gradient.setColorAt(0.0, QColor(0x2b, 0x46, 0x50));
    gradient.setColorAt(0.45, QColor(0x18, 0x21, 0x27));
    gradient.setColorAt(1.0, QColor(0x0f, 0x14, 0x17));
    painter.setBrush(gradient);
    painter.setPen(QPen(QColor(255, 255, 255, 40), 1.0));
    painter.drawRoundedRect(body, kWindowRadius, kWindowRadius);
}

Qt::Edges DlgPreferences::resizeEdgesAt(const QPoint& pos) const {
    const QRect body = bodyRect();
    if (!body.adjusted(-kResizeGrip, -kResizeGrip, kResizeGrip, kResizeGrip).contains(pos)) {
        return {};
    }
    Qt::Edges edges;
    if (pos.x() < body.left() + kResizeGrip) {
        edges |= Qt::LeftEdge;
    } else if (pos.x() > body.right() - kResizeGrip) {
        edges |= Qt::RightEdge;
    }
    if (pos.y() < body.top() + kResizeGrip) {
        edges |= Qt::TopEdge;
    } else if (pos.y() > body.bottom() - kResizeGrip) {
        edges |= Qt::BottomEdge;
    }
    return edges;
}

bool DlgPreferences::isMoveArea(const QPoint& pos) const {
    if (!bodyRect().contains(pos)) {
        return false;
    }
    // The page header: the band of the pane above the page.
    const QRect pane = paneWidget->geometry();
    if (pane.contains(pos)) {
        return pos.y() < pagesWidget->mapTo(this, QPoint(0, 0)).y();
    }
    // The free space around the rail and the pane.
    return !railWidget->geometry().contains(pos);
}

void DlgPreferences::mousePressEvent(QMouseEvent* pEvent) {
    QWindow* pWindow = windowHandle();
    if (pEvent->button() == Qt::LeftButton && pWindow) {
        const QPoint pos = pEvent->position().toPoint();
        const Qt::Edges edges = resizeEdgesAt(pos);
        if (edges && pWindow->startSystemResize(edges)) {
            pEvent->accept();
            return;
        }
        if (isMoveArea(pos) && pWindow->startSystemMove()) {
            pEvent->accept();
            return;
        }
    }
    QDialog::mousePressEvent(pEvent);
}

void DlgPreferences::mouseMoveEvent(QMouseEvent* pEvent) {
    const Qt::Edges edges = resizeEdgesAt(pEvent->position().toPoint());
    const bool horizontal = edges & (Qt::LeftEdge | Qt::RightEdge);
    const bool vertical = edges & (Qt::TopEdge | Qt::BottomEdge);
    if (horizontal && vertical) {
        const bool falling = edges == (Qt::LeftEdge | Qt::TopEdge) ||
                edges == (Qt::RightEdge | Qt::BottomEdge);
        setCursor(falling ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor);
    } else if (horizontal) {
        setCursor(Qt::SizeHorCursor);
    } else if (vertical) {
        setCursor(Qt::SizeVerCursor);
    } else {
        unsetCursor();
    }
    QDialog::mouseMoveEvent(pEvent);
}
// End of AI-generated text.

void DlgPreferences::resizeEvent(QResizeEvent* e) {
    if (m_geometry.length() == 4) {
        m_geometry[2] = QString::number(e->size().width());
        m_geometry[3] = QString::number(e->size().height());
    }
    updateRailBridge();
}

QRect DlgPreferences::getDefaultGeometry() {
    adjustSize();
    const auto* const pScreen =
            mixxx::widgethelper::getScreenForWidgetOrApplication(*this);
    VERIFY_OR_DEBUG_ASSERT(pScreen) {
        return QRect();
    }
    QSize optimumSize = pScreen->size();

    if (frameSize() == size()) {
        // This code is reached in Gnome 2.3
        qDebug() << "guess the size of the window decoration";
        optimumSize -= QSize(2, 30);
    } else {
        optimumSize -= (frameSize() - size());
    }

    QSize staticSize = size() - pagesWidget->size();
    optimumSize = optimumSize.boundedTo(staticSize + m_pageSizeHint);

    QRect optimumRect = geometry();
    optimumRect.setSize(optimumSize);

    return optimumRect;
}
