#pragma once

#include "preferences/configobject.h"

namespace mixxx {

namespace library {

namespace prefs {

extern const ConfigKey kLegacyDirectoryConfigKey;

extern const QString kConfigGroup;

extern const ConfigKey kRescanOnStartupConfigKey;

extern const ConfigKey kRepairDatabaseOnNextRestartConfigKey;

extern const ConfigKey kShowScanSummaryConfigKey;

extern const ConfigKey kKeyNotationConfigKey;

extern const ConfigKey kTrackDoubleClickActionConfigKey;

extern const ConfigKey kSearchDebouncingTimeoutMillisConfigKey;

extern const ConfigKey kSearchBpmFuzzyRangeConfigKey;

extern const ConfigKey kEnableSearchCompletionsConfigKey;

extern const ConfigKey kEnableSearchHistoryShortcutsConfigKey;

extern const ConfigKey kBpmColumnPrecisionConfigKey;

extern const ConfigKey kApplyPlayedTrackColorConfigKey;

extern const ConfigKey kEditMetadataSelectedClickConfigKey;

extern const ConfigKey kHistoryMinTracksToKeepConfigKey;

const int kHistoryMinTracksToKeepDefault = 1;

extern const ConfigKey kHistoryTrackDuplicateDistanceConfigKey;

const int kHistoryTrackDuplicateDistanceDefault = 6;

const bool kEditMetadataSelectedClickDefault = false;

extern const ConfigKey kSyncTrackMetadataConfigKey;

extern const ConfigKey kResetMissingTagMetadataOnImportConfigKey;

extern const ConfigKey kSyncSeratoMetadataConfigKey;

extern const ConfigKey kUseRelativePathOnExportConfigKey;

extern const ConfigKey kCoverArtFetcherQualityConfigKey;

extern const ConfigKey kTagFetcherApplyTagsConfigKey;

extern const ConfigKey kTagFetcherApplyCoverConfigKey;

extern const ConfigKey kDateFormatConfigKey;

const int kSidebarHoverExpandDelayDefault = 500; // ms

extern const ConfigKey kSidebarHoverExpandDelayConfigKey;

// AI-generated (Claude Code). Start of AI-generated text.
// Hold the mouse button on a track row for one second to open the track menu.
extern const ConfigKey kHoldToOpenMenuConfigKey;
// Show the track count in the labels of playlists and crates in the sidebar.
extern const ConfigKey kShowTrackCountInSidebarConfigKey;
const bool kShowTrackCountInSidebarDefault = false;

// Bold weight of the library font.
extern const ConfigKey kLibraryFontBoldConfigKey;
const bool kLibraryFontBoldDefault = false;

// Clear the played mark of all tracks when the program exits.
extern const ConfigKey kResetPlayedOnExitConfigKey;
const bool kResetPlayedOnExitDefault = true;

// Scroll the track list so that the selected track is in the middle.
extern const ConfigKey kCenterSelectedTrackConfigKey;
const bool kCenterSelectedTrackDefault = false;

// Show the whole cover picture in the artwork column. Off shows the upper part.
extern const ConfigKey kCoverArtShowWholeConfigKey;
const bool kCoverArtShowWholeDefault = false;

// The folder pattern of consolidate (see libraryFolderFor()). The default
// pattern is in library/consolidate/folderpattern.h.
extern const ConfigKey kConsolidateFolderPatternConfigKey;

// The search folders of Auto Relocate, as one text (see
// encodeRelocateFolders() in library/trackcollection.h). The default is no
// folder.
extern const ConfigKey kAutoRelocateFoldersConfigKey;
// End of AI-generated text.

} // namespace prefs

} // namespace library

} // namespace mixxx
