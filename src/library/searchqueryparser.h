#pragma once

#include <QRegularExpression>
#include <QString>
#include <memory>

#include "library/searchquery.h"
#include "util/class.h"

class TrackCollection;
class QueryNode;
class AndNode;

class SearchQueryParser {
  public:
    explicit SearchQueryParser(TrackCollection* pTrackCollection, QStringList searchColumns);

    void setSearchColumns(QStringList searchColumns);

    std::unique_ptr<QueryNode> parseQuery(
            const QString& query,
            const QString& extraFilter) const;

    /// splits the query into a list of terms
    static QStringList splitQueryIntoWords(const QString& query);
    /// checks if the changed search query is less specific then the original term
    static bool queryIsLessSpecific(const QString& original, const QString& changed);

    // AI-generated (Claude Code). Start of AI-generated text.
    /// The columns that the search box offers in its column menu, as the
    /// long name of the prefix that the parser understands ("artist").
    static QStringList searchableColumns();
    /// The prefix text for a column of searchableColumns() ("artist:"),
    /// or an empty string for an unknown column.
    static QString searchPrefixForColumn(const QString& column);
    /// Returns the search text after the user chooses a column: the prefix
    /// goes before the text. A prefix that the text already has (also its
    /// short form, with or without a leading "-") is replaced.
    static QString applySearchPrefix(const QString& text, const QString& prefix);
    /// Every prefix name (long and short form) that the parser understands.
    static QStringList knownSearchPrefixNames();
    // End of AI-generated text.

  private:
    void parseTokens(QStringList tokens,
                     AndNode* pQuery) const;

    std::unique_ptr<AndNode> parseAndNode(const QString& query) const;
    std::unique_ptr<OrNode> parseOrNode(const QString& query) const;

    struct TextArgumentResult {
        QString argument;
        StringMatch mode;
    };

    TextArgumentResult getTextArgument(QString argument,
            QStringList* tokens,
            bool removeLeadingEqualsSign = true) const;

    TrackCollection* m_pTrackCollection;
    QStringList m_queryColumns;
    bool m_searchCrates;
    QStringList m_textFilters;
    QStringList m_numericFilters;
    QStringList m_specialFilters;
    QHash<QString, QStringList> m_fieldToSqlColumns;

    QRegularExpression m_textFilterMatcher;
    QRegularExpression m_crateFilterMatcher;
    QRegularExpression m_numericFilterMatcher;
    QRegularExpression m_specialFilterMatcher;

    DISALLOW_COPY_AND_ASSIGN(SearchQueryParser);
};
