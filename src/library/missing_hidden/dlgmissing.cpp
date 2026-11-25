#include "library/missing_hidden/dlgmissing.h"

#include <QItemSelection>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QtConcurrent>

#include "controllers/keyboard/keyboardeventfilter.h"
#include "library/library.h"
#include "library/library_prefs.h"
#include "library/trackcollectionmanager.h"
#include "library/missing_hidden/missingtablemodel.h"
#include "moc_dlgmissing.cpp"
#include "util/assert.h"
#include "widget/wlibrary.h"
#include "widget/wtrackmenu.h"
#include "widget/wtracktableview.h"

DlgMissing::DlgMissing(
        WLibrary* parent,
        UserSettingsPointer pConfig,
        Library* pLibrary,
        KeyboardEventFilter* pKeyboard)
        : QWidget(parent),
          Ui::DlgMissing(),
          m_pTrackTableView(
                  new WTrackTableView(
                          this,
                          pConfig,
                          pLibrary,
                          parent->getTrackTableBackgroundColorOpacity())),
          m_pConfig(pConfig),
          m_pLibrary(pLibrary) {
    setupUi(this);
    m_pTrackTableView->installEventFilter(pKeyboard);

    // Install our own trackTable
    QBoxLayout* box = qobject_cast<QBoxLayout*>(layout());
    VERIFY_OR_DEBUG_ASSERT(box) { //Assumes the form layout is a QVBox/QHBoxLayout!
    } else {
        box->removeWidget(m_pTrackTablePlaceholder);
        m_pTrackTablePlaceholder->hide();
        box->insertWidget(1, m_pTrackTableView);
    }

    m_pMissingTableModel = new MissingTableModel(this, pLibrary->trackCollectionManager());
    m_pTrackTableView->loadTrackModel(m_pMissingTableModel);

    connect(btnPurge, &QPushButton::clicked, m_pTrackTableView, &WTrackTableView::slotPurge);
    connect(btnSelect, &QPushButton::clicked, this, &DlgMissing::selectAll);
    connect(btnRelocate, &QPushButton::clicked, this, &DlgMissing::slotRelocate);
    connect(btnAutoRelocate, &QPushButton::clicked, this, &DlgMissing::slotAutoRelocate);
    connect(&m_searchWatcher,
            &QFutureWatcher<QList<mixxx::library::RelocateCandidate>>::finished,
            this,
            &DlgMissing::slotAutoRelocateSearchFinished);
    connect(m_pTrackTableView->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this,
            &DlgMissing::selectionChanged);
    connect(m_pTrackTableView, &WTrackTableView::trackSelected, this, &DlgMissing::trackSelected);

    connect(pLibrary, &Library::setTrackTableFont, m_pTrackTableView, &WTrackTableView::setTrackTableFont);
    connect(pLibrary, &Library::setTrackTableRowHeight, m_pTrackTableView, &WTrackTableView::setTrackTableRowHeight);
    connect(pLibrary, &Library::setSelectedClick, m_pTrackTableView, &WTrackTableView::setSelectedClick);
}

DlgMissing::~DlgMissing() {
    // Stop a running search before the dialog goes away.
    m_cancelSearch = true;
    m_searchWatcher.waitForFinished();
    // Delete m_pTrackTableView before the table model. This is because the
    // table view saves the header state using the model.
    delete m_pTrackTableView;
    delete m_pMissingTableModel;
}

void DlgMissing::onShow() {
    m_pMissingTableModel->select();
    activateButtons(false);
}

void DlgMissing::onSearch(const QString& text) {
    m_pMissingTableModel->search(text);
}

QString DlgMissing::currentSearch() {
    return m_pMissingTableModel->currentSearch();
}

void DlgMissing::selectAll() {
    m_pTrackTableView->selectAll();
}

void DlgMissing::activateButtons(bool enable) {
    btnPurge->setEnabled(enable);
    btnRelocate->setEnabled(enable);
}

// AI-generated (Claude Code). Start of AI-generated text.
void DlgMissing::slotRelocate() {
    TrackCollection* pCollection = m_pLibrary->trackCollectionManager()->internalCollection();
    if (WTrackMenu::relocateMissingTracks(
                this, pCollection, m_pTrackTableView->getSelectedTrackIds())) {
        m_pMissingTableModel->select();
    }
}

void DlgMissing::slotAutoRelocate() {
    if (m_searchWatcher.isRunning()) {
        return;
    }
    const QStringList folders = mixxx::library::decodeRelocateFolders(
            m_pConfig->getValue(
                    mixxx::library::prefs::kAutoRelocateFoldersConfigKey, QString()));
    if (folders.isEmpty()) {
        QMessageBox::information(this,
                tr("Auto Relocate"),
                tr("There is no search folder. Add a folder in the Library "
                   "preferences."));
        return;
    }
    // The search of the folders can take long: it runs in another thread.
    // The change of the library runs here, in the main thread.
    btnAutoRelocate->setEnabled(false);
    m_cancelSearch = false;
    m_searchWatcher.setFuture(QtConcurrent::run([folders, this]() {
        return mixxx::library::findRelocateCandidates(folders, &m_cancelSearch);
    }));
}

void DlgMissing::slotAutoRelocateSearchFinished() {
    btnAutoRelocate->setEnabled(true);
    if (m_cancelSearch) {
        return;
    }
    QList<TrackId> trackIds;
    const int rows = m_pMissingTableModel->rowCount();
    trackIds.reserve(rows);
    for (int row = 0; row < rows; ++row) {
        const TrackId trackId = m_pMissingTableModel->getTrackId(
                m_pMissingTableModel->index(row, 0));
        if (trackId.isValid()) {
            trackIds.append(trackId);
        }
    }
    TrackCollection* pCollection = m_pLibrary->trackCollectionManager()->internalCollection();
    const mixxx::library::RelocatePlan plan =
            pCollection->planAutoRelocate(trackIds, m_searchWatcher.result());
    if (plan.matches.isEmpty()) {
        QMessageBox::information(this,
                tr("Auto Relocate"),
                tr("%n track(s) not found in the search folders.", nullptr, plan.notFound));
        return;
    }

    // Nothing changes before the user accepts. The user can clear the mark
    // of a match to leave that track as it is.
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Auto Relocate"));
    auto* pLayout = new QVBoxLayout(&dialog);
    pLayout->addWidget(new QLabel(
            tr("%n match(es) found. Clear the mark of a track to keep it as it is.",
                    nullptr,
                    plan.matches.size()),
            &dialog));
    auto* pList = new QListWidget(&dialog);
    const QStringList lines = mixxx::library::relocateMatchLines(plan.matches);
    for (const QString& line : lines) {
        auto* pItem = new QListWidgetItem(line, pList);
        pItem->setFlags(pItem->flags() | Qt::ItemIsUserCheckable);
        pItem->setCheckState(Qt::Checked);
    }
    pLayout->addWidget(pList);
    auto* pButtons = new QDialogButtonBox(&dialog);
    pButtons->addButton(tr("Apply"), QDialogButtonBox::AcceptRole);
    pButtons->addButton(QDialogButtonBox::Cancel);
    connect(pButtons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(pButtons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    pLayout->addWidget(pButtons);
    dialog.resize(900, 400);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QList<mixxx::library::RelocateMatch> accepted;
    for (int i = 0; i < pList->count(); ++i) {
        if (pList->item(i)->checkState() == Qt::Checked) {
            accepted.append(plan.matches[i]);
        }
    }
    const mixxx::library::RelocateOutcome outcome = pCollection->applyRelocateMatches(accepted);
    m_pMissingTableModel->select();
    QMessageBox::information(this,
            tr("Auto Relocate"),
            tr("%n track(s) relocated.", nullptr, outcome.relocated) +
                    QLatin1Char('\n') +
                    tr("%n track(s) not found in the search folders.",
                            nullptr,
                            plan.notFound));
}
// End of AI-generated text.

void DlgMissing::selectionChanged(const QItemSelection &selected,
                                  const QItemSelection &deselected) {
    Q_UNUSED(deselected);
    activateButtons(!selected.indexes().isEmpty());
}

bool DlgMissing::hasFocus() const {
    return m_pTrackTableView->hasFocus();
}

void DlgMissing::saveCurrentViewState() {
    m_pTrackTableView->saveCurrentViewState();
};

bool DlgMissing::restoreCurrentViewState() {
    return m_pTrackTableView->restoreCurrentViewState();
};

void DlgMissing::setFocus() {
    m_pTrackTableView->setFocus();
}
