#pragma once

// required for Qt-Macros
#include <qobjectdefs.h>

namespace mixxx {
Q_NAMESPACE

enum class OverviewType {
    Filtered,
    HSV,
    RGB,
    // Persisted by number: add new types at the end only.
    ColorSaturated,
};
Q_ENUM_NS(OverviewType);

} // namespace mixxx
