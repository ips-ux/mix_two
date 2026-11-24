#pragma once
// AI-generated (Claude Code). Start of AI-generated text.

#include <QString>

class QAbstractItemModel;

/// Finds the first row of `model` whose text in `column` starts with `text`.
/// The match ignores case. The text is the display text of the cell, so it is
/// the text that the user sees in the sorted column. Returns -1 when no row
/// matches or when the text is empty.
int findRowByPrefix(const QAbstractItemModel* pModel, int column, const QString& text);

/// Collects the keys of an alphabet search. A key within the reset time of the
/// last key extends the text. A later key starts a new text. The class has no
/// timer: the caller passes the time, so a test can drive it with plain numbers.
class PrefixSearchBuffer {
  public:
    // Choice: the time after the last key that ends a search text.
    static constexpr qint64 kResetMillis = 1000;

    /// Adds the typed text at time `nowMillis` and returns the whole search text.
    QString add(const QString& typed, qint64 nowMillis);

    /// True when a key at time `nowMillis` would extend the current text.
    bool isActive(qint64 nowMillis) const;

    void clear();

  private:
    QString m_text;
    qint64 m_lastMillis = 0;
};

/// True when the key event text is part of an alphabet search: one printable
/// character. A space only counts inside a running search, because a space
/// alone has another meaning in the track list.
bool isPrefixSearchText(const QString& typed, bool searchIsActive);

// End of AI-generated text.
