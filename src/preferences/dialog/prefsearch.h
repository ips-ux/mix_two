// AI-generated (Claude Code). Start of AI-generated text.
#pragma once

#include <QList>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QWidget>

namespace mixxx::preferences {

/// One piece of text that a preferences page shows, with the widget that shows it.
struct PrefSearchEntry {
    QString text;
    QPointer<QWidget> pWidget;
};

/// Removes rich text tags and mnemonic markers, and folds the case.
QString normalizePrefSearchText(const QString& text);

/// Finds the text of labels, buttons, group boxes, tabs and combo box items in
/// pPage. Widgets that the page hid explicitly are not included.
QList<PrefSearchEntry> collectPrefSearchEntries(QWidget* pPage);

/// Returns true if each word of the query is in the normalized text.
bool prefSearchTextMatches(const QString& normalizedText, const QStringList& queryWords);

/// Splits a query into normalized words.
QStringList prefSearchQueryWords(const QString& query);

/// Returns the first entry that matches, or nullptr if no entry matches.
const PrefSearchEntry* firstMatchingPrefSearchEntry(
        const QList<PrefSearchEntry>& entries,
        const QStringList& queryWords);

} // namespace mixxx::preferences
// End of AI-generated text.
