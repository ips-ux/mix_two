// AI-generated (Claude Code). Start of AI-generated text.
#include "preferences/dialog/prefsnapshot.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractSlider>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QStringList>
#include <QTextEdit>

namespace mixxx::preferences {

namespace {

// Large models, for example the input mapping table of a controller, are
// compared up to this row.
constexpr int kMaxRows = 4000;

bool isInsideComboOrSpinBox(const QWidget* pWidget) {
    for (const QObject* pParent = pWidget->parent(); pParent; pParent = pParent->parent()) {
        if (qobject_cast<const QComboBox*>(pParent) ||
                qobject_cast<const QAbstractSpinBox*>(pParent)) {
            return true;
        }
    }
    return false;
}

QVariant itemViewValue(const QAbstractItemView* pView) {
    const QAbstractItemModel* pModel = pView->model();
    if (!pModel) {
        return QVariant();
    }
    const bool dragOrder = pView->dragDropMode() != QAbstractItemView::NoDragDrop;
    QStringList values;
    const int rows = std::min(pModel->rowCount(), kMaxRows);
    const int columns = pModel->columnCount();
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            const QModelIndex index = pModel->index(row, column);
            const Qt::ItemFlags flags = pModel->flags(index);
            if (flags.testFlag(Qt::ItemIsEditable)) {
                values.append(pModel->data(index, Qt::EditRole).toString());
            }
            if (flags.testFlag(Qt::ItemIsUserCheckable)) {
                values.append(pModel->data(index, Qt::CheckStateRole).toString());
            }
            if (dragOrder && column == 0) {
                values.append(pModel->data(index, Qt::DisplayRole).toString());
            }
        }
    }
    if (values.isEmpty()) {
        return QVariant();
    }
    return values.join(QChar(0x1f));
}

} // namespace

QVariant prefInputValue(const QWidget* pWidget) {
    if (!pWidget || isInsideComboOrSpinBox(pWidget)) {
        return QVariant();
    }
    if (const auto* pButton = qobject_cast<const QAbstractButton*>(pWidget)) {
        return pButton->isCheckable() ? QVariant(pButton->isChecked()) : QVariant();
    }
    if (const auto* pGroupBox = qobject_cast<const QGroupBox*>(pWidget)) {
        return pGroupBox->isCheckable() ? QVariant(pGroupBox->isChecked()) : QVariant();
    }
    if (const auto* pComboBox = qobject_cast<const QComboBox*>(pWidget)) {
        return pComboBox->currentText();
    }
    if (qobject_cast<const QScrollBar*>(pWidget)) {
        // A scroll position is not a setting.
        return QVariant();
    }
    if (const auto* pSlider = qobject_cast<const QAbstractSlider*>(pWidget)) {
        return pSlider->value();
    }
    if (const auto* pSpinBox = qobject_cast<const QSpinBox*>(pWidget)) {
        return pSpinBox->value();
    }
    if (const auto* pDoubleSpinBox = qobject_cast<const QDoubleSpinBox*>(pWidget)) {
        return pDoubleSpinBox->value();
    }
    if (const auto* pDateTimeEdit = qobject_cast<const QDateTimeEdit*>(pWidget)) {
        return pDateTimeEdit->dateTime();
    }
    if (const auto* pLineEdit = qobject_cast<const QLineEdit*>(pWidget)) {
        return pLineEdit->isReadOnly() ? QVariant() : QVariant(pLineEdit->text());
    }
    if (const auto* pPlainTextEdit = qobject_cast<const QPlainTextEdit*>(pWidget)) {
        return pPlainTextEdit->isReadOnly() ? QVariant()
                                            : QVariant(pPlainTextEdit->toPlainText());
    }
    if (const auto* pTextEdit = qobject_cast<const QTextEdit*>(pWidget)) {
        return pTextEdit->isReadOnly() ? QVariant() : QVariant(pTextEdit->toPlainText());
    }
    if (const auto* pView = qobject_cast<const QAbstractItemView*>(pWidget)) {
        return itemViewValue(pView);
    }
    return QVariant();
}

void PrefSnapshot::capture(const QList<QWidget*>& roots) {
    m_items.clear();
    for (QWidget* pRoot : roots) {
        if (!pRoot) {
            continue;
        }
        const QList<QWidget*> widgets = pRoot->findChildren<QWidget*>();
        for (QWidget* pWidget : widgets) {
            QVariant value = prefInputValue(pWidget);
            if (value.isValid()) {
                m_items.append(Item{pWidget, std::move(value)});
            }
        }
    }
}

bool PrefSnapshot::differsFromCurrent() const {
    for (const Item& item : m_items) {
        if (item.pWidget && prefInputValue(item.pWidget) != item.value) {
            return true;
        }
    }
    return false;
}

} // namespace mixxx::preferences
// End of AI-generated text.
