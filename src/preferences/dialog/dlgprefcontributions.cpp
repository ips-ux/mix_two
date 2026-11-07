// AI-generated (Claude Code). Start of AI-generated text.
#include "preferences/dialog/dlgprefcontributions.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

#include "defs_urls.h"
#include "dialog/mixxxcredits.h"
#include "moc_dlgprefcontributions.cpp"
#include "util/desktophelper.h"

namespace {

const QString kMixxxGitHubUrl = QStringLiteral("https://github.com/mixxxdj/mixxx");

QLabel* makeText(const QString& text, QWidget* pParent) {
    auto* pLabel = new QLabel(text, pParent);
    pLabel->setWordWrap(true);
    pLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    pLabel->setOpenExternalLinks(true);
    return pLabel;
}

QPushButton* makeLinkButton(const QString& text, const QString& url, QWidget* pParent) {
    auto* pButton = new QPushButton(text, pParent);
    pButton->setAutoDefault(false);
    pButton->setToolTip(url);
    QObject::connect(pButton, &QPushButton::clicked, pButton, [url] {
        mixxx::DesktopHelper::openUrl(QUrl(url));
    });
    return pButton;
}

} // namespace

DlgPrefContributions::DlgPrefContributions(QWidget* pParent)
        : DlgPreferencePage(pParent) {
    auto* pLayout = new QVBoxLayout(this);
    pLayout->setContentsMargins(0, 0, 0, 0);
    pLayout->setSpacing(12);

    // Who made this app.
    auto* pOrigin = new QGroupBox(tr("Based on Mixxx"), this);
    auto* pOriginLayout = new QVBoxLayout(pOrigin);
    pOriginLayout->addWidget(makeText(
            tr("Mix Two is a fork of Mixxx, the free and open-source DJ software."),
            pOrigin));
    pOriginLayout->addWidget(makeText(
            tr("The Mixxx team and its contributors made most of this app: the "
               "audio engine, the effects, the library, the analysis, the "
               "controller system and the skins. Mix Two adds the DDJ-800 "
               "work and the rekordbox-style changes to their work."),
            pOrigin));
    pOriginLayout->addWidget(makeText(
            tr("Without the many years of work of these volunteers, Mix Two "
               "cannot exist. Thank you to each person in the list below."),
            pOrigin));
    pOriginLayout->addWidget(makeText(
            tr("Mixxx is free software under the GNU General Public License, "
               "version 2 or later. Mix Two uses the same license."),
            pOrigin));
    pOriginLayout->addWidget(makeText(
            tr("The Mixxx team does not make Mix Two. Report Mix Two problems "
               "to the Mix Two project, not to the Mixxx team."),
            pOrigin));
    pLayout->addWidget(pOrigin);

    // How to support them.
    auto* pSupport = new QGroupBox(tr("Support the Mixxx team"), this);
    auto* pSupportLayout = new QVBoxLayout(pSupport);
    pSupportLayout->addWidget(makeText(
            tr("The Mixxx team works without pay. A donation helps them continue "
               "their work. If Mix Two is useful to you, think about a donation "
               "to Mixxx."),
            pSupport));
    auto* pButtons = new QHBoxLayout();
    auto* pDonate = makeLinkButton(tr("Donate to Mixxx"), MIXXX_DONATE_URL, pSupport);
    pDonate->setIcon(QIcon(QStringLiteral(":/images/heart_icon_rainbow.svg")));
    pDonate->setProperty("primary", true);
    pButtons->addWidget(pDonate);
    pButtons->addWidget(makeLinkButton(tr("Mixxx website"), MIXXX_WEBSITE_URL, pSupport));
    pButtons->addWidget(makeLinkButton(tr("Mixxx on GitHub"), kMixxxGitHubUrl, pSupport));
    pButtons->addStretch();
    pSupportLayout->addLayout(pButtons);
    pLayout->addWidget(pSupport);

    // The full credits, the same as in the About dialog.
    auto* pCredits = new QGroupBox(tr("The Mixxx team and contributors"), this);
    auto* pCreditsLayout = new QVBoxLayout(pCredits);
    auto* pBrowser = new QTextBrowser(pCredits);
    pBrowser->setHtml(mixxx::mixxxCreditsHtml());
    pBrowser->setMinimumHeight(420);
    pCreditsLayout->addWidget(pBrowser);
    pLayout->addWidget(pCredits, 1);
}
// End of AI-generated text.
