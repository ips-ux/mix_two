#include "widget/wcombobox.h"

#include <QDomNode>
#include <QEvent>
#include <QIcon>
#include <QSignalBlocker>

#include "moc_wcombobox.cpp"
#include "skin/legacy/skincontext.h"

WComboBox::WComboBox(QWidget* pParent)
        : QComboBox(pParent),
          WBaseWidget(this) {
    connect(this,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &WComboBox::slotCurrentIndexChanged);
}

void WComboBox::setup(const QDomNode& node, const SkinContext& context) {
    // AI-generated (Claude Code). Start of AI-generated text.
    // The connections exist before setup() runs. Block the signals while the
    // items are added: the first addItem() changes the index from -1 to 0, and
    // slotCurrentIndexChanged() would write 0 to the control (a persisted
    // value would be lost). Init() then sets the index from the control.
    // End of AI-generated text.
    const QSignalBlocker blocker(this);
    // Load pixmaps for associated states
    QDomNode state = context.selectNode(node, "State");
    while (!state.isNull()) {
        if (state.isElement() && state.nodeName() == "State") {
            int iState = 0;
            if (context.hasNodeSelectInt(state, "Number", &iState)) {
                QString text = context.selectString(state, "Text");
                QString icon = context.selectString(state, "Icon");
                addItem(QIcon(icon), text, QVariant(iState));
            } else {
                SKIN_WARNING(state,
                        context,
                        QStringLiteral("WComboBox ignoring <State> without "
                                       "<Number> node."));
            }
        }
        state = state.nextSibling();
    }
}

bool WComboBox::event(QEvent* pEvent) {
    if (pEvent->type() == QEvent::ToolTip) {
        updateTooltip();
    }
    return QComboBox::event(pEvent);
}

void WComboBox::onConnectedControlChanged(double dParameter, double dValue) {
    Q_UNUSED(dParameter);
    // Enums are not currently represented using parameter space so it doesn't
    // make sense to use the parameter here yet.
    int index = findData(static_cast<int>(dValue));
    if (index != -1) {
        setCurrentIndex(index);
    }
}

void WComboBox::slotCurrentIndexChanged(int index) {
    setControlParameter(index);
}
