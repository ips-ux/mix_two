// AI-generated (Claude Code). Start of AI-generated text.
// WComboBox keeps the value of its control while the skin adds its items.
// End of AI-generated text.
#include "widget/wcombobox.h"

#include <gtest/gtest.h>

#include <QDomDocument>
#include <memory>

#include "control/controlpushbutton.h"
#include "skin/legacy/skincontext.h"
#include "test/mixxxtest.h"
#include "widget/controlwidgetconnection.h"
#include "widget/wbasewidget.h"

namespace {

class WComboBoxTest : public MixxxTest {
  protected:
    WComboBoxTest()
            : m_control(ConfigKey(QStringLiteral("[Test]"), QStringLiteral("mode"))),
              m_context(config(), QStringLiteral("test")) {
        m_control.setButtonMode(mixxx::control::ButtonMode::Toggle);
        m_control.setStates(10);
    }

    /// Builds the widget in the same order as LegacySkinParser:
    /// connections, then setup(), then Init().
    std::unique_ptr<WComboBox> build() {
        auto pCombo = std::make_unique<WComboBox>(nullptr);
        pCombo->addAndSetDisplayConnection(
                std::make_unique<ControlParameterWidgetConnection>(pCombo.get(),
                        m_control.getKey(),
                        nullptr,
                        ControlParameterWidgetConnection::DIR_FROM_AND_TO_WIDGET,
                        ControlParameterWidgetConnection::EMIT_ON_PRESS),
                WBaseWidget::ConnectionSide::None);
        QDomDocument doc;
        QDomElement node = doc.createElement(QStringLiteral("ComboBox"));
        for (int i = 0; i < 10; ++i) {
            QDomElement state = doc.createElement(QStringLiteral("State"));
            QDomElement number = doc.createElement(QStringLiteral("Number"));
            number.appendChild(doc.createTextNode(QString::number(i)));
            QDomElement text = doc.createElement(QStringLiteral("Text"));
            text.appendChild(doc.createTextNode(QStringLiteral("Mode %1").arg(i)));
            state.appendChild(number);
            state.appendChild(text);
            node.appendChild(state);
        }
        doc.appendChild(node);
        pCombo->setup(node, m_context);
        pCombo->Init();
        return pCombo;
    }

    ControlPushButton m_control;
    SkinContext m_context;
};

TEST_F(WComboBoxTest, SetupKeepsTheControlValue) {
    m_control.set(4);
    auto pCombo = build();
    EXPECT_EQ(4.0, m_control.get());
    EXPECT_EQ(10, pCombo->count());
    EXPECT_EQ(4, pCombo->currentIndex());
}

TEST_F(WComboBoxTest, SetupWithValueZero) {
    m_control.set(0);
    auto pCombo = build();
    EXPECT_EQ(0.0, m_control.get());
    EXPECT_EQ(0, pCombo->currentIndex());
}

TEST_F(WComboBoxTest, SelectionWritesAndControlUpdates) {
    m_control.set(2);
    auto pCombo = build();
    pCombo->setCurrentIndex(7);
    EXPECT_EQ(7.0, m_control.get());
    m_control.set(3);
    EXPECT_EQ(3, pCombo->currentIndex());
}

} // namespace
