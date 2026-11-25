#pragma once
// AI-generated (Claude Code). Start of AI-generated text.

#include <QString>
#include <QStringList>

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

/// The real-time priority that the audio code asks for (the value that
/// PortAudio uses). The limits file for the audio group allows it.
constexpr unsigned int kRequiredRtPrio = 82;

/// True when the real-time priority limit blocks the audio code. `cur` is the
/// soft limit and `max` is the hard limit of the process. The soft limit
/// decides, as `RLimit::isRtPrioAllowed()` does; Mix Two does not raise the
/// soft limit itself, so the hard limit does not change the answer.
bool rtNoticeNeeded(unsigned int cur, unsigned int max);

/// The decision "show a notice": the condition holds and the user has not
/// seen the notice in this profile before.
bool shouldShowNotice(bool conditionHolds, bool alreadyShown);

/// The kernel release of this computer, as `uname -r` gives it. An empty
/// string when the system does not give it.
QString currentKernelRelease();

/// True when the ALSA sequencer snapshot lists a DDJ-800 MIDI port or
/// `/proc/asound/cards` lists a DDJ-800 sound card.
bool isDdj800Connected(const QStringList& midiPortNames, const QString& cardsText);

/// Read the available ALSA sources on Linux. Other systems return false.
bool isDdj800Connected();

} // namespace mixxx

// End of AI-generated text.
