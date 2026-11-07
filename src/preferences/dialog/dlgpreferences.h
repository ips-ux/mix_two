#pragma once

#include <QDialog>
#include <QDir>
#include <QEvent>
#include <QHash>
#include <QList>
#include <QMetaObject>
#include <QRect>
#include <QStringList>
#include <memory>

#include "control/controlpushbutton.h"
#include "preferences/constants.h"
#include "preferences/dialog/dlgpreferencepage.h"
#include "preferences/dialog/prefsearch.h"
#include "preferences/dialog/ui_dlgpreferencesdlg.h"
#include "preferences/settingsmanager.h"
#include "preferences/usersettings.h"

class ControllerManager;
class DlgPrefControllers;
class DlgPrefSound;
class EffectsManager;
class Library;
class QFrame;
class QVariantAnimation;
class SoundManager;
class VinylControlManager;

namespace mixxx {
class ScreensaverManager;
namespace preferences {
class PrefRailDelegate;
} // namespace preferences
namespace skin {
class SkinLoader;
} // namespace skin
} // namespace mixxx

class DlgPreferences : public QDialog, public Ui::DlgPreferencesDlg {
    Q_OBJECT
  public:
    struct PreferencesPage {
        PreferencesPage() {
        }
        PreferencesPage(DlgPreferencePage* pDlg, QTreeWidgetItem* pTreeItem)
                : pDlg(pDlg),
                  pTreeItem(pTreeItem) {
        }

        DlgPreferencePage* pDlg;
        QTreeWidgetItem* pTreeItem;
        QString iconFile;
        QString description;
    };

    DlgPreferences(
            std::shared_ptr<mixxx::ScreensaverManager> pScreensaverManager,
            std::shared_ptr<mixxx::skin::SkinLoader> pSkinLoader,
            std::shared_ptr<SoundManager> pSoundManager,
            std::shared_ptr<ControllerManager> pControllerManager,
            std::shared_ptr<VinylControlManager> pVCManager,
            std::shared_ptr<EffectsManager> pEffectsManager,
            std::shared_ptr<SettingsManager> pSettingsManager,
            std::shared_ptr<Library> pLibrary,
            bool includeWaveformPreferences = true);
    virtual ~DlgPreferences();

    void addPageWidget(const PreferencesPage& page,
            const QString& pageTitle,
            const QString& iconFile);
    void removePageWidget(DlgPreferencePage* pWidget);
    void expandTreeItem(QTreeWidgetItem* pItem);
    void switchToPage(const QString& pageTitle, DlgPreferencePage* pPage);

  public slots:
    void changePage(QTreeWidgetItem* pCurrent, QTreeWidgetItem* pPrevious);
    void showSoundHardwarePage(
            std::optional<mixxx::preferences::SoundHardwareTab> tab =
                    std::nullopt);
    void showSoundHardwareInputPage();
    void slotButtonPressed(QAbstractButton* pButton);
    void slotHelpClicked();
  signals:
    void closeDlg();
    void showDlg();

    // Emitted just after the user clicks Apply or OK.
    void applyPreferences();
    // Emitted if the user clicks Cancel
    void cancelPreferences();
    // Emitted if the user clicks Reset to Defaults.
    void resetToDefaults();

    void reloadUserInterface();
    void tooltipModeChanged(mixxx::preferences::Tooltips tooltipMode);
    void menuBarAutoHideChanged();

  protected:
    bool eventFilter(QObject*, QEvent*) override;
    void moveEvent(QMoveEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

  private:
    DlgPreferencePage* currentPage();
    // AI-generated (Claude Code). Start of AI-generated text.
    void setupRail();
    void buildRailGroups();
    void updateControllerRowsExpansion();
    QString descriptionForIcon(const QString& iconFile) const;
    void setRailCollapsed(bool collapsed, bool animate);
    /// Joins the selected rail row and the pane. Call when the row or the rail moves.
    void updateRailBridge();
    void updateDeviceStatus();
    void showDeviceStatusText();
    void slotDeviceStatusClicked();
    /// Shows only the rows of the pages that match the query.
    void filterRail(const QString& query);
    bool pageMatchesSearch(const PreferencesPage& page) const;
    void selectFirstSearchMatch();
    /// Opens the tab of the first match on the current page and scrolls to it.
    void showSearchMatchOnCurrentPage();
    // End of AI-generated text.
    QList<PreferencesPage> m_allPages;
    void onShow();
    void onHide();
    QRect getDefaultGeometry();

    QAbstractButton* m_pApplyButton;
    QAbstractButton* m_pAcceptButton;

    QStringList m_geometry;
    UserSettingsPointer m_pConfig;
    std::shared_ptr<ControllerManager> m_pControllerManager;
    std::unique_ptr<DlgPrefSound> m_pSoundDlg;
    PreferencesPage m_soundPage;
    DlgPrefControllers* m_pControllersDlg;
    QTreeWidgetItem* m_pControllersRootItem{nullptr};

    mixxx::preferences::PrefRailDelegate* m_pRailDelegate{nullptr};
    QFrame* m_pRailBridge{nullptr};
    QVariantAnimation* m_pRailAnimation{nullptr};
    bool m_railCollapsed{false};

    QString m_deviceStatusName;
    QString m_deviceStatusState;
    QList<QMetaObject::Connection> m_deviceStatusConnections;

    QHash<DlgPreferencePage*, QList<mixxx::preferences::PrefSearchEntry>> m_searchIndex;
    QStringList m_searchWords;
    bool m_searchIndexDirty{true};

    QSize m_pageSizeHint;

    QDir m_iconsPath;
    bool pendingConfigValidOnAllPages();
};
