// AI-generated (Claude Code). Start of AI-generated text.
#include "preferences/dialog/prefsearch.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTextDocument>
#include <QTextDocumentFragment>

namespace mixxx::preferences {

namespace {

void addEntry(QList<PrefSearchEntry>* pEntries, const QString& text, QWidget* pWidget) {
    const QString normalized = normalizePrefSearchText(text);
    if (!normalized.isEmpty()) {
        pEntries->append(PrefSearchEntry{normalized, pWidget});
    }
}

/// Returns true if the widget or one of its parents below pPage is hidden.
/// The pages of a stacked widget (and so of a tab widget) stay searchable,
/// because the user can open them.
bool isHiddenByPage(const QWidget* pWidget, const QWidget* pPage) {
    for (; pWidget && pWidget != pPage; pWidget = pWidget->parentWidget()) {
        if (pWidget->isHidden() &&
                !qobject_cast<const QStackedWidget*>(pWidget->parentWidget())) {
            return true;
        }
    }
    return false;
}

} // namespace

QString normalizePrefSearchText(const QString& text) {
    QString plain = Qt::mightBeRichText(text)
            ? QTextDocumentFragment::fromHtml(text).toPlainText()
            : text;
    // "&&" is a literal ampersand, a single "&" marks a mnemonic.
    plain.replace(QStringLiteral("&&"), QStringLiteral("\x01"));
    plain.remove(QLatin1Char('&'));
    plain.replace(QChar(0x01), QLatin1Char('&'));
    return plain.simplified().toCaseFolded();
}

QList<PrefSearchEntry> collectPrefSearchEntries(QWidget* pPage) {
    QList<PrefSearchEntry> entries;
    if (!pPage) {
        return entries;
    }
    const QList<QWidget*> widgets = pPage->findChildren<QWidget*>();
    for (QWidget* pWidget : widgets) {
        if (auto* pTabWidget = qobject_cast<QTabWidget*>(pWidget)) {
            for (int i = 0; i < pTabWidget->count(); ++i) {
                addEntry(&entries, pTabWidget->tabText(i), pTabWidget->widget(i));
            }
        }
        if (isHiddenByPage(pWidget, pPage)) {
            continue;
        }
        if (auto* pLabel = qobject_cast<QLabel*>(pWidget)) {
            addEntry(&entries, pLabel->text(), pLabel);
        } else if (auto* pButton = qobject_cast<QAbstractButton*>(pWidget)) {
            addEntry(&entries, pButton->text(), pButton);
        } else if (auto* pGroupBox = qobject_cast<QGroupBox*>(pWidget)) {
            addEntry(&entries, pGroupBox->title(), pGroupBox);
        } else if (auto* pComboBox = qobject_cast<QComboBox*>(pWidget)) {
            for (int i = 0; i < pComboBox->count(); ++i) {
                addEntry(&entries, pComboBox->itemText(i), pComboBox);
            }
        }
    }
    return entries;
}

bool prefSearchTextMatches(const QString& normalizedText, const QStringList& queryWords) {
    if (queryWords.isEmpty()) {
        return false;
    }
    for (const QString& word : queryWords) {
        if (!normalizedText.contains(word)) {
            return false;
        }
    }
    return true;
}

QStringList prefSearchQueryWords(const QString& query) {
    return normalizePrefSearchText(query).split(QLatin1Char(' '), Qt::SkipEmptyParts);
}

const PrefSearchEntry* firstMatchingPrefSearchEntry(
        const QList<PrefSearchEntry>& entries,
        const QStringList& queryWords) {
    for (const PrefSearchEntry& entry : entries) {
        if (prefSearchTextMatches(entry.text, queryWords)) {
            return &entry;
        }
    }
    return nullptr;
}

} // namespace mixxx::preferences
// End of AI-generated text.
