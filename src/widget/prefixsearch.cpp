// AI-generated (Claude Code). Start of AI-generated text.
#include "widget/prefixsearch.h"

#include <QAbstractItemModel>

int findRowByPrefix(const QAbstractItemModel* pModel, int column, const QString& text) {
    if (!pModel || text.isEmpty() || column < 0 || column >= pModel->columnCount()) {
        return -1;
    }
    const int rowCount = pModel->rowCount();
    for (int row = 0; row < rowCount; ++row) {
        const QString cell =
                pModel->index(row, column).data(Qt::DisplayRole).toString();
        if (cell.startsWith(text, Qt::CaseInsensitive)) {
            return row;
        }
    }
    return -1;
}

bool PrefixSearchBuffer::isActive(qint64 nowMillis) const {
    return !m_text.isEmpty() && nowMillis - m_lastMillis <= kResetMillis;
}

QString PrefixSearchBuffer::add(const QString& typed, qint64 nowMillis) {
    if (!isActive(nowMillis)) {
        m_text.clear();
    }
    m_text += typed;
    m_lastMillis = nowMillis;
    return m_text;
}

void PrefixSearchBuffer::clear() {
    m_text.clear();
}

bool isPrefixSearchText(const QString& typed, bool searchIsActive) {
    if (typed.size() != 1) {
        return false;
    }
    const QChar c = typed.at(0);
    if (c == QLatin1Char(' ')) {
        return searchIsActive;
    }
    return c.isPrint();
}

// End of AI-generated text.
