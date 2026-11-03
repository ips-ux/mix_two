#include "controllers/controllermanager.h"

#include <QCoreApplication>
#include <QPointer>
#include <QSet>
#include <QThread>

#include "controllers/controller.h"
#include "controllers/controllerlearningeventfilter.h"
#include "controllers/controllermappinginfoenumerator.h"
#include "controllers/defs_controllers.h"
#include "controllers/legacycontrollermappingfilehandler.h"
#include "moc_controllermanager.cpp"
#include "preferences/usersettings.h"
#include "util/cmdlineargs.h"
#include "util/compatibility/qmutex.h"
#include "util/duration.h"
#include "util/thread_affinity.h"
#include "util/time.h"

#ifdef __PORTMIDI__
#include "controllers/midi/midiportwatcher.h"
#include "controllers/midi/portmidienumerator.h"
#endif

#ifdef __HSS1394__
#include "controllers/midi/hss1394enumerator.h"
#endif

#ifdef __HID__
#include "controllers/hid/hidenumerator.h"
#endif

#ifdef __BULK__
#include "controllers/bulk/bulkenumerator.h"
#endif

// http://developer.qt.nokia.com/wiki/Threads_Events_QObjects

// Poll every 1ms (where possible) for good controller response
#ifdef __LINUX__
// Many Linux distros ship with the system tick set to 250Hz so 1ms timer
// reportedly causes CPU hosage. See https://github.com/mixxxdj/mixxx/issues/6383
// rryan 6/2012
const mixxx::Duration ControllerManager::kPollInterval = mixxx::Duration::fromMillis(5);
#else
const mixxx::Duration ControllerManager::kPollInterval = mixxx::Duration::fromMillis(1);
#endif

namespace {
/// Strip slashes and spaces from device name, so that it can be used as config
/// key or a filename.
QString sanitizeDeviceName(QString name) {
    return name.replace(" ", "_").replace("/", "_").replace("\\", "_");
}

QFileInfo findMappingFile(const QString& pathOrFilename, const QStringList& paths) {
    QFileInfo fileInfo(pathOrFilename);
    if (fileInfo.isAbsolute()) {
        return fileInfo;
    }

    for (const QString& path : paths) {
        fileInfo = QFileInfo(QDir(path).absoluteFilePath(pathOrFilename));
        if (fileInfo.exists()) {
            return fileInfo;
        }
    }

    return QFileInfo();
}

// Legacy code referred to mappings as "presets", so "[ControllerPreset]" must be
// kept for backwards compatibility.
const QString kSettingsGroup = QLatin1String("[ControllerPreset]");

// How often the platform's MIDI port list is compared for hotplug detection.
// A change is acted on once it is stable for two checks, so that a device's
// input and output ports are both present.
const mixxx::Duration kHotplugCheckInterval = mixxx::Duration::fromMillis(1000);

// How many hotplug checks a controller that failed to open is retried for.
constexpr int kOpenRetryChecks = 30;

} // anonymous namespace

QString firstAvailableFilename(QSet<QString>& filenames,
        const QString& originalFilename) {
    QString filename = originalFilename;
    int i = 1;
    while (filenames.contains(filename)) {
        i++;
        filename = QString("%1--%2").arg(originalFilename, QString::number(i));
    }
    filenames.insert(filename);
    return filename;
}

bool controllerCompare(Controller *a,Controller *b) {
    return a->getName() < b->getName();
}

ControllerManager::ControllerManager(UserSettingsPointer pConfig)
        : QObject(),
          m_pConfig(pConfig),
          // WARNING: Do not parent m_pControllerLearningEventFilter to
          // ControllerManager because the CM, together with all its children,
          // is moved to the ControllerManager thread (m_pThread) and
          // runs its own event loop there.
          m_pControllerLearningEventFilter(
                  std::make_unique<ControllerLearningEventFilter>()),
          m_pollTimer(this),
          m_pThread(std::make_unique<QThread>()),
          m_skipPoll(false),
          m_hotplugTimer(this),
          m_pPortMidiEnumerator(nullptr),
          m_midiHotplugAvailable(false),
          m_rescanGeneration(0),
          m_openRetryChecksLeft(0) {
    qRegisterMetaType<std::shared_ptr<LegacyControllerMapping>>(
            "std::shared_ptr<LegacyControllerMapping>");

    // Create controller mapping paths in the user's home directory.
    QString userMappings = userMappingsPath(m_pConfig);
    if (!QDir(userMappings).exists()) {
        qDebug() << "Creating user controller mappings directory:" << userMappings;
        QDir().mkpath(userMappings);
    }

    m_pollTimer.setInterval(kPollInterval.toIntegerMillis());
    connect(&m_pollTimer, &QTimer::timeout, this, &ControllerManager::slotPollDevices);
    m_hotplugTimer.setInterval(kHotplugCheckInterval.toIntegerMillis());
    connect(&m_hotplugTimer,
            &QTimer::timeout,
            this,
            &ControllerManager::slotCheckMidiHotplug);

    m_pThread->setObjectName("ControllerManager");

    // Move the entire ControllerManager object and all its children (including
    // the poll timer) onto the ControllerManager thread.
    moveToThread(m_pThread.get());

    // The ControllerManager thread is high-priority because controller input can
    // affect audio output directly (e.g. scratching).
    m_pThread->start(QThread::HighPriority);

    connect(this, &ControllerManager::requestInitialize, this, &ControllerManager::slotInitialize);
    connect(this,
            &ControllerManager::requestSetUpDevices,
            this,
            &ControllerManager::slotSetUpDevices);
    connect(this, &ControllerManager::requestShutdown, this, &ControllerManager::slotShutdown);

    // Signal that we should run slotInitialize once our event loop has started
    // up. invokeMethod with QueuedConnection will post an event to our event loop,
    // so slotInitialize will not run until after the ControllerManager thread is fully
    // started and ready to process events.
    QMetaObject::invokeMethod(this, &ControllerManager::slotInitialize, Qt::QueuedConnection);
}

ControllerManager::~ControllerManager() {
    // slotShutdown() closes controllers, deletes enumerators and calls
    // m_pThread->quit(). We must wait for the thread to finish before our
    // members (m_pollTimer, m_pControllerLearningEventFilter, …) are destroyed,
    // because they may still be accessed by the thread's event loop.
    emit requestShutdown(); // clazy:exclude=incorrect-emit
    m_pThread->wait();
    // m_pThread and m_pControllerLearningEventFilter are released by unique_ptr
    // after this point, in reverse declaration order.
}

ControllerLearningEventFilter* ControllerManager::getControllerLearningEventFilter() const {
    return m_pControllerLearningEventFilter.get();
}

void ControllerManager::slotInitialize() {
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    qDebug() << "ControllerManager:slotInitialize";

    // Initialize mapping info parsers. This object is only for use in the main
    // thread. Do not touch it from within ControllerManager.
    m_pMainThreadUserMappingEnumerator =
            QSharedPointer<MappingInfoEnumerator>::create(
                    userMappingsPath(m_pConfig));
    m_pMainThreadSystemMappingEnumerator =
            QSharedPointer<MappingInfoEnumerator>::create(
                    resourceMappingsPath(m_pConfig));

    // Instantiate all enumerators. Enumerators can take a long time to
    // construct since they interact with host MIDI APIs.
    {
        auto locker = lockMutex(&m_mutex);
#ifdef __PORTMIDI__
        // Read before PortMidi initializes so that a device connected while
        // it does is seen as a change.
        const std::optional<QStringList> midiPortNames = mixxx::midi::currentPortNames();
        m_enumerators.push_back(std::make_unique<PortMidiEnumerator>(m_pConfig));
        m_pPortMidiEnumerator = m_enumerators.back().get();
        if (midiPortNames && m_pConfig->getValue(kMidiHotplugEnabledCfgKey, true)) {
            // The timer starts after the first slotSetUpDevices().
            m_midiPortNames = *midiPortNames;
            m_midiHotplugAvailable = true;
        }
#endif
#ifdef __HSS1394__
        m_enumerators.push_back(std::make_unique<Hss1394Enumerator>());
#endif
#ifdef __BULK__
        m_enumerators.push_back(std::make_unique<BulkEnumerator>());
#endif
#ifdef __HID__
        m_enumerators.push_back(std::make_unique<HidEnumerator>());
#endif
    } // Mutex locker released here
    emit initialized();
}

void ControllerManager::slotShutdown() {
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    stopPolling();
    m_hotplugTimer.stop();
    m_openRetryControllers.clear();
    slotReleaseRetiredControllers(m_rescanGeneration);

    // Clear m_enumerators before deleting the enumerators to prevent other code
    // paths from accessing them during teardown.
    auto locker = lockMutex(&m_mutex);
    std::vector<std::unique_ptr<ControllerEnumerator>> enumerators =
            std::move(m_enumerators); // m_enumerators is guaranteed empty after move
    locker.unlock();

    // Delete enumerators (and their owned Controllers) by letting unique_ptrs
    // go out of scope here — no raw deletes needed.
    enumerators.clear();

    // Stop the event loop after the enumerators are torn down, since the
    // controller scripting engines live inside the enumerators.
    m_pThread->quit();
}

void ControllerManager::updateControllerList() {
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    // NOTE: Currently this function is only called on startup. If hotplug is added, changes to the
    // controller list must be synchronized with dlgprefcontrollers to avoid dangling connections
    // and possible crashes.
    auto locker = lockMutex(&m_mutex);
    if (m_enumerators.empty()) {
        qWarning() << "updateControllerList called but no enumerators have been added!";
        return;
    }
    // Take a snapshot of the enumerator pointers while holding the lock.
    std::vector<ControllerEnumerator*> enumerators;
    enumerators.reserve(m_enumerators.size());
    for (const auto& pEnumerator : m_enumerators) {
        enumerators.push_back(pEnumerator.get());
    }
    locker.unlock();

    QList<Controller*> newDeviceList;
    for (ControllerEnumerator* pEnumerator : enumerators) {
        newDeviceList.append(pEnumerator->queryDevices());
    }

    locker.relock();
    if (newDeviceList == m_controllers) {
        return;
    }
    m_controllers = std::move(newDeviceList);
    locker.unlock();
    emit devicesChanged();
}

QList<Controller*> ControllerManager::getControllers() const {
    const auto locker = lockMutex(&m_mutex);
    return m_controllers;
}

QList<Controller*> ControllerManager::getControllerList(bool bOutputDevices, bool bInputDevices) {
    qDebug() << "ControllerManager::getControllerList";

    auto locker = lockMutex(&m_mutex);
    QList<Controller*> controllers = m_controllers;
    locker.unlock();

    // Create a list of controllers filtered to match the given input/output
    // options.
    QList<Controller*> filteredDeviceList;

    for (Controller* device : controllers) {
        if ((bOutputDevices == device->isOutputDevice()) ||
            (bInputDevices == device->isInputDevice())) {
            filteredDeviceList.push_back(device);
        }
    }
    return filteredDeviceList;
}

QString ControllerManager::getConfiguredMappingFileForDevice(const QString& name) const {
    // Thread-Safe : ConfigObject<ValueType>::getValueString/get is protected by QWriteLocker
    return m_pConfig->getValueString(ConfigKey(kSettingsGroup, sanitizeDeviceName(name)));
}

void ControllerManager::slotSetUpDevices() {
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    qDebug() << "ControllerManager: Setting up devices";

    updateControllerList();
    setUpDevices(getControllerList(false, true));

    // Only after the initial enumeration: a rescan before it would hand out
    // controllers that updateControllerList() then deletes.
    if (m_midiHotplugAvailable && !m_hotplugTimer.isActive()) {
        m_hotplugTimer.start();
    }
}

void ControllerManager::setUpDevices(const QList<Controller*>& devices) {
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    QStringList mappingPaths(getMappingPaths(m_pConfig));
    // Created on first use. The main-thread enumerators must not be used here.
    std::unique_ptr<MappingInfoEnumerator> pUserMappingEnumerator;
    std::unique_ptr<MappingInfoEnumerator> pSystemMappingEnumerator;

    for (Controller* pController : devices) {
        QString name = pController->getName();
        m_openRetryControllers.removeAll(pController);

        if (pController->isOpen()) {
            pController->close();
        }

        // The filename for this device name.
        QString deviceName = sanitizeDeviceName(name);

        // Plug and play: a device that has never been configured is enabled
        // with a mapping that declares it. Devices the user configured or
        // disabled keep their settings.
        if (!m_pConfig->exists(ConfigKey("[Controller]", deviceName)) &&
                !m_pConfig->exists(ConfigKey(kSettingsGroup, deviceName)) &&
                m_pConfig->getValue(kAutoEnableMatchingMappingsCfgKey, true)) {
            if (!pSystemMappingEnumerator) {
                pUserMappingEnumerator = std::make_unique<MappingInfoEnumerator>(
                        userMappingsPath(m_pConfig));
                pSystemMappingEnumerator = std::make_unique<MappingInfoEnumerator>(
                        resourceMappingsPath(m_pConfig));
            }
            autoConfigureDevice(pController,
                    deviceName,
                    {pUserMappingEnumerator.get(), pSystemMappingEnumerator.get()});
        }

        // Check if device is enabled
        if (!m_pConfig->getValue(ConfigKey("[Controller]", deviceName), 0)) {
            continue;
        }

        // Check if device has a configured mapping
        QString mappingFilePath = getConfiguredMappingFileForDevice(deviceName);
        if (mappingFilePath.isEmpty()) {
            continue;
        }

        qDebug() << "Searching for controller mapping" << mappingFilePath
                 << "in paths:" << mappingPaths.join(",");
        QFileInfo mappingFile = findMappingFile(mappingFilePath, mappingPaths);
        if (!mappingFile.exists()) {
            qDebug() << "Could not find" << mappingFilePath << "in any mapping path.";
            continue;
        }

        std::shared_ptr<LegacyControllerMapping> pMapping =
                LegacyControllerMappingFileHandler::loadMapping(
                        mappingFile, resourceMappingsPath(m_pConfig));

        if (!pMapping) {
            continue;
        }
        pMapping->loadSettings(m_pConfig, pController->getName());

        // This runs on the main thread but LegacyControllerMapping is not thread safe, so clone it.
        pController->setMapping(std::move(pMapping));

        // If we are in safe mode, skip opening controllers.
        if (CmdlineArgs::Instance().getSafeMode()) {
            qDebug() << "We are in safe mode -- skipping opening controller.";
            continue;
        }

        qDebug() << "Opening controller:" << name;

        int value = pController->open(m_pConfig->getResourcePath());
        if (value != 0) {
            qWarning() << "There was a problem opening" << name;
            if (m_midiHotplugAvailable) {
                qInfo() << "ControllerManager: retrying to open" << name
                        << "for" << kOpenRetryChecks << "checks";
                m_openRetryControllers.append(pController);
                m_openRetryChecksLeft = kOpenRetryChecks;
            }
            continue;
        }
    }

    pollIfAnyControllersOpen();
}

void ControllerManager::retryFailedOpens() {
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    if (m_openRetryControllers.isEmpty()) {
        return;
    }
    if (--m_openRetryChecksLeft < 0) {
        for (Controller* pController : std::as_const(m_openRetryControllers)) {
            qWarning() << "ControllerManager: giving up opening" << pController->getName();
        }
        m_openRetryControllers.clear();
        return;
    }

    bool anyOpened = false;
    const QList<Controller*> controllers = m_openRetryControllers;
    for (Controller* pController : controllers) {
        const QString deviceName = sanitizeDeviceName(pController->getName());
        // Opened from the preferences meanwhile, or disabled by the user.
        if (pController->isOpen() ||
                !m_pConfig->getValue(ConfigKey("[Controller]", deviceName), 0)) {
            m_openRetryControllers.removeAll(pController);
            continue;
        }
        if (pController->open(m_pConfig->getResourcePath()) == 0) {
            qInfo() << "ControllerManager: opened" << pController->getName() << "on retry";
            m_openRetryControllers.removeAll(pController);
            anyOpened = true;
        }
    }
    if (anyOpened) {
        pollIfAnyControllersOpen();
    }
}

bool ControllerManager::autoConfigureDevice(Controller* pController,
        const QString& deviceName,
        const QList<MappingInfoEnumerator*>& enumerators) {
    // Enumerators are in priority order, so user mappings win over system ones.
    for (MappingInfoEnumerator* pEnumerator : enumerators) {
        const QList<MappingInfo> mappings =
                pEnumerator->getMappingsByExtension(pController->mappingExtension());
        for (const MappingInfo& mapping : mappings) {
            if (!pController->matchMapping(mapping)) {
                continue;
            }
            qInfo() << "ControllerManager: auto-enabling" << pController->getName()
                    << "with matching mapping" << mapping.getPath();
            m_pConfig->set(ConfigKey(kSettingsGroup, deviceName), mapping.getPath());
            m_pConfig->setValue(ConfigKey("[Controller]", deviceName), 1);
            return true;
        }
    }
    return false;
}

bool ControllerManager::isKnownController(Controller* pController) const {
    const auto locker = lockMutex(&m_mutex);
    return m_controllers.contains(pController);
}

void ControllerManager::slotCheckMidiHotplug() {
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    retryFailedOpens();
#ifdef __PORTMIDI__
    const std::optional<QStringList> portNames = mixxx::midi::currentPortNames();
    if (!portNames || *portNames == m_midiPortNames) {
        m_pendingMidiPortNames.reset();
        return;
    }
    if (m_pendingMidiPortNames != portNames) {
        // Wait until the port list stops changing.
        m_pendingMidiPortNames = portNames;
        return;
    }
    // The learning wizard holds on to its controller, so wait until it ends.
    for (Controller* pController : std::as_const(m_controllers)) {
        if (pController->isLearning()) {
            return;
        }
    }
    m_pendingMidiPortNames.reset();
    m_midiPortNames = *portNames;
    rescanMidiDevices();
#endif
}

void ControllerManager::rescanMidiDevices() {
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
#ifdef __PORTMIDI__
    VERIFY_OR_DEBUG_ASSERT(m_pPortMidiEnumerator) {
        return;
    }
    auto* pEnumerator = static_cast<PortMidiEnumerator*>(m_pPortMidiEnumerator);
    qInfo() << "ControllerManager: MIDI devices changed, rescanning."
            << "Open MIDI controllers are reopened.";

    // PortMidi can only see new devices after a restart, which invalidates
    // every PortMidi controller, so all of them are replaced.
    const QList<Controller*> retired = pEnumerator->releaseDevicesForRescan();
    m_openRetryControllers.removeIf([&retired](Controller* pController) {
        return retired.contains(pController);
    });
    const QList<Controller*> midiDevices = pEnumerator->queryDevices();

    auto locker = lockMutex(&m_mutex);
    QList<Controller*> controllers;
    for (Controller* pController : std::as_const(m_controllers)) {
        if (!retired.contains(pController)) {
            controllers.append(pController);
        }
    }
    controllers.append(midiDevices);
    m_controllers = std::move(controllers);
    locker.unlock();

    // The main thread may still hold pointers to the retired controllers
    // (preference pages, QML proxies) until it handles devicesChanged().
    // Delete them only after that: events posted to the main thread are
    // handled in order, so the round trip below runs after those handlers.
    const int generation = ++m_rescanGeneration;
    for (Controller* pController : retired) {
        m_retiredControllers.append(std::make_pair(generation, pController));
    }
    emit devicesChanged();
    QMetaObject::invokeMethod(
            qApp,
            [pManager = QPointer<ControllerManager>(this), generation]() {
                if (pManager) {
                    QMetaObject::invokeMethod(
                            pManager,
                            [pManager, generation]() {
                                pManager->slotReleaseRetiredControllers(generation);
                            },
                            Qt::QueuedConnection);
                }
            },
            Qt::QueuedConnection);

    // Same selection as getControllerList(false, true) in slotSetUpDevices().
    QList<Controller*> newDevices;
    for (Controller* pController : midiDevices) {
        if (!pController->isOutputDevice() || pController->isInputDevice()) {
            newDevices.append(pController);
        }
    }
    setUpDevices(newDevices);
#endif
}

void ControllerManager::slotReleaseRetiredControllers(int rescanGeneration) {
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    auto it = m_retiredControllers.begin();
    while (it != m_retiredControllers.end()) {
        if (it->first <= rescanGeneration) {
            delete it->second;
            it = m_retiredControllers.erase(it);
        } else {
            ++it;
        }
    }
}

void ControllerManager::pollIfAnyControllersOpen() {
    // Not Thread-Safe because calls startPolling()/stopPolling()
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    auto locker = lockMutex(&m_mutex);
    QList<Controller*> controllers = m_controllers;
    locker.unlock();

    bool shouldPoll = false;
    for (Controller* pController : controllers) {
        if (pController->isOpen() && pController->isPolling()) {
            shouldPoll = true;
        }
    }
    if (shouldPoll) {
        startPolling();
    } else {
        stopPolling();
    }
}

void ControllerManager::startPolling() {
    // Not Thread-Safe because QTimer::start() must be called from the thread that owns the timer.
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    // Start the polling timer.
    if (!m_pollTimer.isActive()) {
        m_pollTimer.start();
        qDebug() << "Controller polling started.";
    }
}

void ControllerManager::stopPolling() {
    // Not Thread-Safe because QTimer::stop() must be called from the thread that owns the timer.
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    m_pollTimer.stop();
    qDebug() << "Controller polling stopped.";
}

void ControllerManager::slotPollDevices() {
    // Not Thread-Safe because it accesses m_controllers and m_skipPoll without a mutex.
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    // Note: this function is called from a high priority thread which
    // may stall the GUI or may reduce the available CPU time for other
    // High Priority threads like caching reader or broadcasting more
    // then desired, if it is called endless loop like.
    //
    // This especially happens if a controller like the 3x Speed
    // Stanton SCS.1D emits more massages than Mixxx is able to handle
    // or a controller like Hercules RMX2 goes wild. In such a case the
    // receive buffer is stacked up every call to insane values > 500 messages.
    //
    // To avoid this we pick here a strategies similar like the audio
    // thread. In case pollDevice() takes longer than a call cycle
    // we are cooperative a skip the next cycle to free at least some
    // CPU time
    //
    // Some random test data from a i5-3317U CPU @ 1.70GHz Running
    // Ubuntu Trusty:
    // * Idle poll: ~5 µs.
    // * 5 messages burst (full midi bandwidth): ~872 µs.

    if (m_skipPoll) {
        // skip poll in overload situation
        m_skipPoll = false;
        //qDebug() << "ControllerManager::pollDevices() skip";
        return;
    }

    mixxx::Duration start = mixxx::Time::elapsed();
    for (Controller* pDevice : std::as_const(m_controllers)) {
        if (pDevice->isOpen() && pDevice->isPolling()) {
            pDevice->poll();
        }
    }

    mixxx::Duration duration = mixxx::Time::elapsed() - start;
    if (duration > kPollInterval) {
        m_skipPoll = true;
    }
    //qDebug() << "ControllerManager::pollDevices()" << duration << start;
}

void ControllerManager::openController(Controller* pController) {
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    if (!pController) {
        return;
    }
    if (!isKnownController(pController)) {
        qWarning() << "openController: controller was replaced by a device rescan";
        return;
    }
    if (pController->isOpen()) {
        pController->close();
    }
    int result = pController->open(m_pConfig->getResourcePath());
    pollIfAnyControllersOpen();

    // If successfully opened the device, apply the mapping and save the
    // preference setting.
    if (result == 0) {
        // Update configuration to reflect controller is enabled.
        m_pConfig->setValue(
                ConfigKey("[Controller]", sanitizeDeviceName(pController->getName())), 1);
    }
}

void ControllerManager::closeController(Controller* pController) {
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    if (!pController) {
        return;
    }
    pController->close();
    pollIfAnyControllersOpen();
    // Update configuration to reflect controller is disabled.
    m_pConfig->setValue(
            ConfigKey("[Controller]", sanitizeDeviceName(pController->getName())), 0);
}

// This needs to be called in a Qt::BlockingQueuedConnection so that the
// signaling thread can't alter the LegacyControllerMapping during applying
void ControllerManager::slotApplyMapping(Controller* pController,
        std::shared_ptr<LegacyControllerMapping> pMapping,
        bool bEnabled) {
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    VERIFY_OR_DEBUG_ASSERT(pController) {
        qWarning() << "slotApplyMapping got invalid controller!";
        return;
    }
    // The preferences may still show a controller that a MIDI rescan has
    // just replaced.
    if (!isKnownController(pController)) {
        qWarning() << "slotApplyMapping: controller was replaced by a device rescan";
        emit mappingApplied(false);
        return;
    }

    closeController(pController);
    ConfigKey key(kSettingsGroup, sanitizeDeviceName(pController->getName()));
    if (!pMapping) {
        // Unset the controller mapping for this controller
        pController->setMapping(nullptr);
        m_pConfig->remove(key);
        emit mappingApplied(false);
        return;
    }

    VERIFY_OR_DEBUG_ASSERT(!pMapping->isDirty()) {
        qWarning() << "Mapping is dirty, changes might be lost on restart!";
    }

    // Save the file path/name in the config so it can be auto-loaded at
    // startup next time
    m_pConfig->set(key, pMapping->filePath());

    pController->setMapping(std::move(pMapping));

    if (bEnabled) {
        emit mappingApplied(pController->isMappable());
    } else {
        emit mappingApplied(false);
        return;
    }

    // Note: openController() may call ControllerRenderingEngine::setup()
    // Which has a blocking invokeMethod() call for QOffscreenSurface::create()
    // That why we need to return from this blocking call first.
    QMetaObject::invokeMethod(
            this,
            [this, pController]() { openController(pController); },
            Qt::QueuedConnection);
}

// static
QList<QString> ControllerManager::getMappingPaths(UserSettingsPointer pConfig) {
    QList<QString> scriptPaths;
    scriptPaths.append(userMappingsPath(pConfig));
    scriptPaths.append(resourceMappingsPath(pConfig));
    return scriptPaths;
}
