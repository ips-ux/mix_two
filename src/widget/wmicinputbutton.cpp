// AI-generated (Claude Code)
#include "widget/wmicinputbutton.h"

#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QStyle>

#include "control/controlproxy.h"
#include "mixer/playermanager.h"
#include "moc_wmicinputbutton.cpp"
#include "skin/legacy/skincontext.h"
#include "soundio/audiotargetmanager.h"

namespace {
const QString kAppGroup = QStringLiteral("[App]");
const QString kSkinGroup = QStringLiteral("[Skin]");
constexpr int kNumMics = 4;
} // namespace

WMicInputButton::WMicInputButton(QWidget* pParent, AudioTargetManager* pTargets)
        : WPushButton(pParent),
          m_pTargets(pTargets),
          m_index(-1),
          m_assigned(false),
          m_selected(false) {
}

WMicInputButton::~WMicInputButton() = default;

void WMicInputButton::setup(const QDomNode& node, const SkinContext& context) {
    WPushButton::setup(node, context);
    bool ok = false;
    const int number = context.selectInt(node, QStringLiteral("MicInput"), &ok);
    if (!ok || number < 1 || number > kNumMics) {
        SKIN_WARNING(node, context, QStringLiteral("MicInput must be 1 to 4"));
        return;
    }
    m_index = number - 1;

    setBaseTooltip(tr("MIC %1. Click to set the microphone on or off. "
                      "Right-click to select the input device.")
                           .arg(number));

    const QString micGroup = PlayerManager::groupForMicrophone(m_index);
    m_pInputConfigured = std::make_unique<ControlProxy>(
            micGroup, QStringLiteral("input_configured"), this);
    m_pTalkover = std::make_unique<ControlProxy>(
            micGroup, QStringLiteral("talkover"), this);
    // The skin creates [Skin],mic_selected. Without it the square still works.
    m_pSelectedMic = std::make_unique<ControlProxy>(
            kSkinGroup,
            QStringLiteral("mic_selected"),
            this,
            ControlFlag::AllowMissingOrInvalid);

    m_pInputConfigured->connectValueChanged(this, &WMicInputButton::slotStateChanged);
    m_pSelectedMic->connectValueChanged(this, &WMicInputButton::slotStateChanged);
    slotStateChanged();
}

void WMicInputButton::slotStateChanged() {
    const bool assigned = m_pInputConfigured && m_pInputConfigured->toBool();
    const bool selected = m_pSelectedMic && m_pSelectedMic->valid() &&
            static_cast<int>(m_pSelectedMic->get()) == m_index + 1;
    const bool assignedDiffers = assigned != m_assigned;
    const bool selectedDiffers = selected != m_selected;
    m_assigned = assigned;
    m_selected = selected;
    if (assignedDiffers) {
        emit assignedChanged(m_assigned);
    }
    if (selectedDiffers) {
        emit selectedChanged(m_selected);
    }
    if (assignedDiffers || selectedDiffers) {
        style()->unpolish(this);
        style()->polish(this);
        update();
    }
}

void WMicInputButton::mousePressEvent(QMouseEvent* pEvent) {
    if (m_index < 0) {
        WPushButton::mousePressEvent(pEvent);
        return;
    }
    const Qt::MouseButton button = pEvent->button();
    if (button != Qt::LeftButton && button != Qt::RightButton) {
        WPushButton::mousePressEvent(pEvent);
        return;
    }

    if (m_pSelectedMic->valid()) {
        m_pSelectedMic->set(m_index + 1);
        // The proxy drops its own change signal, so update this square here.
        slotStateChanged();
    }

    const bool hasInput = m_pInputConfigured->toBool();
    if (button == Qt::LeftButton && m_pTalkover->toBool()) {
        // A lit square always goes off, also when its input is gone.
        m_pTalkover->set(0.0);
    } else if (button == Qt::LeftButton && hasInput) {
        m_pTalkover->set(1.0);
    } else if (m_pTargets) {
        showInputMenu(pEvent->globalPosition().toPoint());
    }
    pEvent->accept();
}

void WMicInputButton::mouseReleaseEvent(QMouseEvent* pEvent) {
    if (m_index >= 0 &&
            (pEvent->button() == Qt::LeftButton ||
                    pEvent->button() == Qt::RightButton)) {
        pEvent->accept();
        return;
    }
    WPushButton::mouseReleaseEvent(pEvent);
}

bool WMicInputButton::deckIsPlaying() const {
    const ControlProxy numDecks(kAppGroup, QStringLiteral("num_decks"));
    const int decks = static_cast<int>(numDecks.get());
    for (int i = 1; i <= decks; ++i) {
        const ControlProxy play(PlayerManager::groupForDeck(i - 1), QStringLiteral("play"));
        if (play.toBool()) {
            return true;
        }
    }
    return false;
}

void WMicInputButton::showInputMenu(const QPoint& globalPos) {
    if (!m_pTargets || m_index < 0) {
        return;
    }

    const QString current = m_pTargets->micInputDeviceName(m_index);
    const auto devices = m_pTargets->micInputDevices();

    QMenu menu(this);
    QAction* pTitle = menu.addAction(
            tr("Select the input device for MIC %1").arg(m_index + 1));
    pTitle->setEnabled(false);
    menu.addSeparator();

    // The action that the user picks sets these.
    bool picked = false;
    SoundDeviceId chosen;

    QAction* pNone = menu.addAction(tr("None"));
    pNone->setCheckable(true);
    pNone->setChecked(current.isEmpty());
    connect(pNone, &QAction::triggered, this, [&picked]() { picked = true; });

    bool currentListed = current.isEmpty();
    for (const auto& pDevice : devices) {
        const SoundDeviceId id = pDevice->getDeviceId();
        QString holder;
        for (int other = 0; other < kNumMics; ++other) {
            if (other != m_index && m_pTargets->micInputDeviceName(other) == id.name) {
                holder = tr("MIC %1").arg(other + 1);
                break;
            }
        }
        const bool isCurrent = (id.name == current);
        currentListed = currentListed || isCurrent;

        QString text = pDevice->getDisplayName();
        if (!holder.isEmpty()) {
            text = QStringLiteral("%1 (%2)").arg(text, holder);
        }
        QAction* pAction = menu.addAction(text);
        pAction->setCheckable(true);
        pAction->setChecked(isCurrent);
        pAction->setEnabled(holder.isEmpty());
        connect(pAction, &QAction::triggered, this, [&picked, &chosen, id]() {
            picked = true;
            chosen = id;
        });
    }

    if (!currentListed) {
        QAction* pMissing = menu.addAction(
                tr("%1 (not connected)").arg(current));
        pMissing->setCheckable(true);
        pMissing->setChecked(true);
        pMissing->setEnabled(false);
    }

    menu.exec(globalPos);
    if (!picked) {
        return;
    }
    // The same device again: nothing to do.
    if (chosen.name == current) {
        return;
    }

    // None on a mic with no live input only removes its saved keys, so the
    // audio does not stop.
    const bool keysOnly = chosen.name.isEmpty() && !m_pInputConfigured->toBool();
    if (!keysOnly && deckIsPlaying()) {
        const auto answer = QMessageBox::question(this,
                tr("Mix Two"),
                tr("The audio stops for a short time when the input changes. "
                   "Continue?"),
                QMessageBox::Ok | QMessageBox::Cancel,
                QMessageBox::Cancel);
        if (answer != QMessageBox::Ok) {
            return;
        }
    }

    switch (m_pTargets->assignMicInput(m_index, chosen)) {
    case AudioTargetManager::MicAssignResult::Applied:
        break;
    case AudioTargetManager::MicAssignResult::Rejected:
    case AudioTargetManager::MicAssignResult::PreviousActive:
        QMessageBox::warning(this,
                tr("Mix Two"),
                tr("Mix Two cannot open this device. "
                   "The previous input is active."));
        break;
    case AudioTargetManager::MicAssignResult::MicrophonesOff:
        QMessageBox::warning(this,
                tr("Mix Two"),
                AudioTargetManager::microphonesOffText());
        break;
    case AudioTargetManager::MicAssignResult::AudioOff:
        // No setup came back, so no text can say what is active. The
        // manager writes the failure to the log.
        break;
    }
}

// End of AI-generated text
