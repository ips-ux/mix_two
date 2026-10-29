<!-- AI-generated (Claude Code, 2026-10-28). Start of AI-generated text. -->

# Bucket 13: STE-Document Handling

> **AI-generated:** built from the 2026-10-28 inventories and the owner's
> decisions in [DECISIONS.md](../DECISIONS.md). Status ladder and rules: DECISIONS.md §3.

**Criticality:** Low, ongoing  ·  **Scope:** all text that Mix Two writes for a person (tooltips, notifications, dialogs, prompts, errors, first-run setup, user docs, USER_changelog) in ASD-STE100, with Pioneer and industry terms kept as printed (DEC-§7); and the privacy policy that lists every outgoing data flow (DEC-§6). The functions behind the text live in other buckets.

STE rules in use (from the USER_changelog.md header): one topic per paragraph; one instruction per sentence; instructions 20 words or fewer, imperative; descriptions 25 words or fewer, active voice; approved words ("push" not "press", "show" not "display", "make sure" not "ensure", "start" not "launch"); technical names as on the screen or the DDJ-800; a CAUTION comes before the step it is about.

## Text rule and tooling
| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| STE-001 | ASD-STE100 rule for everything we write | Improve | — | — | — | Partial | DEC-§7; USER_changelog.md header comment | The rule is written only in the USER_changelog header. Add a pointer in AGENT_changelog "Working agreements" so every agent applies it to tooltips, dialogs, errors and docs. Owner's reason: clear, short text for DJs under stress. | DEC-§7 |
| STE-002 | STE word check script | Improve | — | — | — | Build | none | A small script (for example tools/ste_check.py) that flags unapproved words and long sentences in tr() strings, .ui files, mapping XML descriptions and the user docs. Advisory only; Pioneer terms on an allow list. | DEC-§7 |
| STE-003 | Term list: Pioneer and rekordbox names as printed | rb6 | — | Y | — | Build | DEC-§1, DEC-§4, DEC-§7 | One list of names to keep exactly: SYNC, MASTER, KEY SYNC, BEAT FX, SOUND COLOR FX (SWEEP, not PITCH), Device Library Plus (not OneLibrary), My Settings, Sync Manager, Collection, Tag List, My Tag. rb6 wording wins where rb6 and the manuals differ. | DEC-§7 |

## In-app text
| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| STE-004 | Audit of the strings Mix Two added | Improve | — | — | — | Build | git diff ee17a27266 HEAD: src/coreservices.cpp (rekordbox-running dialog), src/library/loadprompt.cpp ("Load to the Deck"), src/preferences/dialog/dlgprefdeckdlg.ui (Sync options, Platter options), src/controllers/dlgprefcontroller.cpp | Not audited. Example to check: "rekordbox is open. While it runs it holds the DJ controller, ..." (coreservices.cpp). | DEC-§7 |
| STE-005 | DDJ-800 mapping option labels and descriptions | Improve | — | — | — | Build | res/controllers/Pioneer-DDJ-800.midi.xml:17-63 | Plain English, not STE (for example "Touching the top of the jog wheel scratches."). Rewrite; keep the labels the same as the USER_changelog list. | DEC-§7 |
| STE-006 | Tooltips for Mix Two controls | rb6 | — | Y | — | Partial | src/skin/legacy/tooltips.cpp (+38 lines since ee17a27266); LateNight deck hub, SYNC, KEY SYNC, MT, BOOTH knob | Some tooltips added; not checked against STE or against rb6's tooltip text (rb6 `Tip` strings are a model for content, not wording). | DEC-§7 |
| STE-007 | Tooltips on/off setting | rb6 | p.177 | p.194 | p.219 | Built | upstream Mixxx tooltip preference | See PREP for the preference itself if PREP lists it; text rule only here. | B-011, PAR-A13 |
| STE-008 | Notifications and error messages | Improve | — | — | — | Build | controller open retry, hotplug, later export and backup errors | One STE pattern: what happened, then what to do (one instruction per sentence). | DEC-§7 |
| STE-009 | First-run setup wording | Improve | — | — | — | Build | DEC-§6 | Text for: Local or Portable library (and "not the USB performance stick"); import in place; Consolidate and Back up pre-selected and marked Recommended; "Continue without a backup"; the quiet "No backup" status. No repeated pop-ups. Function in LIB. | DEC-§6, DEC-§7 |
| STE-010 | Library action wording | Improve | — | — | — | Build | DEC-§6 | Fix tags list ("Skip for now", "No artist needed"); move-in question with "Do this for all tracks"; delete dialog ("Delete from library?", "Delete from backup?", "Are you sure?"); Restore from backup; rekordbox master.db update and Auto Relocate guide. Function in LIB / IMP. | DEC-§6 |
| STE-011 | One-time privacy notice before the first online lookup | Improve | — | — | — | Build | DEC-§6 | Shows once, before the first AcoustID / MusicBrainz lookup; names what is sent and links to the privacy policy (STE-013). | DEC-§6 |
| STE-012 | New strings and translations | Owner? | Y | Y | Y | Build | upstream Mixxx translations (res/translations) | New Mix Two strings have no translations. See question 2. | DEC-§7 |

## Privacy policy
| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| STE-013 | Privacy policy document | Improve | — | — | — | Build | DEC-§6 ("to be written") | One STE document listing every outgoing data flow (STE-014 to STE-020), what is sent, to whom, and when. Link it from the privacy notice, the Help menu and the user docs. Mix Two sends no telemetry or log uploads (Improve); the policy says so. | DEC-§6, PAR-N2 |
| STE-014 | Flow: AcoustID lookup | Improve | — | — | — | Build | src/musicbrainz/web/acoustidlookuptask.cpp; src/musicbrainz/chromaprinter.cpp | Sends an audio fingerprint and the duration to the AcoustID service. Starts only from a user action or the Fix tags auto-lookup (DEC-§6). Policy entry to write. | DEC-§6 |
| STE-015 | Flow: MusicBrainz lookup | Improve | — | — | — | Build | src/musicbrainz/web/musicbrainzrecordingstask.cpp | Sends recording IDs to MusicBrainz; receives tags. Policy entry to write. | DEC-§6 |
| STE-016 | Flow: Cover Art Archive | Improve | — | — | — | Build | src/musicbrainz/web/coverartarchivelinkstask.cpp, coverartarchiveimagetask.cpp | Sends release IDs, receives cover images. Part of the tag fetcher. Policy entry to write. | DEC-§6 |
| STE-017 | Flow: Live Broadcasting | Improve | — | — | — | Build | src/engine/sidechain/shoutconnection.cpp; src/preferences/broadcastprofile.cpp | Sends the audio, the track metadata (artist / title) and the login to the server the DJ sets. Only when the DJ starts it. Policy entry to write (see STR-003). | DEC-§6 |
| STE-018 | Flow: cloud-folder backup | Improve | — | — | — | Build | DEC-§6 | Mix Two copies files to a local folder; the Dropbox / Google Drive client (or rclone on Linux, PORT-021) uploads them. Mix Two itself sends nothing. Say so. | DEC-§6 |
| STE-019 | Statement: no update check, no telemetry, no account | Improve | — | — | — | Build | grep 2026-10-28: network code only in src/musicbrainz/web/, src/network/, shoutconnection.cpp | No update check or telemetry found in src. rekordbox accounts, KUVO, cloud sync and Radar are Skip in other buckets. Re-check this statement at each release. | DEC-§6 |
| STE-020 | Statement: local data that Mix Two reads and writes | Improve | — | — | — | Build | DEC-§5, DEC-§6 | Reads rekordbox master.db, XML and USB sticks; writes tags only for managed-library files; writes master.db only with consent and a backup; never writes to a stick that is only played. | DEC-§5, DEC-§6 |

## Changelogs and user docs
| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| STE-021 | USER_changelog upkeep | Improve | — | — | — | Partial | USER_changelog.md; AGENT_changelog.md:20-21 ("Mirror user-visible changes") | Exists and follows STE. Stale items: TRU-056 (jog 720), TRU-057 (MASTER / BOOTH LEVEL). Add each user-visible change in the same commit. | DEC-§7 |
| STE-022 | AI disclaimer in text blocks | Improve | — | — | — | Built | AGENTS.md "AI-Generated Text Must Carry a Disclaimer"; USER_changelog start and end markers | Keep start and end markers on every AI-written doc, comment and commit body. | AGENTS.md |
| STE-023 | DDJ-800 user guide | DDJ-800 | — | — | — | Build | XML `<manual>pioneer_ddj_800</manual>` | Points to an upstream Mixxx manual page that does not exist for Mix Two. Write a Mix Two DDJ-800 page in STE (controls, SHIFT layer, pad modes, settings) and point the manual link to it. | DEC-§7 |
| STE-024 | Getting started guide | Improve | — | — | — | Partial | USER_changelog.md "Before you start" | Covers only Windows (tools\mixtwo_windows.bat, quit rekordbox, Pioneer driver). Add Linux install (.deb, AppImage, udev, real-time), first-run setup and troubleshooting. See PORT-001. | DEC-§7, DEC-§8 |
| STE-025 | Help menu: manual link | rb6 | Y | Y | Y | Build | src/widget/wmainmenubar.cpp | See PREP (menus own B-007); here only the target: Mix Two docs, not the upstream manual. | DEC-§7 |

## Open questions for the owner
1. **Upstream Mixxx text.** Most screens still show Mixxx's own strings. Options: (a) rewrite only the text Mix Two adds or changes; (b) also rewrite upstream strings on screens a DJ uses (Prepare, Performance, Preferences pages we keep); (c) rewrite all. Recommendation: (b), page by page as each screen is rebuilt; it keeps the rewrite small and visible.
2. **Translations.** Options: (a) English only for new text, keep upstream translations for old text; (b) drop the language setting until the text is stable; (c) translate new text later with STE as the source. Recommendation: (a) now, (c) later; STE text translates well.
3. **Where the privacy policy lives.** Options: (a) a docs file only; (b) a docs file plus Help menu and the one-time notice link to it. Recommendation: (b), one source file.
4. **STE check strictness.** Options: (a) advisory report only; (b) a pre-commit check that fails on unapproved words. Recommendation: (a): a hard check would block Pioneer and Mixxx terms too often.

<!-- End of AI-generated text. -->
