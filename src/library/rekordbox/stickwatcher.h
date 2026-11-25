#pragma once
// AI-generated (Claude Code). Start of AI-generated text.

#include <QObject>
#include <QSet>
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

  signals:
    void volumeAdded(const QString& root);
    void volumeRemoved(const QString& root);
    void volumeRecheckRequested(const QString& root);

  private:
    void scanMounts(bool recheck);
    void applySnapshot(const QSet<QString>& roots, bool recheck);

    QSet<QString> m_roots;
    std::unique_ptr<QAbstractNativeEventFilter> m_nativeFilter;
    std::unique_ptr<QThread> m_mountMonitor;
    bool m_snapshotPending = false;
    bool m_rescanPending = false;
    bool m_recheckPending = false;
};

} // namespace mixxx::rekordbox
// End of AI-generated text.
