// AI-generated (Claude Code). Start of AI-generated text.
#pragma once

#include <QList>
#include <QPointer>
#include <QVariant>
#include <QWidget>

namespace mixxx::preferences {

/// Returns the value that the user can change in this widget, or an invalid
/// QVariant if the widget is not an input. Item views give their editable and
/// checkable cells, and the row order if the user can drag the rows.
QVariant prefInputValue(const QWidget* pWidget);

/// Keeps the values of all inputs below some root widgets, so that the
/// preferences window can see if the user changed a setting since then.
class PrefSnapshot {
  public:
    void capture(const QList<QWidget*>& roots);
    void clear() {
        m_items.clear();
    }
    bool isEmpty() const {
        return m_items.isEmpty();
    }
    /// Returns true if an input that existed at capture() has a different
    /// value now. Inputs that were added or removed since then are ignored.
    bool differsFromCurrent() const;

  private:
    struct Item {
        QPointer<QWidget> pWidget;
        QVariant value;
    };
    QList<Item> m_items;
};

} // namespace mixxx::preferences
// End of AI-generated text.
