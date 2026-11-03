// AI-generated (Claude Code, 2026-10-28). Start of AI-generated text.
// Mock-engine checks for the DDJ-800 mapping script: Beat Loop pads,
// hot cues A-P on two pages, PAGE in hot cue mode, SHIFT + BROWSE zoom,
// key shift and keyboard modes, SHIFT + RELOOP/EXIT, SLIP REVERSE's 8-beat
// limit, Super Fast Search, and that every bound handler runs without an
// error.
// Run from the repository root: node tools/ddj800_mapping_test.js
// End of AI-generated text.
"use strict";
const fs = require("fs");
const vm = require("vm");

const values = {};
const sets = [];
const leds = [];
const conns = {};
const handlers = {};
const key = (g, k) => g + "," + k;

const engine = {
    getValue: (g, k) => (key(g, k) in values ? values[key(g, k)] : (k === "num_samplers" ? 16 : 0)),
    setValue: (g, k, v) => {
        values[key(g, k)] = v;
        sets.push([g, k, v]);
        (conns[key(g, k)] || []).forEach(cb => cb(v, g, k));
    },
    getParameter: () => 0,
    setParameter: (g, k, v) => sets.push([g, k, v]),
    getSetting: () => undefined,
    makeConnection: (g, k, cb) => {
        (conns[key(g, k)] = conns[key(g, k)] || []).push(cb);
        return {trigger: () => cb(engine.getValue(g, k), g, k), disconnect: () => {}};
    },
    softTakeover: () => {},
    scratchEnable: () => {}, scratchDisable: () => {}, scratchTick: () => {},
    isScratching: () => false,
    beginTimer: (ms, cb) => { timers[++timerId] = {ms, cb}; return timerId; },
    stopTimer: id => { delete timers[id]; },
};
const timers = {};
let timerId = 0;
const midi = {
    sendShortMsg: (s, n, v) => leds.push([s, n, v]),
    sendSysexMsg: () => {},
    makeInputHandler: (s, n, cb) => {
        (handlers[s * 0x80 + n] = handlers[s * 0x80 + n] || []).push(cb);
        return {disconnect: () => {}};
    },
};
const ctx = vm.createContext({engine, midi, console, print: console.log, script: {}});
const scriptPath = process.argv[2] || "res/controllers/Pioneer-DDJ-800-script.js";
vm.runInContext(fs.readFileSync(scriptPath, "utf8") + "\nthis.PioneerDDJ800 = PioneerDDJ800;", ctx);
const P = ctx.PioneerDDJ800;
values["[App],num_decks"] = 4;
P.init("DDJ-800", false);
const initialAlt = [engine.getValue("[Skin]", "left_deck_alt"), engine.getValue("[Skin]", "right_deck_alt")];

function send(status, midino, value) {
    (handlers[status * 0x80 + midino] || []).forEach(cb => cb(status & 0x0F, midino, value, status));
}
let failures = 0;
function check(name, ok) {
    console.log((ok ? "PASS " : "FAIL ") + name);
    if (!ok) failures++;
}
const setsSince = n => sets.slice(n);
const did = (from, g, k, v) => setsSince(from).some(s => s[0] === g && s[1] === k && (v === undefined || s[2] === v));

// Beat Loop mode: pad 5 (4 beats at scale 1) toggles a 4-beat loop.
let n = sets.length;
send(0x97, 0x64, 0x7F);
check("beat loop pad 5 -> beatloop_4_toggle", did(n, "[Channel1]", "beatloop_4_toggle", 1));
check("beat loop never writes beatloop_toggle", !did(n, "[Channel1]", "beatloop_toggle"));

// Every size reachable through PAGE scaling exists in the engine.
const engineSizes = [0.03125, 0.0625, 0.125, 0.25, 0.5, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512];
let allExist = true;
for (let scale = 1 / 8; scale <= 8; scale *= 2) {
    for (const s of P.beatLoopSizes) {
        if (!engineSizes.includes(s * scale)) { allExist = false; console.log("  missing size", s * scale); }
    }
}
check("all PAGE-scaled loop sizes are engine sizes", allExist);

// Hot cue mode: page 1 and page 2 pads.
send(0x90, 0x1B, 0x7F); // HOT CUE mode button
n = sets.length;
send(0x97, 0x00, 0x7F);
check("pad 1 -> hotcue_1_activate", did(n, "[Channel1]", "hotcue_1_activate", 1));
n = sets.length;
send(0x97, 0x08, 0x7F);
check("page 2 pad 1 -> hotcue_9_activate", did(n, "[Channel1]", "hotcue_9_activate", 1));
n = sets.length;
send(0x97, 0x0F, 0x7F);
check("page 2 pad 8 -> hotcue_16_activate", did(n, "[Channel1]", "hotcue_16_activate", 1));
n = sets.length;
send(0x98, 0x0A, 0x7F);
check("SHIFT + page 2 pad 3 -> hotcue_11_clear", did(n, "[Channel1]", "hotcue_11_clear", 1));
n = sets.length;
send(0x99, 0x09, 0x7F); // deck 2 pads
check("deck 2 page 2 pad 2 -> [Channel2] hotcue_10_activate", did(n, "[Channel2]", "hotcue_10_activate", 1));

// PAGE in hot cue mode changes nothing in the engine.
n = sets.length;
send(0x90, 0x2C, 0x7F);
send(0x90, 0x24, 0x7F);
const touched = setsSince(n).filter(s => s[0] === "[Channel1]");
check("PAGE in hot cue mode does not touch the deck (" + JSON.stringify(touched) + ")", touched.length === 0);

// Hot cue 12 set -> LED on page-2 note 0x0B.
let l = leds.length;
engine.setValue("[Channel1]", "hotcue_12_status", 1);
check("hotcue_12 LED on note 0x0B", leds.slice(l).some(m => m[0] === 0x97 && m[1] === 0x0B && m[2] > 0));

// Memory cue CALL buttons still call memory cues.
check("CALL buttons still bound", (handlers[0x90 * 0x80 + 0x51] || []).length > 0 && (handlers[0x90 * 0x80 + 0x53] || []).length > 0);

// SHIFT + BROWSE: right zooms in (zoom_down), left zooms out (zoom_up).
n = sets.length;
send(0xB6, 0x64, 0x01);
check("SHIFT + browse right -> waveform_zoom_down", did(n, "[Channel1]", "waveform_zoom_down", 1) && !did(n, "[Channel1]", "waveform_zoom_up"));
n = sets.length;
send(0xB6, 0x64, 0x7F);
check("SHIFT + browse left -> waveform_zoom_up", did(n, "[Channel1]", "waveform_zoom_up", 1) && !did(n, "[Channel1]", "waveform_zoom_down"));

// Key shift mode (SHIFT + SAMPLER = 0x6F): manual layout, PAGE moves 8.
send(0x90, 0x6F, 0x7F);
n = sets.length;
send(0x97, 0x70, 0x7F);
check("key shift pad 1 -> pitch +4", did(n, "[Channel1]", "pitch", 4));
n = sets.length;
send(0x97, 0x74, 0x7F);
check("key shift pad 5 -> pitch 0", did(n, "[Channel1]", "pitch", 0));
n = sets.length;
send(0x90, 0x2B, 0x7F); // PAGE left in key shift mode
check("PAGE in key shift mode does not step the key", !did(n, "[Channel1]", "pitch_down") && !did(n, "[Channel1]", "pitch_up"));
n = sets.length;
send(0x97, 0x77, 0x7F);
check("after PAGE left, pad 8 -> pitch -5", did(n, "[Channel1]", "pitch", -5));

// SHIFT + RELOOP/EXIT is not "reloop and stop".
n = sets.length;
send(0x90, 0x50, 0x7F);
check("SHIFT + RELOOP/EXIT sends no reloop_andstop", !did(n, "[Channel1]", "reloop_andstop"));

// Keyboard mode: first pad picks the hot cue (and sets it if empty).
send(0x90, 0x69, 0x7F); // SHIFT + HOT CUE = keyboard mode
n = sets.length;
send(0x97, 0x42, 0x7F);
send(0x97, 0x42, 0x00);
check("keyboard: first pad 3 sets hot cue 3, plays nothing", did(n, "[Channel1]", "hotcue_3_set", 1) && !did(n, "[Channel1]", "pitch"));
n = sets.length;
send(0x97, 0x40, 0x7F);
check("keyboard: next pad 1 plays hot cue 3 at +4", did(n, "[Channel1]", "pitch", 4) && did(n, "[Channel1]", "hotcue_3_activate", 1));
send(0x97, 0x40, 0x00);
engine.setValue("[Channel1]", "hotcue_5_status", 1);
send(0x90, 0x69, 0x7F); // pick again
n = sets.length;
send(0x97, 0x44, 0x7F);
check("keyboard: re-pick existing hot cue 5 does not overwrite it", !did(n, "[Channel1]", "hotcue_5_set"));
// AI-generated (Claude Code). Start of AI-generated text.
// A.HOT CUE off: the pads are empty (status 0) although the track has a hot
// cue there. The first pad must show the cues (hotcues_load) before it reads
// the status, else it would move the stored cue to the play position.
engine.makeConnection("[Channel1]", "hotcues_load", v => {
    if (v > 0) {
        values["[Channel1],hotcue_6_status"] = 1; // the hidden cue shows
    }
});
send(0x90, 0x69, 0x7F); // pick again
n = sets.length;
send(0x97, 0x45, 0x7F);
send(0x97, 0x45, 0x00);
const loadAt = sets.findIndex((s, i) => i >= n && s[0] === "[Channel1]" && s[1] === "hotcues_load" && s[2] === 1);
check("keyboard: hidden hot cue 6 is shown first (hotcues_load 1, 0)",
        loadAt >= 0 && did(n, "[Channel1]", "hotcues_load", 0));
check("keyboard: hidden hot cue 6 is not overwritten", !did(n, "[Channel1]", "hotcue_6_set"));
send(0x90, 0x69, 0x7F); // pick again
n = sets.length;
send(0x97, 0x46, 0x7F);
send(0x97, 0x46, 0x00);
const loadAt7 = sets.findIndex((s, i) => i >= n && s[1] === "hotcues_load" && s[2] === 1);
const setAt7 = sets.findIndex((s, i) => i >= n && s[1] === "hotcue_7_set");
check("keyboard: empty pad 7 shows the cues before it sets hot cue 7",
        loadAt7 >= 0 && setAt7 > loadAt7);
// End of AI-generated text.

// SLIP REVERSE: held -> reverseroll; ends after 8 beats; release stops the timer.
values["[Channel1],bpm"] = 120;
n = sets.length;
send(0x90, 0x15, 0x7F);
const t = Object.values(timers).pop();
check("slip reverse on, 8-beat timer = 4000 ms at 120 BPM", did(n, "[Channel1]", "reverseroll", 1) && t && Math.round(t.ms) === 4000);
n = sets.length;
if (t) { t.cb(); }
check("timer ends slip reverse while held", did(n, "[Channel1]", "reverseroll", 0));
send(0x90, 0x15, 0x00);
send(0x90, 0x15, 0x7F);
const live = Object.keys(timers).length;
send(0x90, 0x15, 0x00);
check("release stops the 8-beat timer", Object.keys(timers).length === live - 1);

// TRACK SEARCH + jog (CC 0x29): moves the play position.
values["[Channel1],duration"] = 300;
values["[Channel1],playposition"] = 0.5;
n = sets.length;
send(0xB0, 0x29, 0x40 + 54); // 54 ticks forward
const pos = values["[Channel1],playposition"];
check("super fast search moves forward (" + pos.toFixed(5) + ")", did(n, "[Channel1]", "playposition") && pos > 0.5);

// MASTER, BOOTH and HEADPHONES knobs drive Mix Two with both audio targets
// (the deck does not apply them to Mix Two's sound).
for (const target of [1, 0]) {
    values["[Master],audio_target"] = target;
    const name = target ? "HARDWARE" : "PC";
    for (const [cc, key] of [[0x08, "gain"], [0x09, "booth_gain"], [0x0C, "headMix"], [0x0D, "headGain"]]) {
        n = sets.length;
        send(0xB6, cc, 0x30); send(0xB6, cc + 0x20, 0x00);
        check(name + " target: knob B6 " + cc.toString(16) + " -> [Master] " + key, did(n, "[Master]", key));
    }
}
n = sets.length;
send(0xB6, 0x08, 0x40); send(0xB6, 0x28, 0x00);
send(0xB6, 0x0C, 0x40); send(0xB6, 0x2C, 0x00);
send(0xB6, 0x0D, 0x40); send(0xB6, 0x2D, 0x00);
check("MASTER LEVEL -> [Master] gain", did(n, "[Master]", "gain"));
check("HEADPHONES MIXING -> [Master] headMix", did(n, "[Master]", "headMix"));
check("HEADPHONES LEVEL -> [Master] headGain", did(n, "[Master]", "headGain"));

// Sound Color FX: FILTER at start; presses go to the engine's press controls.
check("no CFX selected at start (as rekordbox)", !values["[SoundColorFX],cfx4"] && !values["[SoundColorFX],cfx1"]);
n = sets.length;
send(0x96, 0x00, 0x7F); send(0x96, 0x00, 0x00);
check("DUB ECHO -> cfx1_press", did(n, "[SoundColorFX]", "cfx1_press", 1) && !did(n, "[SoundColorFX]", "cfx1_press_shift", 1));
n = sets.length;
send(0x96, 0x09, 0x7F); send(0x96, 0x09, 0x00);
check("SHIFT + SWEEP -> cfx2_press_shift", did(n, "[SoundColorFX]", "cfx2_press_shift", 1));
check("the script sets no cfx state itself on a press", !did(n, "[SoundColorFX]", "cfx2"));
check("the script loads no chain preset itself", !sets.some(x => x[1] === "loaded_chain_preset"));
l = leds.length;
engine.setValue("[SoundColorFX]", "cfx2", 1);
const cfxLedNotes = [0x00, 0x01, 0x02, 0x03, 0x08, 0x09, 0x0A, 0x0B];
check("no CFX LED output (the deck ignores it in general-MIDI mode)",
    !leds.some(m => m[0] === 0x96 && cfxLedNotes.indexOf(m[1]) >= 0));

// BEAT FX channel selector in 2-channel mixer mode: 0x1C-0x1F = one channel each.
const unitGroup = "[EffectRack1_EffectUnit1]";
const assigned = () => ["[Channel1]", "[Channel2]", "[Channel3]", "[Channel4]", "[Master]"]
    .filter(g => values[unitGroup + ",group_" + g + "_enable"] === 1);
send(0x94, 0x1C, 0x7F);
check("selector 1 (0x1C) -> channel 1 only (" + assigned() + ")", assigned().join() === "[Channel1]");
send(0x94, 0x1D, 0x7F);
check("selector 2 (0x1D) -> channel 2 only", assigned().join() === "[Channel2]");
send(0x94, 0x1E, 0x7F);
check("selector 3 (0x1E) -> channel 3 only", assigned().join() === "[Channel3]");
send(0x94, 0x14, 0x7F);
check("selector M (0x14) -> master only", assigned().join() === "[Master]");

// BEAT FX: LEVEL/DEPTH -> the slot's meta knob; BEAT > steps the beat
// fraction (parameter1) by one of nine steps; SHIFT + BEAT < resets it.
const fxSlot = "[EffectRack1_EffectUnit1_Effect1]";
n = sets.length;
send(0xB4, 0x02, 0x40); send(0xB4, 0x22, 0x00);
check("LEVEL/DEPTH -> slot meta", did(n, fxSlot, "meta"));
check("LEVEL/DEPTH -> depth (parameter2) directly, past soft takeover", did(n, fxSlot, "parameter2"));
n = sets.length;
engine.setValue(fxSlot, "loaded_effect", 3); // FX SELECT loads a new effect
check("a new effect takes the knob's position", did(n, fxSlot, "parameter2"));
n = sets.length;
send(0x94, 0x4B, 0x7F);
check("BEAT > -> parameter1 + 1/8", did(n, fxSlot, "parameter1", 0.125));
send(0x94, 0x66, 0x7F);
check("SHIFT + BEAT < -> parameter1 = 0.5 (1 beat)", did(n, fxSlot, "parameter1", 0.5));

// AI-generated (Claude Code). Start of AI-generated text.
// The deck shown on each side follows the deck that sends messages.
check("init: both deck sides are 0", initialAlt[0] === 0 && initialAlt[1] === 0);
n = sets.length;
send(0x92, 0x0B, 0x7F);
check("deck 3 message -> left_deck_alt 1", values["[Skin],left_deck_alt"] === 1 && did(n, "[Skin]", "left_deck_alt", 1));
check("deck 3 message leaves the right side", !did(n, "[Skin]", "right_deck_alt"));
n = sets.length;
send(0x92, 0x0C, 0x7F);
check("same side again writes nothing", !did(n, "[Skin]", "left_deck_alt"));
send(0x90, 0x0B, 0x7F);
check("deck 1 message -> left_deck_alt 0", values["[Skin],left_deck_alt"] === 0);
send(0x93, 0x0B, 0x7F);
check("deck 4 message -> right_deck_alt 1", values["[Skin],right_deck_alt"] === 1);
send(0x91, 0x0B, 0x7F);
check("deck 2 message -> right_deck_alt 0", values["[Skin],right_deck_alt"] === 0);
send(0x90, 0x56, 0x7F);
check("note 0x56 on channel 1 -> left_deck_alt 1", values["[Skin],left_deck_alt"] === 1);
send(0x92, 0x55, 0x7F);
check("note 0x55 on channel 3 -> left_deck_alt 0", values["[Skin],left_deck_alt"] === 0);
send(0x91, 0x56, 0x7F);
check("note 0x56 on channel 2 -> right_deck_alt 1", values["[Skin],right_deck_alt"] === 1);
send(0x93, 0x55, 0x7F);
check("note 0x55 on channel 4 -> right_deck_alt 0", values["[Skin],right_deck_alt"] === 0);
send(0x92, 0x57, 0x7F);
send(0x93, 0x46, 0x7F);
check("LINE/PHONO notes do not switch a side", values["[Skin],left_deck_alt"] === 0 && values["[Skin],right_deck_alt"] === 0);
send(0x90, 0x56, 0x7F);
send(0x91, 0x56, 0x7F);
send(0xB2, 0x13, 0x00);
check("deck 3 CC message keeps left_deck_alt 1", values["[Skin],left_deck_alt"] === 1);
P.shutdown();
check("shutdown leaves the sides and clears ddj800_connected",
        values["[Skin],left_deck_alt"] === 1 && values["[Skin],right_deck_alt"] === 1 && values["[Skin],ddj800_connected"] === 0);
// Fewer than 3 decks: the sides stay 0.
values["[App],num_decks"] = 2;
values["[Skin],left_deck_alt"] = 1;
for (const k of Object.keys(handlers)) delete handlers[k];
P.init("DDJ-800", false);
send(0x91, 0x0B, 0x7F);
check("2 decks: sides stay 0", values["[Skin],left_deck_alt"] === 0 && values["[Skin],right_deck_alt"] === 0);
values["[App],num_decks"] = 4;
for (const k of Object.keys(handlers)) delete handlers[k];
P.init("DDJ-800", false);
// End of AI-generated text.

// Every bound handler runs without throwing (press and release).
let thrown = 0;
for (const k of Object.keys(handlers)) {
    const status = Math.floor(k / 0x80), midino = k % 0x80;
    for (const v of [0x7F, 0x00, 0x41, 0x3F]) {
        try { send(status, midino, v); } catch (e) { thrown++; if (thrown < 5) console.log("  threw", status.toString(16), midino.toString(16), e.message); }
    }
}
check("no handler throws (" + Object.keys(handlers).length + " bindings)", thrown === 0);
try { P.shutdown(); check("shutdown runs", true); } catch (e) { check("shutdown runs: " + e.message, false); }

console.log(failures ? failures + " FAILED" : "ALL PASSED");
process.exit(failures ? 1 : 0);
