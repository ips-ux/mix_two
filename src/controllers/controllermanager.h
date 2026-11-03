#pragma once

#include <QMutex>
#include <QSharedPointer>
#include <QTimer>
#include <QStringList>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "controllers/controllerenumerator.h"
#include "preferences/usersettings.h"
#include "util/duration.h"

// Forward declaration(s)
class Controller;
class ControllerLearningEventFilter;
class MappingInfoEnumerator;
class LegacyControllerMapping;
class ControllerEnumerator;

/// Function to sort controllers by name
bool controllerCompare(Controller *a, Controller *b);

/// Manages enumeration/operation/deletion of hardware controllers.
class ControllerManager : public QObject {
    Q_OBJECT
  public:
    explicit ControllerManager(UserSettingsPointer pConfig);
    ~ControllerManager() override;

    static const mixxx::Duration kPollInterval;

    QList<Controller*> getControllers() const;
    QList<Controller*> getControllerList(bool outputDevices=true, bool inputDevices=true);
    ControllerLearningEventFilter* getControllerLearningEventFilter() const;
    QSharedPointer<MappingInfoEnumerator> getMainThreadUserMappingEnumerator() const {
        return m_pMainThreadUserMappingEnumerator;
    }
    QSharedPointer<MappingInfoEnumerator> getMainThreadSystemMappingEnumerator() const {
        return m_pMainThreadSystemMappingEnumerator;
    }
    QString getConfiguredMappingFileForDevice(const QString& name) const;

    /// Prevent other parts of Mixxx from having to manually connect to our slots
    void setUpDevices() { emit requestSetUpDevices(); };

    static QList<QString> getMappingPaths(UserSettingsPointer pConfig);

  signals:
    void initialized();
    void devicesChanged();
    void requestSetUpDevices();
    void requestShutdown();
    void requestInitialize();
    void mappingApplied(bool applied);

  public slots:
    void slotApplyMapping(Controller* pController,
            std::shared_ptr<LegacyControllerMapping> pMapping,
            bool bEnabled);

  private slots:
    /// Perform initialization that should be delayed until the ControllerManager
    /// thread is started.
    void slotInitialize();
    /// Open whatever controllers are selected in the preferences. This currently
    /// only runs on start-up but maybe should instead be signaled by the
    /// preferences dialog on apply, and only open/close changed devices
    void slotSetUpDevices();
    void slotShutdown();
    /// Calls poll() on all devices that have isPolling() true.
    void slotPollDevices();
    /// Compares the platform's MIDI port list with the one PortMidi was
    /// initialized with and rescans MIDI devices once a change has settled.
    void slotCheckMidiHotplug();
    /// Deletes controllers retired by rescans up to and including this one,
    /// once the main thread has handled the matching devicesChanged().
    void slotReleaseRetiredControllers(int rescanGeneration);

  private:
    void updateControllerList();
    /// Stores the first mapping matching this never-configured device and
    /// enables it. Returns false if no mapping declares the device.
    bool autoConfigureDevice(Controller* pController,
            const QString& deviceName,
            const QList<MappingInfoEnumerator*>& enumerators);
    /// Opens the devices that are enabled in the config, auto-enabling
    /// never-configured devices that a mapping declares.
    void setUpDevices(const QList<Controller*>& devices);
    void rescanMidiDevices();
    /// Retries opening enabled controllers whose open() failed, e.g. because
    /// the port was briefly busy or the device was still starting up.
    void retryFailedOpens();
    bool isKnownController(Controller* pController) const;
    void startPolling();
    void stopPolling();
    void pollIfAnyControllersOpen();
    void openController(Controller* pController);
    void closeController(Controller* pController);

    UserSettingsPointer m_pConfig;
    // WARNING: Do not parent m_pControllerLearningEventFilter to ControllerManager
    // because the CM is moved to its own thread and runs its own event loop.
    std::unique_ptr<ControllerLearningEventFilter> m_pControllerLearningEventFilter;
    QTimer m_pollTimer;
    mutable QMutex m_mutex;
    /// Guarded by m_mutex for main-thread reads.
    /// Written/iterated only on the ControllerManager thread
    std::vector<std::unique_ptr<ControllerEnumerator>> m_enumerators;
    /// Non-owning; Controllers are owned by their respective ControllerEnumerator.
    /// Guarded by m_mutex for main-thread reads.
    /// Written only on the ControllerManager thread
    QList<Controller*> m_controllers;
    /// The single shared background thread that drives the entire ControllerManager,
    /// all ControllerEnumerators, and all Controller instances.
    std::unique_ptr<QThread> m_pThread;
    /// Written once on the ControllerManager thread during slotInitialize(),
    /// before initialized() is emitted. Afterwards only read from the main thread
    QSharedPointer<MappingInfoEnumerator> m_pMainThreadUserMappingEnumerator;
    QSharedPointer<MappingInfoEnumerator> m_pMainThreadSystemMappingEnumerator;
    /// Accessed only from the ControllerManager thread via slotPollDevices().
    bool m_skipPoll;

    // MIDI hotplug state, accessed only from the ControllerManager thread.
    QTimer m_hotplugTimer;
    /// Owned by m_enumerators; null without PortMidi.
    ControllerEnumerator* m_pPortMidiEnumerator;
    /// True if the platform can list MIDI ports and hotplug is enabled.
    bool m_midiHotplugAvailable;
    /// Port list PortMidi was last initialized with.
    QStringList m_midiPortNames;
    /// Changed port list seen on the previous check, waiting to settle.
    std::optional<QStringList> m_pendingMidiPortNames;
    int m_rescanGeneration;
    /// Controllers replaced by a rescan, kept alive until the main thread has
    /// dropped its pointers to them. Owned; tagged with their rescan generation.
    QList<std::pair<int, Controller*>> m_retiredControllers;
    /// Enabled controllers whose open() failed, retried on hotplug checks.
    /// Non-owning.
    QList<Controller*> m_openRetryControllers;
    int m_openRetryChecksLeft;
};
