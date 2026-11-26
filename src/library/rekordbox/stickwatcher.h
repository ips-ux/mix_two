#pragma once
// AI-generated (Claude Code). Start of AI-generated text.

#include <QHash>
#include <QObject>
#include <QStringList>
#include <memory>

class QAbstractNativeEventFilter;
class QThread;

namespace mixxx::rekordbox {

struct StickFormats {
    QString legacyPath;
    QString deviceLibraryPlusPath;
    bool hasLegacy() const { return !legacyPath.isEmpty(); }
    bool hasDeviceLibraryPlus() const { return !deviceLibraryPlusPath.isEmpty(); }
};

StickFormats detectStickFormats(const QString& root);
QStringList driveRootsFromMask(quint32 mask);

class StickWatcher : public QObject {
    Q_OBJECT
  public:
    explicit StickWatcher(QObject* parent = nullptr);
    ~StickWatcher() override;

    void start();
    void refresh();
    void volumeArrived(const QString& root);
    void volumeDeparted(const QString& root);
    void applySnapshot(const QHash<QString, QString>& mounts, bool recheck);
    void forgetVolume(const QString& root);

  signals:
    // An arrival event comes from the system (a stick is plugged in). The
    // startup scan, a refresh and a Linux mount table change report false.
    void volumeAdded(const QString& root, bool arrivalEvent);
    void volumeRemoved(const QString& root);
    void volumeRecheckRequested(const QString& root);

  private:
    void scanMounts(bool recheck);

    QHash<QString, QString> m_mounts;
    std::unique_ptr<QAbstractNativeEventFilter> m_nativeFilter;
    std::unique_ptr<QThread> m_mountMonitor;
    bool m_snapshotPending = false;
    bool m_rescanPending = false;
    bool m_recheckPending = false;
};

} // namespace mixxx::rekordbox
// End of AI-generated text.
