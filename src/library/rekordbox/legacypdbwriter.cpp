// AI-generated (Claude Code). Start of AI-generated text.

#include <QObject>

#include "library/rekordbox/legacylibrary.h"

namespace mixxx {
namespace rekordbox {

// Placeholder: the PDB writer is not built yet. The export reports that it
// cannot write the legacy device library.
QByteArray legacyPdbBytes(const LegacyLibrary& library, QString* pError) {
    Q_UNUSED(library);
    if (pError) {
        *pError = QObject::tr("Mix Two cannot write the Device Library at this time.");
    }
    return {};
}

} // namespace rekordbox
} // namespace mixxx

// End of AI-generated text.
