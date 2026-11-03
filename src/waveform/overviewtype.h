#pragma once

// required for Qt-Macros
#include <qobjectdefs.h>

namespace mixxx {
Q_NAMESPACE

enum class OverviewType {
    Filtered,
    HSV,
    RGB,
    // AI-generated (Claude Code). Start of AI-generated text.
    // Persisted by number: add new types at the end only.
    ColorSaturated,
    // End of AI-generated text.
};
Q_ENUM_NS(OverviewType);

} // namespace mixxx
