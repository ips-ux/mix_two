<!-- AI-generated (Claude Code, 2026-10-28). Start of AI-generated text. -->

# Bucket 15: Streaming

> **AI-generated:** built from the 2026-10-28 inventories and the owner's
> decisions in [DECISIONS.md](../DECISIONS.md). Status ladder and rules: DECISIONS.md §3.

**Criticality:** Low (far future)  ·  **Scope:** live streaming of the mix. Base: Mixxx's Live Broadcasting (src/broadcast/, libshout); later a self-hosted stream that listeners open in a browser (DEC-§8). Music streaming services (Beatport, TIDAL and others) are not here; they are Skip in the library bucket.

## Live Broadcasting (Mixxx base)
| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| STR-001 | Live Broadcasting to Icecast / Shoutcast | Improve | N? | N? | N? | Built | src/broadcast/broadcastmanager.cpp; src/engine/sidechain/shoutconnection.cpp; CMakeLists.txt BROADCAST option (default ON) | Mix Two extra, kept (PARITY L10). Owner's reason: DEC-§8 streaming goal. No Icecast output found in the rekordbox manuals (unverified). | PAR-L10 |
| STR-002 | Broadcast preferences (servers, mounts, encoder, several connections) | Improve | — | — | — | Built | src/preferences/dialog/dlgprefbroadcast.cpp; src/preferences/broadcastprofile.cpp; src/test/broadcastprofile_test.cpp | Upstream. Text to STE when the page is reworked (STE-004). Passwords use the system keychain when built with QtKeychain. | DEC-§8 |
| STR-003 | Now-playing metadata sent to the server | Improve | — | — | — | Built | src/engine/sidechain/shoutconnection.cpp | Sends artist and title. Must be listed in the privacy policy (STE-017). | DEC-§6 |
| STR-004 | Broadcasting on Linux | Improve | — | — | — | Build | CMakeLists.txt (libshout-idjc, version check) | Not built or tested on Linux yet. Check the .deb and AppImage include libshout-idjc (PORT-008, PORT-009). | DEC-§8 |
| STR-005 | DDJ-800 microphone in the stream | DDJ-800 | — | — | — | Build | MIX microphone rows; manual p78 | The DDJ-800 mixes its mic in hardware, so the mic does not reach Mix Two's broadcast or recording. Needs the "via PC" mic path (MIX) or a separate input. | DEC-§8 |
| STR-006 | Recording | rb6 | Y | Y | Y | Build | src/library/recording | See PERF (recording owns the record source and settings); shares the sidechain with broadcasting. | DEC-§8 |

## Self-hosted streaming (far future)
| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| STR-007 | Self-hosted browser stream | Improve | — | — | — | Build | DEC-§8 | Mix Two (or a small server it starts) serves the mix to listeners in a web browser, with no third-party platform. First step: document a self-hosted Icecast setup on Linux that STR-001 already feeds. | DEC-§8 |
| STR-008 | Video or camera streaming | Owner? | — | — | — | Build | none | Not in DECISIONS. See question 1. | DEC-§8 |

## Open questions for the owner
1. **What "live streaming" covers.** Options: (a) audio only (Icecast, with a browser player page); (b) audio plus a camera or screen picture; (c) audio sent to a video platform through other software (for example OBS). Recommendation: (a) in Mix Two; (c) for video, so Mix Two does not grow a video stack.
2. **Self-hosting form.** Options: (a) document a separate Icecast server the DJ runs; (b) build a small server into Mix Two. Recommendation: (a) first; it works today with STR-001.

<!-- End of AI-generated text. -->
