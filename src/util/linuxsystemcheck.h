#pragma once
// AI-generated (Claude Code). Start of AI-generated text.

#include <QString>

namespace mixxx {

/// True when the kernel `release` is older than `major`.`minor`. `release` is
/// the text that `uname -r` gives, for example "6.3.9-generic". Only the
/// first two numbers count; the rest of the text is ignored. A text that does
/// not start with two numbers is not "older", so the notice never shows for
/// a kernel name that Mix Two cannot read.
bool kernelOlderThan(const QString& release, int major, int minor);

/// The oldest Linux kernel that works well with the DDJ-800 (USB audio class 2
/// mode of the controller). Choice: 6.4.
constexpr int kDdj800KernelMajor = 6;
constexpr int kDdj800KernelMinor = 4;

/// True when the text of `/proc/asound/cards` lists a DDJ-800 sound card.
/// The comparison ignores letter case.
bool soundCardsListDdj800(const QString& cardsText);

/// The decision "show a notice": the condition holds and the user has not
/// seen the notice in this profile before.
bool shouldShowNotice(bool conditionHolds, bool alreadyShown);

/// The kernel release of this computer, as `uname -r` gives it. An empty
/// string when the system does not give it.
QString currentKernelRelease();

/// True when a DDJ-800 is connected. Linux reads `/proc/asound/cards`; every
/// other system returns false.
bool isDdj800Connected();

} // namespace mixxx

// End of AI-generated text.
