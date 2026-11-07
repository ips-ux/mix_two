// AI-generated (Claude Code). Start of AI-generated text.
#pragma once

#include <QIcon>
#include <QPixmap>
#include <QStyledItemDelegate>

namespace mixxx::preferences {

/// The colors of the preferences window that C++ code paints.
/// Keep these equal to the values in res/preferences/glass.qss.
namespace glass {
inline constexpr QRgb kPane = 0xff222c33;
inline constexpr QRgb kAccent = 0xff4fd8c8;
inline constexpr QRgb kText = 0xffe6eef0;
inline constexpr QRgb kTextDim = 0xff9db0b6;
inline constexpr QRgb kLine = 0xff3a4750;
} // namespace glass

/// Item data role that marks a rail row as a group heading.
inline constexpr int kRailHeadingRole = Qt::UserRole + 1;

/// Paints a QIcon in one color, as the rail and the page header use it.
QPixmap tintedIconPixmap(const QIcon& icon, QSize size, qreal devicePixelRatio, QColor color);

/// Paints the rows of the preferences rail: group headings, pages and
/// controllers, with or without labels.
class PrefRailDelegate : public QStyledItemDelegate {
    Q_OBJECT
  public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void setCollapsed(bool collapsed) {
        m_collapsed = collapsed;
    }

    void paint(QPainter* pPainter,
            const QStyleOptionViewItem& option,
            const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
            const QModelIndex& index) const override;

  private:
    bool m_collapsed{false};
};

} // namespace mixxx::preferences
// End of AI-generated text.
