#include "dialog/dlgabout.h"

#include <QDebug>
#include <QFile>
#include <QLocale>

#include "defs_urls.h"
#include "dialog/mixxxcredits.h"
#include "moc_dlgabout.cpp"
#include "util/color/color.h"
#include "util/desktophelper.h"
#include "util/versionstore.h"

DlgAbout::DlgAbout()
        : QDialog(nullptr),
          Ui::DlgAboutDlg() {
    setupUi(this);
    setWindowIcon(QIcon(MIXXX_ICON_PATH));

    mixxx_icon->load(QString(MIXXX_ICON_PATH));
    // AI-generated (Claude Code). Start of AI-generated text.
    // The Mix Two word is dark. On a dark window it uses the light version.
    mixxx_logo->load(Color::isDimColor(palette().window().color())
                    ? QStringLiteral(":/images/mixtwo_logo_light.svg")
                    : QString(MIXXX_LOGO_PATH));
    // End of AI-generated text.

    version_label->setText(VersionStore::applicationName() +
            QStringLiteral(" ") + VersionStore::version());
    git_version_label->setText(VersionStore::gitVersion());
    qt_version_label->setText(VersionStore::qtVersion());
    platform_label->setText(VersionStore::platform());
    QLocale locale;
    date_label->setText(locale.toString(VersionStore::date().toLocalTime(), QLocale::LongFormat));

    QFile licenseFile(":/LICENSE");
    if (!licenseFile.open(QIODevice::ReadOnly)) {
        qWarning() << "LICENSE file not found";
    } else {
        licenseText->setPlainText(licenseFile.readAll());
    }

    textBrowser->setHtml(mixxx::mixxxCreditsHtml());

    textWebsiteLink->setText(
            QString("<a style=\"color:%1;\" href=\"%2\">%3</a>")
                    .arg(Color::blendColors(palette().link().color(),
                                 palette().text().color())
                                    .name(),
                            MIXXX_WEBSITE_URL,
                            tr("Official Website")));
    if (std::rand() % 6) {
        if (!Color::isDimColor(palette().text().color())) {
            btnDonate->setIcon(QIcon(":/images/heart_icon_light.svg"));
        } else {
            btnDonate->setIcon(QIcon(":/images/heart_icon_dark.svg"));
        }
    } else {
        btnDonate->setIcon(QIcon(":/images/heart_icon_rainbow.svg"));
    }
    btnDonate->setText(tr("Donate"));
    connect(btnDonate, &QPushButton::clicked, this, [] {
        mixxx::DesktopHelper::openUrl(QUrl(MIXXX_DONATE_URL));
    });

    connect(buttonBox, &QDialogButtonBox::accepted, this, &DlgAbout::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &DlgAbout::reject);
}
