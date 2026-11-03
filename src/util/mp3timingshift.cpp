#include "util/mp3timingshift.h"

#include <mp3guessenc.h>

#include <QMutex>
#include <QMutexLocker>

namespace mixxx {

int mp3guessencTimingShiftCase(const QString& filePath) {
    // Protects the global variables of mp3guessenc during one call.
    static QMutex s_mutex;
    const QMutexLocker locker(&s_mutex);
    return mp3guessenc_timing_shift_case(filePath.toStdString().c_str());
}

} // namespace mixxx
