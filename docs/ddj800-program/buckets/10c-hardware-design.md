<!-- AI-generated (Claude Code, 2026-10-28). Start of AI-generated text. -->

# Bucket 10c: Hardware Design + skin system

> **AI-generated:** built from the 2026-10-28 inventories and the owner's
> decisions in [DECISIONS.md](../DECISIONS.md). Status ladder and rules: DECISIONS.md §3.

**Criticality:** Medium, rising  ·  **Scope:** the Hardware Design direction for performance mode (DEC-§7): a real skin system, the skin that follows the connected gear, the device-to-feature capability map, the feature ownership display, the DDJ-800 skin (first round) and the later CDJ-3000 + DJM-900NXS2 skin. The current Legacy Design is bucket 10b (LEG); what the controller sends and receives is bucket 01 (CTL) and bucket 11 (HWL).

No inventory row describes this work: rekordbox has one fixed layout per mode and no device skins, so the rows come from DECISIONS.md §7 (cited `DEC-§7`) and DESIGN.md (`DES-<part>`). rb5 / rb6 / rb7 columns are `N` where rekordbox has nothing comparable.

## Skin system

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| HWD-001 | Hardware Design direction: performance mode moves towards hardware skeuomorphism (concept art direction) | Improve | N | N | N | Build | DEC-§7 | Owner's direction. Prepare / Export stays a 1:1 rekordbox replica (PREP); only performance mode gets hardware skins. Concept art is visual inspiration, not structure (as DEC-§6 says for the sidebar). | DEC-§7 |
| HWD-002 | Real skin system: several performance skins over one engine, each with its own layout | Improve | N | ≠ (Dark / Light only) | ≠ | Partial | Mixxx loads skins from `res/skins/` (LateNight, Deere, Tango, Shade) | Mixxx can switch whole skins, but a switch reloads the UI and there is no device-driven choice. Skins must share the engine's controls only (FUN "engine owns behaviour"). | DEC-§7 |
| HWD-003 | The DJ can switch skins at any time | Improve | N | N | N | Partial | Mixxx Preferences > Interface > Skin | Exists as Mixxx's skin preference; needs a quick switch in performance mode and the skins below. | DEC-§7 |
| HWD-004 | Plugging in a DDJ-800 shows the DDJ-800 skin | Improve | N | N | N | Build | controller auto-enable + hotplug exist (AGENT_changelog "Controllers") | Hook the skin choice to controller detection. Whether a later manual choice overrides auto-switch: open question 1. | DEC-§7 |
| HWD-005 | No gear plugged in: show the Legacy Design or the DJ's last choice | Improve | N | N | N | Build | — | | DEC-§7 |
| HWD-006 | Layout customisation and view presets carry over to hardware skins | Improve | N | N | N | Build | — | Cross-reference: section show / hide and arrangement presets are LEG-060 / LEG-061; hardware skins reuse the same per-section visibility controls. | see LEG-060, LEG-061 |

## Capability map and feature ownership

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| HWD-007 | Device-to-feature capability map (which features each piece of gear has) | Improve | N | N | N | Build | — | Needed by HWD-008 and HWD-009. First entries: DDJ-800 (from inventory A), CDJ-3000 and DJM-900NXS2 (from inventory D). Format and source: open question 2. | DEC-§7 |
| HWD-008 | Feature ownership setting: Highlight owned / Desaturate unowned / Hide unowned / No change (default) | Improve | N | N | N | Build | — | Marks the connected gear's features against the skin's gear. Default **No change**. | DEC-§7 |
| HWD-009 | Unowned features are dimmed, never disabled | Improve | N | N | N | Build | — | Rule for every mode of HWD-008: a dimmed control still works from the screen. | DEC-§7 |

## Device skins

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| HWD-010 | DDJ-800 skin (first round, with the Legacy Design) | DDJ-800 | N | N | N | Build | — | Lay out the screen like the DDJ-800: its sections, order and printed wording (DESIGN.md "hardware comes first", moved here from the Legacy Design by DEC-§7). Uses the DESIGN.md module look (raised titled modules, domed round buttons, colour language). | DEC-§7, DES-Direction |
| HWD-011 | DDJ-800 skin follows the input selectors (decks 1 / 3 left, 2 / 4 right) | DDJ-800 | N | N | N | Build | — | Cross-reference: the deck-switch signal and soft takeover are CTL (inventory A P-003); the skin only shows which deck each side controls. | see CTL |
| HWD-012 | DDJ-800 skin: SOUND COLOR FX buttons DUB ECHO / SWEEP / NOISE / FILTER and the BEAT FX section as on the unit | DDJ-800 | N | N | N | Build | — | Labels per DEC-§1 (SWEEP, not PITCH). Effects themselves: FX bucket. | DEC-§1, DEC-§7 |
| HWD-013 | CDJ-3000 + DJM-900NXS2 skin | CDJ-3000 | N | N | N | Build | — | Deferred: waits for borrowed or donated gear (DEC-§7). Uses the capability map rows for both units. | DEC-§7 |

## Open questions for the owner

1. **Auto-switch after a manual choice (HWD-004).** If the DJ picks the Legacy Design while a DDJ-800 is connected, should the next plug-in switch to the DDJ-800 skin again? Options: (a) always switch on plug-in; (b) remember the DJ's choice per device. *Recommendation:* (b).
2. **Capability map format (HWD-007).** Options: (a) a hand-written data file per device, shipped with the mapping, listing feature IDs that match bucket row IDs; (b) derive owned features from the controller mapping file. *Recommendation:* (a); the mapping does not know about features the gear has but Mix Two does not map.
3. **Granularity of the ownership display (HWD-008).** Mark whole sections (FX unit, sampler, mixer strip) or single controls? *Recommendation:* whole sections first, single controls later.

<!-- End of AI-generated text. -->
