#include "widget/wsoundcolorfxbutton.h"

#include <QMenu>
#include <QMouseEvent>

#include "effects/effectsmanager.h"
#include "effects/presets/effectchainpreset.h"
#include "moc_wsoundcolorfxbutton.cpp"
#include "skin/legacy/skincontext.h"

WSoundColorFxButton::WSoundColorFxButton(
        QWidget* pParent, EffectsManager* pEffectsManager)
        : WPushButton(pParent),
          m_pEffectsManager(pEffectsManager),
          m_index(-1) {
}

void WSoundColorFxButton::setup(const QDomNode& node, const SkinContext& context) {
    WPushButton::setup(node, context);
    bool ok = false;
    const int number = context.selectInt(node, QStringLiteral("SoundColorFx"), &ok);
    if (ok && number >= 1 && number <= EffectsManager::kNumSoundColorFx) {
        m_index = number - 1;
    } else {
        SKIN_WARNING(node, context, QStringLiteral("SoundColorFx must be 1 to 4"));
    }
}

void WSoundColorFxButton::mousePressEvent(QMouseEvent* pEvent) {
    if (pEvent->button() == Qt::RightButton && m_index >= 0 && m_pEffectsManager) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        showPresetMenu(pEvent->globalPosition().toPoint());
#else
        showPresetMenu(pEvent->globalPos());
#endif
        pEvent->accept();
        return;
    }
    // A left click is a press with the same rules as the DDJ-800's buttons;
    // the keyboard's SHIFT works as SHIFT on the deck.
    if (pEvent->button() == Qt::LeftButton && m_index >= 0 && m_pEffectsManager) {
        m_pEffectsManager->pressSoundColorFx(m_index,
                pEvent->modifiers().testFlag(Qt::ShiftModifier));
        pEvent->accept();
        return;
    }
    WPushButton::mousePressEvent(pEvent);
}

void WSoundColorFxButton::mouseReleaseEvent(QMouseEvent* pEvent) {
    if (pEvent->button() == Qt::RightButton ||
            (pEvent->button() == Qt::LeftButton && m_index >= 0 && m_pEffectsManager)) {
        pEvent->accept();
        return;
    }
    WPushButton::mouseReleaseEvent(pEvent);
}

void WSoundColorFxButton::showPresetMenu(const QPoint& globalPos) {
    QMenu menu(this);
    QAction* pTitle = menu.addAction(
            tr("Select the effect for %1")
                    .arg(EffectsManager::soundColorFxName(m_index)));
    pTitle->setEnabled(false);
    menu.addSeparator();

    const QString current = m_pEffectsManager->soundColorFxPreset(m_index);
    const auto presets =
            m_pEffectsManager->getChainPresetManager()->getQuickEffectPresetsSorted();
    for (const auto& pPreset : presets) {
        const QString name = pPreset->name();
        QAction* pAction = menu.addAction(name);
        pAction->setCheckable(true);
        pAction->setChecked(name == current);
        connect(pAction, &QAction::triggered, this, [this, name]() {
            m_pEffectsManager->setSoundColorFxPreset(m_index, name);
        });
    }
    menu.exec(globalPos);
}
