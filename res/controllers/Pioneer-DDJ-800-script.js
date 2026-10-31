// Pioneer-DDJ-800-script.js
// ****************************************************************************
// * Mixxx mapping script file for the Pioneer DDJ-800.
// *
// * AI-generated (Claude Code). Derived from the MIDI message table that
// * rekordbox ships for this controller (MidiMappings/DDJ-800.midi.csv) and
// * from the DDJ-400/FLX4 mappings for shared Pioneer conventions.
// * Not yet tested on hardware; see "Needs hardware verification" below.
// * End of AI-generated notice.
// ****************************************************************************
//
//  MIDI layout (from the rekordbox table):
//      0x90-0x93 / 0xB0-0xB3   Deck 1-4 buttons / knobs, jog wheels
//      0x94 / 0xB4             Beat FX
//      0x96 / 0xB6             Browser, crossfader, CFX, sampler volume, mic
//      0x97,0x99,0x9B,0x9D     Performance pads deck 1-4
//      0x98,0x9A,0x9C,0x9E     Performance pads deck 1-4 with SHIFT
//      0x9F / 0xBF             Controller state / settings (output)
//      Pad note = (pad mode - 1) * 0x10 + pad index (hot cue page 2: + 0x08)
//      High resolution knobs send MSB on CC n and LSB on CC n + 0x20.
//
//  Implemented:
//      * Browser: scroll, load, instant double (double press LOAD), zoom,
//        BROWSE push on a track opens the "Load to the Deck" prompt
//      * Deck: play, cue, sync/leader, master tempo, tempo range, tempo
//        slider, slip, reverse, slip reverse, vinyl mode, quantize,
//        track search, key sync/reset
//      * Jog wheels: scratch, pitch bend, search, loop in/out adjust,
//        beat grid slide/adjust while the GRID buttons are held
//      * Loop section: in/out, 4 beat (long press), reloop/exit,
//        1/2x / 2x when a loop is active, memory cue call prev/next
//        otherwise
//      * Memory cues: MEMORY stores a memory cue (or the active loop as a
//        memory loop), DELETE removes the one just stored or called
//      * Mixer: gain, 3-band EQ, channel faders, crossfader, headphone
//        cue, master cue, VU meters, fader start
//      * Sound Color FX: CFX buttons set [SoundColorFX],cfx1..4 (the engine
//        loads each button's preset), COLOR knobs drive the super knob
//      * Beat FX: effect selector, channel assign, depth, on/off
//      * Pad modes: Hot Cue, Pad FX1 (rolls), Beat Jump, Sampler,
//        Keyboard, Pad FX2 (momentary effect units), Beat Loop, Key Shift
//
//  Needs hardware verification:
//      * Jog wheel resolution: 6480 ticks per turn, measured from a USB
//        capture of the deck in rekordbox (rekordbox's own constant too);
//        check that one turn moves 1.8 s in Mix Two
//      * Pad LED velocities (single colour vs. palette)
//      * No keep-alive SysEx needed (firmware: the knob, jog and button
//        messages do not depend on it)
//      * Meaning of the "Permission_PadMode" page flags
//      * Jog LCD (sent over the vendor HID interface, not MIDI)

var PioneerDDJ800 = {
    // Defaults so that shutdown() is safe even if init() did not run.
    inputHandlers: [],
    connections: [],
    usedLeds: {},
    decks: [],
    numDecks: 0,
};

PioneerDDJ800.STATUS = {
    deckNote: 0x90,
    deckCC: 0xB0,
    fxNote: 0x94,
    fxCC: 0xB4,
    globalNote: 0x96,
    globalCC: 0xB6,
    padNote: 0x97,
    stateNote: 0x9F,
};

PioneerDDJ800.LED_ON = 0x7F;
PioneerDDJ800.LED_OFF = 0x00;

PioneerDDJ800.PAD_MODE = {
    hotCue: 0,
    padFx1: 1,
    beatJump: 2,
    sampler: 3,
    keyboard: 4,
    padFx2: 5,
    beatLoop: 6,
    keyShift: 7,
};

// Deck-channel notes of the pad mode buttons, indexed by PAD_MODE.
PioneerDDJ800.padModeButtons = [0x1B, 0x1E, 0x20, 0x22, 0x69, 0x6B, 0x6D, 0x6F];

// Deck-channel notes of the PARAMETER left/right buttons, indexed by PAD_MODE.
PioneerDDJ800.pageLeftButtons = [0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B];
PioneerDDJ800.pageRightButtons = [0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33];
// PARAMETER left/right with SHIFT (sampler bank change in every pad mode).
PioneerDDJ800.shiftPageLeftButtons = [0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08];
PioneerDDJ800.shiftPageRightButtons = [0x09, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x00];

// Output notes the controller expects before it allows page changes.
PioneerDDJ800.pagePermissionNotes = [0x21, 0x63, 0x6A, 0x6C, 0x6E, 0x74, 0x75, 0x76];

// Hot cues A-P: two pages of 8 pads.
PioneerDDJ800.numHotCues = 16;

PioneerDDJ800.beatJumpSizes = [1, 1, 2, 2, 4, 4, 8, 8];
PioneerDDJ800.beatLoopSizes = [0.25, 0.5, 1, 2, 4, 8, 16, 32];
PioneerDDJ800.rollSizes = [0.0625, 0.125, 0.25, 0.5, 1, 2, 4, 8];
// Keyboard and key shift share the manual's layout (p.53, p.55): top row
// +4..+7, bottom row 0..+3; PAGE moves it by 8 semitones.
PioneerDDJ800.keyboardSemitones = [4, 5, 6, 7, 0, 1, 2, 3];
PioneerDDJ800.keyShiftSemitones = PioneerDDJ800.keyboardSemitones;

// Beat FX channel selector positions (FX channel 0x94).
PioneerDDJ800.fxAssignTargets = {
    0x10: ["[Channel1]"],
    0x11: ["[Channel2]"],
    0x12: ["[Channel3]"],
    0x13: ["[Channel4]"],
    0x14: ["[Master]"],
    0x16: ["[Sampler]"],
    0x1C: ["[Channel1]", "[Channel3]"],
    0x1D: ["[Channel2]", "[Channel4]"],
};

PioneerDDJ800.beatFxUnit = "[EffectRack1_EffectUnit1]";
PioneerDDJ800.beatFxSlot = "[EffectRack1_EffectUnit1_Effect1]";
PioneerDDJ800.padFx2Units = ["[EffectRack1_EffectUnit2]", "[EffectRack1_EffectUnit3]"];

PioneerDDJ800.instantDoubleWindowMs = 300;
// [Library],focused_widget value of the track table (FocusWidget::TracksTable).
PioneerDDJ800.FOCUS_TRACKS_TABLE = 3;
PioneerDDJ800.loopAdjustMultiply = 50;
PioneerDDJ800.gridAdjustTicks = 8;
// GRID SLIDE / SHIFT + jog: fine enough to place the grid on a kick when
// zoomed in, yet a quarter turn still moves it about 80 ms.
PioneerDDJ800.gridSlideMsPerTick = 0.05;
// Super Fast Search: track time moved by one jog turn. rekordbox's value is
// not known; a guess to tune on the deck.
PioneerDDJ800.superFastSearchSecondsPerTurn = 30;
// SLIP REVERSE ends by itself after this many beats (manual p.21).
PioneerDDJ800.slipReverseBeats = 8;

PioneerDDJ800.getSetting = function(name, fallback) {
    const value = engine.getSetting(name);
    return value === undefined ? fallback : value;
};

PioneerDDJ800.deckGroup = function(deck) {
    return "[Channel" + (deck + 1) + "]";
};

PioneerDDJ800.bind = function(status, midino, callback) {
    PioneerDDJ800.inputHandlers.push(midi.makeInputHandler(status, midino, callback));
};

PioneerDDJ800.bindButton = function(status, midino, onPress) {
    PioneerDDJ800.bind(status, midino, function(_channel, _control, value) {
        if (value > 0) {
            onPress();
        }
    });
};

// Combines a 14 bit MSB/LSB pair and calls onValue with 0..1.
PioneerDDJ800.bindHiRes = function(status, midinoMSB, onValue) {
    let msb = 0;
    PioneerDDJ800.bind(status, midinoMSB, function(_channel, _control, value) {
        msb = value;
    });
    PioneerDDJ800.bind(status, midinoMSB + 0x20, function(_channel, _control, value) {
        onValue(((msb << 7) | value) / 0x3FFF);
    });
};

PioneerDDJ800.setLed = function(status, midino, on) {
    midi.sendShortMsg(status, midino, on ? PioneerDDJ800.LED_ON : PioneerDDJ800.LED_OFF);
    PioneerDDJ800.usedLeds[status * 0x80 + midino] = true;
};

PioneerDDJ800.connect = function(group, key, callback) {
    const connection = engine.makeConnection(group, key, callback);
    if (connection) {
        connection.trigger();
        PioneerDDJ800.connections.push(connection);
    }
    return connection;
};

PioneerDDJ800.connectLed = function(group, key, status, midino) {
    PioneerDDJ800.connect(group, key, function(value) {
        PioneerDDJ800.setLed(status, midino, value > 0);
    });
};

PioneerDDJ800.toggle = function(group, key) {
    engine.setValue(group, key, !engine.getValue(group, key));
};

PioneerDDJ800.relative = function(value) {
    return value < 0x40 ? value : value - 0x80;
};

//
// Init / shutdown
//

PioneerDDJ800.init = function() {
    PioneerDDJ800.inputHandlers = [];
    PioneerDDJ800.connections = [];
    PioneerDDJ800.usedLeds = {};

    PioneerDDJ800.vinylModeDefault = PioneerDDJ800.getSetting("vinylModeDefault", true);
    PioneerDDJ800.jogResolution = PioneerDDJ800.getSetting("jogResolution", 6480);
    PioneerDDJ800.inputSelectPassthrough = PioneerDDJ800.getSetting("inputSelectPassthrough", false);

    PioneerDDJ800.numDecks = Math.min(4, engine.getValue("[App]", "num_decks"));
    PioneerDDJ800.decks = [];
    for (let deck = 0; deck < PioneerDDJ800.numDecks; deck++) {
        PioneerDDJ800.decks.push({
            shift: false,
            vinylMode: PioneerDDJ800.vinylModeDefault,
            padMode: PioneerDDJ800.PAD_MODE.hotCue,
            beatJumpScale: 1,
            beatLoopScale: 1,
            rollScale: 1,
            samplerBank: 0,
            keyboardBase: 0,
            keyShiftBase: 0,
            // Hot cue keyboard mode plays; 0 until the DJ picks one with a pad.
            keyboardCue: 0,
            loopAdjustIn: false,
            loopAdjustOut: false,
            gridAdjust: false,
            gridSlide: false,
            gridTicks: 0,
            lastLoadPress: 0,
            slipReverseTimer: 0,
            padFx2Held: {},
        });
    }

    PioneerDDJ800.initBrowser();
    PioneerDDJ800.initMixer();
    PioneerDDJ800.initCfx();
    PioneerDDJ800.initBeatFx();
    for (let deck = 0; deck < PioneerDDJ800.numDecks; deck++) {
        PioneerDDJ800.initDeck(deck);
        PioneerDDJ800.initJog(deck);
        PioneerDDJ800.initPads(deck);
        for (const note of PioneerDDJ800.pagePermissionNotes) {
            PioneerDDJ800.setLed(PioneerDDJ800.STATUS.deckNote + deck, note, true);
        }
        PioneerDDJ800.updateVinylLed(deck);
        PioneerDDJ800.selectPadMode(deck, PioneerDDJ800.PAD_MODE.hotCue);
    }

    // Ask the deck for the positions of its knobs and faders, as rekordbox
    // does at connect. It never sends them by itself, and the reply leaves
    // out MASTER, BOOTH and the headphone knobs.
    midi.sendSysexMsg(PioneerDDJ800.SYSEX_SEND_ALL, PioneerDDJ800.SYSEX_SEND_ALL.length);
};

// DDJ-800 host command 0x03 (traced in its firmware; seen in a USB capture
// of rekordbox): resend every knob, fader and switch position.
PioneerDDJ800.SYSEX_SEND_ALL = [0xF0, 0x00, 0x40, 0x05, 0x00, 0x00, 0x02, 0x09, 0x00, 0x03, 0x01, 0xF7];

PioneerDDJ800.shutdown = function() {
    for (let deck = 0; deck < PioneerDDJ800.numDecks; deck++) {
        if (PioneerDDJ800.decks[deck].slipReverseTimer) {
            engine.stopTimer(PioneerDDJ800.decks[deck].slipReverseTimer);
        }
        engine.scratchDisable(deck + 1);
        engine.setValue(PioneerDDJ800.deckGroup(deck), "platter_touch", 0);
    }
    for (const connection of PioneerDDJ800.connections) {
        connection.disconnect();
    }
    for (const handler of PioneerDDJ800.inputHandlers) {
        handler.disconnect();
    }
    for (const key in PioneerDDJ800.usedLeds) {
        const status = Math.floor(key / 0x80);
        midi.sendShortMsg(status, key % 0x80, PioneerDDJ800.LED_OFF);
    }
};

//
// Browser
//

PioneerDDJ800.initBrowser = function() {
    const S = PioneerDDJ800.STATUS;
    const loadPromptOpen = function() {
        return engine.getValue("[Library]", "load_prompt") > 0;
    };

    PioneerDDJ800.bind(S.globalCC, 0x40, function(_channel, _control, value) {
        // While the "Load to the Deck" prompt is open, turning picks the deck.
        if (loadPromptOpen()) {
            engine.setValue("[Library]", "load_prompt_select", PioneerDDJ800.relative(value));
            return;
        }
        engine.setValue("[Library]", "MoveVertical", PioneerDDJ800.relative(value));
    });
    // BROWSE +SHIFT: waveform zoom for all decks.
    PioneerDDJ800.bind(S.globalCC, 0x64, function(_channel, _control, value) {
        // Turn right to enlarge the waveform. The engine's "zoom_down" zooms in.
        const key = PioneerDDJ800.relative(value) > 0 ? "waveform_zoom_down" : "waveform_zoom_up";
        for (let deck = 0; deck < PioneerDDJ800.numDecks; deck++) {
            engine.setValue(PioneerDDJ800.deckGroup(deck), key, 1);
        }
    });
    // BROWSE press (+SHIFT): opens a folder ("Library Forward"). On a track it
    // opens the "Load to the Deck" prompt; while it is open, the DJ either
    // turns and pushes BROWSE again, or presses a deck's LOAD button.
    const browsePress = function() {
        const onTrack = engine.getValue("[Library]", "focused_widget") ===
                PioneerDDJ800.FOCUS_TRACKS_TABLE;
        if (loadPromptOpen() || onTrack) {
            engine.setValue("[Library]", "load_prompt_push", 1);
            engine.setValue("[Library]", "load_prompt_push", 0);
            return;
        }
        engine.setValue("[Library]", "GoToItem", 1);
    };
    PioneerDDJ800.bindButton(S.globalNote, 0x41, browsePress);
    PioneerDDJ800.bindButton(S.globalNote, 0x42, browsePress);
    PioneerDDJ800.bindButton(S.globalNote, 0x65, function() {
        engine.setValue("[Library]", "MoveFocusBackward", 1);
    });
    PioneerDDJ800.bindButton(S.globalNote, 0x66, function() {
        engine.setValue("[Library]", "MoveFocusForward", 1);
    });
    PioneerDDJ800.bindButton(S.globalNote, 0x7A, function() {
        PioneerDDJ800.toggle("[Skin]", "show_maximized_library");
    });
    // BROWSE VIEW long press ("Tag Track" in rekordbox): add to Auto DJ.
    PioneerDDJ800.bindButton(S.globalNote, 0x67, function() {
        engine.setValue("[Library]", "AutoDjAddBottom", 1);
    });
    // BROWSE VIEW +SHIFT ("Related Tracks", 0x68) has no Mixxx equivalent yet.

    const loadNotes = [0x46, 0x47, 0x48, 0x49];
    const ejectNotes = [0x5D, 0x6D, 0x5E, 0x6F];
    for (let deck = 0; deck < PioneerDDJ800.numDecks; deck++) {
        PioneerDDJ800.bindButton(S.globalNote, loadNotes[deck], function() {
            // With the prompt open, LOAD picks this deck.
            if (loadPromptOpen()) {
                engine.setValue("[Library]", "load_prompt_load", deck + 1);
                return;
            }
            PioneerDDJ800.loadPressed(deck);
        });
        PioneerDDJ800.bindButton(S.globalNote, ejectNotes[deck], function() {
            engine.setValue(PioneerDDJ800.deckGroup(deck), "eject", 1);
        });
    }
};

// A second press within the window clones the track playing on the
// neighbouring deck (rekordbox "Instant Double").
PioneerDDJ800.loadPressed = function(deck) {
    const state = PioneerDDJ800.decks[deck];
    const group = PioneerDDJ800.deckGroup(deck);
    const now = Date.now();
    if (now - state.lastLoadPress < PioneerDDJ800.instantDoubleWindowMs) {
        const otherDeck = (deck % 2 === 0 ? deck + 1 : deck - 1) + 1;
        engine.setValue(group, "CloneFromDeck", otherDeck);
        state.lastLoadPress = 0;
        return;
    }
    state.lastLoadPress = now;
    engine.setValue(group, "LoadSelectedTrack", 1);
};

//
// Mixer
//

PioneerDDJ800.initMixer = function() {
    const S = PioneerDDJ800.STATUS;

    PioneerDDJ800.bindHiRes(S.globalCC, 0x1F, function(value) {
        engine.setParameter("[Master]", "crossfader", value);
    });
    // MASTER LEVEL, BOOTH LEVEL and HEADPHONES MIXING / LEVEL: the DDJ-800
    // applies them in hardware to its own outputs, and rekordbox leaves them
    // unassigned. With the HARDWARE audio target ([Master],audio_target = 1)
    // Mix Two plays through the DDJ-800 and leaves them to it. With the PC
    // target the mix plays through another interface, so the knobs drive
    // Mix Two's main, booth and headphone controls. The deck cannot report
    // these four at start (FIRMWARE.md 2.7), so on PC they take effect when
    // they first move.
    const levelKnob = function(cc, key) {
        PioneerDDJ800.bindHiRes(S.globalCC, cc, function(value) {
            if (engine.getValue("[Master]", "audio_target") === 0) {
                engine.setParameter("[Master]", key, value);
            }
        });
    };
    levelKnob(0x08, "gain");
    levelKnob(0x09, "booth_gain");
    levelKnob(0x0C, "headMix");
    levelKnob(0x0D, "headGain");
    PioneerDDJ800.bindHiRes(S.globalCC, 0x03, function(value) {
        const numSamplers = engine.getValue("[App]", "num_samplers");
        for (let i = 1; i <= numSamplers; i++) {
            engine.setParameter("[Sampler" + i + "]", "volume", value);
        }
    });

    // MASTER CUE: put the main mix on the headphone cue bus, as on Pioneer
    // mixers ([Master],main_cue; also the CUE button under MASTER LEVEL on
    // screen).
    const masterCue = function() { PioneerDDJ800.toggle("[Master]", "main_cue"); };
    PioneerDDJ800.bindButton(S.globalNote, 0x63, masterCue);
    PioneerDDJ800.bindButton(S.globalNote, 0x62, masterCue);
    PioneerDDJ800.connectLed("[Master]", "main_cue", S.globalNote, 0x63);
    PioneerDDJ800.connectLed("[Master]", "main_cue", S.globalNote, 0x62);

    // Crossfader start (sent by the controller when fader start is enabled).
    const crossfaderStart = function(orientation, key) {
        for (let deck = 0; deck < PioneerDDJ800.numDecks; deck++) {
            const group = PioneerDDJ800.deckGroup(deck);
            if (engine.getValue(group, "orientation") === orientation) {
                engine.setValue(group, key, 1);
            }
        }
    };
    PioneerDDJ800.bindButton(S.globalNote, 0x50, function() { crossfaderStart(0, "play"); });
    PioneerDDJ800.bindButton(S.globalNote, 0x51, function() { crossfaderStart(2, "play"); });
    PioneerDDJ800.bindButton(S.globalNote, 0x52, function() { crossfaderStart(0, "cue_gotoandstop"); });
    PioneerDDJ800.bindButton(S.globalNote, 0x53, function() { crossfaderStart(2, "cue_gotoandstop"); });

    // MIC ON/OFF. Mic level and EQ knobs are processed in hardware.
    PioneerDDJ800.bindButton(S.globalNote, 0x6B, function() {
        engine.setValue("[Microphone]", "talkover", 1);
    });
    PioneerDDJ800.bindButton(S.globalNote, 0x6A, function() {
        engine.setValue("[Microphone]", "talkover", 0);
    });
};

//
// Sound Color FX
//

PioneerDDJ800.initCfx = function() {
    const S = PioneerDDJ800.STATUS;
    const knobs = [0x17, 0x18, 0x19, 0x1A];
    for (let channel = 0; channel < 4; channel++) {
        const group = "[QuickEffectRack1_[Channel" + (channel + 1) + "]]";
        PioneerDDJ800.bindHiRes(S.globalCC, knobs[channel], function(value) {
            engine.setParameter(group, "super1", value);
        });
    }
    // The selected CFX lives in [SoundColorFX],cfx1..cfx4, shared with the
    // on-screen buttons under HEADPHONES; the engine keeps one on at a time.
    for (let cfx = 0; cfx < 4; cfx++) {
        PioneerDDJ800.bindButton(S.globalNote, 0x00 + cfx, function() { PioneerDDJ800.selectCfx(cfx); });
        PioneerDDJ800.bindButton(S.globalNote, 0x08 + cfx, function() { PioneerDDJ800.selectCfx(cfx); });
        PioneerDDJ800.connect("[SoundColorFX]", "cfx" + (cfx + 1), function() {
            PioneerDDJ800.applyCfx();
        });
    }
    // Pioneer behaviour: with no CFX selected the COLOR knobs do nothing.
    // Start with FILTER so the knobs behave like Mixxx's default filter.
    PioneerDDJ800.selectCfx(3, true);
};

PioneerDDJ800.activeCfx = function() {
    for (let cfx = 0; cfx < 4; cfx++) {
        if (engine.getValue("[SoundColorFX]", "cfx" + (cfx + 1)) > 0) {
            return cfx;
        }
    }
    return -1;
};

PioneerDDJ800.selectCfx = function(cfx, on) {
    const key = "cfx" + (cfx + 1);
    const value = on === undefined ? (engine.getValue("[SoundColorFX]", key) > 0 ? 0 : 1) : (on ? 1 : 0);
    // The engine also keeps one on at a time, but later (queued); do it now
    // so the LEDs are right at once.
    for (let other = 0; other < 4; other++) {
        if (other !== cfx && value > 0) {
            engine.setValue("[SoundColorFX]", "cfx" + (other + 1), 0);
        }
    }
    engine.setValue("[SoundColorFX]", key, value);
    PioneerDDJ800.applyCfx();
};

PioneerDDJ800.applyCfx = function() {
    // The engine loads each button's chain preset (right-click a CFX button
    // on screen to change it) and switches the QuickEffects on or off.
    const active = PioneerDDJ800.activeCfx();
    for (let cfx = 0; cfx < 4; cfx++) {
        PioneerDDJ800.setLed(PioneerDDJ800.STATUS.globalNote, 0x00 + cfx, cfx === active);
    }
};

//
// Beat FX
//

PioneerDDJ800.initBeatFx = function() {
    const S = PioneerDDJ800.STATUS;
    const unit = PioneerDDJ800.beatFxUnit;
    const slot = PioneerDDJ800.beatFxSlot;

    // FX SELECT positions 0x20-0x2D load visible effect 1-14. Order the
    // visible effects list in Preferences > Effects to match the labels.
    for (let position = 0; position < 14; position++) {
        PioneerDDJ800.bindButton(S.fxNote, 0x20 + position, function() {
            engine.setValue(slot, "loaded_effect", position + 1);
        });
    }

    Object.keys(PioneerDDJ800.fxAssignTargets).forEach(function(note) {
        PioneerDDJ800.bindButton(S.fxNote, Number(note), function() {
            PioneerDDJ800.assignBeatFx(PioneerDDJ800.fxAssignTargets[note]);
        });
    });

    PioneerDDJ800.bindHiRes(S.fxCC, 0x02, function(value) {
        engine.setParameter(unit, "mix", value);
    });
    PioneerDDJ800.bindButton(S.fxNote, 0x47, function() {
        PioneerDDJ800.toggle(slot, "enabled");
    });
    // SHIFT + ON ("Release FX"): switch the effect off immediately.
    PioneerDDJ800.bindButton(S.fxNote, 0x43, function() {
        engine.setValue(slot, "enabled", 0);
    });
    PioneerDDJ800.connectLed(slot, "enabled", S.fxNote, 0x47);

    // BEAT </>: step the effect's meta knob, SHIFT + BEAT < resets it.
    const stepMeta = function(delta) {
        const meta = engine.getParameter(slot, "meta") + delta;
        engine.setParameter(slot, "meta", Math.min(1, Math.max(0, meta)));
    };
    PioneerDDJ800.bindButton(S.fxNote, 0x4A, function() { stepMeta(-0.125); });
    PioneerDDJ800.bindButton(S.fxNote, 0x4B, function() { stepMeta(0.125); });
    PioneerDDJ800.bindButton(S.fxNote, 0x66, function() {
        engine.setParameter(slot, "meta", 0.5);
    });
};

PioneerDDJ800.assignBeatFx = function(targets) {
    const unit = PioneerDDJ800.beatFxUnit;
    const all = ["[Channel1]", "[Channel2]", "[Channel3]", "[Channel4]", "[Master]"];
    for (const target of all) {
        engine.setValue(unit, "group_" + target + "_enable", targets.indexOf(target) >= 0 ? 1 : 0);
    }
    const numSamplers = engine.getValue("[App]", "num_samplers");
    const samplersOn = targets.indexOf("[Sampler]") >= 0 ? 1 : 0;
    for (let i = 1; i <= numSamplers; i++) {
        engine.setValue(unit, "group_[Sampler" + i + "]_enable", samplersOn);
    }
};

//
// Deck
//

PioneerDDJ800.initDeck = function(deck) {
    const S = PioneerDDJ800.STATUS;
    const note = S.deckNote + deck;
    const cc = S.deckCC + deck;
    const group = PioneerDDJ800.deckGroup(deck);
    const state = PioneerDDJ800.decks[deck];
    const eqGroup = "[EqualizerRack1_" + group + "_Effect1]";

    const pushButton = function(midino, key) {
        PioneerDDJ800.bind(note, midino, function(_channel, _control, value) {
            engine.setValue(group, key, value > 0 ? 1 : 0);
        });
    };
    const toggleButton = function(midino, key) {
        PioneerDDJ800.bindButton(note, midino, function() { PioneerDDJ800.toggle(group, key); });
    };

    // Transport
    toggleButton(0x0B, "play");
    toggleButton(0x47, "play");
    pushButton(0x0C, "cue_default");
    pushButton(0x48, "start");
    PioneerDDJ800.connectLed(group, "play_indicator", note, 0x0B);
    PioneerDDJ800.connectLed(group, "play_indicator", note, 0x47);
    PioneerDDJ800.connectLed(group, "cue_indicator", note, 0x0C);
    PioneerDDJ800.connectLed(group, "cue_indicator", note, 0x48);

    // Tempo. The rate direction preference is honoured by "rate" itself,
    // so the slider is inverted here to keep UI and hardware in sync.
    // rate_physical tells the skin where the slider is (slider BPM). Soft
    // takeover keeps the tempo from jumping when the slider is away from
    // the playing tempo, e.g. after SYNC moved it; the slider has to catch
    // up first. While the deck follows a MASTER, the engine ignores its
    // slider altogether.
    engine.softTakeover(group, "rate", true);
    const tempo = function(value) {
        engine.setValue(group, "rate_physical", 1 - 2 * value);
        engine.setValue(group, "rate", 1 - 2 * value);
    };
    PioneerDDJ800.bindHiRes(cc, 0x00, tempo);
    PioneerDDJ800.bindHiRes(cc, 0x05, tempo);
    toggleButton(0x1A, "keylock");
    PioneerDDJ800.connectLed(group, "keylock", note, 0x1A);
    // TEMPO RANGE steps ±6, ±10, ±16 %, WIDE (the engine owns the list;
    // while synced it widens the range itself when needed).
    pushButton(0x60, "rate_range_next");

    // SYNC: each press turns sync on or off, as in rekordbox ("Sync On/Off").
    toggleButton(0x58, "sync_enabled");
    PioneerDDJ800.connectLed(group, "sync_enabled", note, 0x58);
    // SHIFT + SYNC: MASTER on/off. Makes this deck the tempo master, also
    // while it is stopped; pressing it on the master hands MASTER back to
    // automatic choice. sync_leader: 1 = chosen automatically, 2 = MASTER.
    PioneerDDJ800.bindButton(note, 0x5C, function() {
        const explicitMaster = engine.getValue(group, "sync_leader") === 2;
        engine.setValue(group, "sync_leader", explicitMaster ? 0 : 1);
    });
    PioneerDDJ800.connectLed(group, "sync_leader", note, 0x5C);
    // KEY SYNC (SHIFT + SEMITONE DOWN): on/off; keeps this deck in the key of
    // the master deck. KEY RESET or a semitone change turns it off.
    // KEY SYNC: the engine may refuse (no other deck with a key). Set the
    // LED from its answer, so the deck never shows a state the app has not.
    PioneerDDJ800.bindButton(note, 0x65, function() {
        PioneerDDJ800.toggle(group, "keysync_enabled");
        PioneerDDJ800.setLed(note, 0x65, engine.getValue(group, "keysync_enabled") > 0);
    });
    PioneerDDJ800.connectLed(group, "keysync_enabled", note, 0x65);
    pushButton(0x64, "reset_key");

    // Loops
    PioneerDDJ800.bind(note, 0x10, function(_channel, _control, value) {
        PioneerDDJ800.loopButton(deck, "In", value);
    });
    PioneerDDJ800.bind(note, 0x11, function(_channel, _control, value) {
        PioneerDDJ800.loopButton(deck, "Out", value);
    });
    pushButton(0x14, "beatloop_4_activate");
    pushButton(0x4D, "reloop_toggle");
    // SHIFT + RELOOP/EXIT (0x50) is rekordbox's Active Loop (mark a memory
    // loop active). The engine has no active loop yet, so it is left unbound.
    pushButton(0x12, "loop_halve");
    pushButton(0x13, "loop_double");
    PioneerDDJ800.connectLed(group, "loop_enabled", note, 0x4D);
    PioneerDDJ800.connect(group, "loop_enabled", function(value) {
        // Tells the controller to send 1/2x / 2x instead of cue call.
        PioneerDDJ800.setLed(S.stateNote, 0x04 + deck, value > 0);
        if (!value) {
            state.loopAdjustIn = false;
            state.loopAdjustOut = false;
            PioneerDDJ800.setLed(note, 0x10, false);
            PioneerDDJ800.setLed(note, 0x11, false);
        }
    });

    // Cue / loop call and memory
    PioneerDDJ800.bindButton(note, 0x51, function() { PioneerDDJ800.callCue(deck, -1); });
    PioneerDDJ800.bindButton(note, 0x53, function() { PioneerDDJ800.callCue(deck, 1); });
    PioneerDDJ800.bindButton(note, 0x3D, function() { PioneerDDJ800.memoryCue(deck); });
    PioneerDDJ800.bindButton(note, 0x3E, function() { PioneerDDJ800.deleteCue(deck); });

    // Slip, reverse, vinyl
    toggleButton(0x40, "slip_enabled");
    PioneerDDJ800.connectLed(group, "slip_enabled", note, 0x40);
    PioneerDDJ800.connect(group, "slip_enabled", function(value) {
        PioneerDDJ800.setLed(S.stateNote, 0x24 + deck, value > 0);
    });
    PioneerDDJ800.bind(note, 0x15, function(_channel, _control, value) {
        PioneerDDJ800.slipReverse(deck, value > 0);
    });
    PioneerDDJ800.connectLed(group, "reverseroll", note, 0x15);
    toggleButton(0x38, "reverse");
    PioneerDDJ800.connectLed(group, "reverse", note, 0x38);
    PioneerDDJ800.bindButton(note, 0x17, function() {
        state.vinylMode = !state.vinylMode;
        PioneerDDJ800.updateVinylLed(deck);
    });
    // The controller reports its own vinyl state.
    PioneerDDJ800.bind(note, 0x3A, function(_channel, _control, value) {
        state.vinylMode = value > 0;
        PioneerDDJ800.updateVinylLed(deck);
    });

    toggleButton(0x35, "quantize");
    PioneerDDJ800.connectLed(group, "quantize", note, 0x35);

    // Track search: |<< returns to the start, >>| loads the next library row.
    pushButton(0x5E, "start");
    PioneerDDJ800.bindButton(note, 0x5F, function() {
        engine.setValue("[Library]", "MoveDown", 1);
        engine.setValue(group, "LoadSelectedTrack", 1);
    });
    pushButton(0x70, "back");
    pushButton(0x71, "fwd");

    // Beat grid edit states (held while GRID ADJUST / GRID SLIDE is held).
    PioneerDDJ800.bind(note, 0x79, function(_channel, _control, value) {
        state.gridAdjust = value > 0;
        state.gridTicks = 0;
    });
    PioneerDDJ800.bind(note, 0x0A, function(_channel, _control, value) {
        state.gridSlide = value > 0;
        state.gridTicks = 0;
    });

    // Mixer channel
    PioneerDDJ800.bindHiRes(cc, 0x04, function(value) { engine.setParameter(group, "pregain", value); });
    PioneerDDJ800.bindHiRes(cc, 0x07, function(value) { engine.setParameter(eqGroup, "parameter3", value); });
    PioneerDDJ800.bindHiRes(cc, 0x0B, function(value) { engine.setParameter(eqGroup, "parameter2", value); });
    PioneerDDJ800.bindHiRes(cc, 0x0F, function(value) { engine.setParameter(eqGroup, "parameter1", value); });
    PioneerDDJ800.bindHiRes(cc, 0x13, function(value) { engine.setParameter(group, "volume", value); });
    toggleButton(0x54, "pfl");
    PioneerDDJ800.connectLed(group, "pfl", note, 0x54);
    pushButton(0x68, "bpm_tap");
    PioneerDDJ800.connect(group, "vu_meter", function(value) {
        midi.sendShortMsg(cc, 0x02, Math.round(value * 0x7F));
    });

    // Channel fader start
    PioneerDDJ800.bindButton(note, 0x66, function() { engine.setValue(group, "play", 1); });
    PioneerDDJ800.bindButton(note, 0x52, function() { engine.setValue(group, "cue_gotoandstop", 1); });

    // Input select: DECK sends 0x55/0x56, LINE/PHONO sends 0x57 (and 0x46 on CH3/4).
    const setPassthrough = function(enabled) {
        if (PioneerDDJ800.inputSelectPassthrough) {
            engine.setValue(group, "passthrough", enabled ? 1 : 0);
        }
    };
    PioneerDDJ800.bindButton(note, 0x55, function() { setPassthrough(false); });
    PioneerDDJ800.bindButton(note, 0x56, function() { setPassthrough(false); });
    PioneerDDJ800.bindButton(note, 0x57, function() { setPassthrough(true); });
    PioneerDDJ800.bindButton(note, 0x46, function() { setPassthrough(true); });

    // Shift is only tracked for completeness: the controller already sends
    // distinct notes for shifted controls.
    PioneerDDJ800.bind(note, 0x3F, function(_channel, _control, value) {
        state.shift = value > 0;
    });
};

PioneerDDJ800.updateVinylLed = function(deck) {
    const note = PioneerDDJ800.STATUS.deckNote + deck;
    PioneerDDJ800.setLed(note, 0x17, PioneerDDJ800.decks[deck].vinylMode);
};

// LOOP IN / OUT: set the loop point, or while a loop is active toggle jog
// adjustment of that loop point (Pioneer behaviour).
PioneerDDJ800.loopButton = function(deck, which, value) {
    const group = PioneerDDJ800.deckGroup(deck);
    const state = PioneerDDJ800.decks[deck];
    const note = PioneerDDJ800.STATUS.deckNote + deck;
    if (!engine.getValue(group, "loop_enabled")) {
        engine.setValue(group, which === "In" ? "loop_in" : "loop_out", value > 0 ? 1 : 0);
        return;
    }
    if (value === 0) {
        return;
    }
    if (which === "In") {
        state.loopAdjustIn = !state.loopAdjustIn;
        state.loopAdjustOut = false;
    } else {
        state.loopAdjustOut = !state.loopAdjustOut;
        state.loopAdjustIn = false;
    }
    PioneerDDJ800.setLed(note, 0x10, state.loopAdjustIn);
    PioneerDDJ800.setLed(note, 0x11, state.loopAdjustOut);
};


// CALL ◀ / ▶: call the previous / next memory cue or memory loop.
// SLIP REVERSE: reverse with slip while held, and back to normal play after
// 8 beats even if the button stays down.
PioneerDDJ800.slipReverse = function(deck, pressed) {
    const state = PioneerDDJ800.decks[deck];
    const group = PioneerDDJ800.deckGroup(deck);
    if (state.slipReverseTimer) {
        engine.stopTimer(state.slipReverseTimer);
        state.slipReverseTimer = 0;
    }
    engine.setValue(group, "reverseroll", pressed ? 1 : 0);
    const bpm = engine.getValue(group, "bpm");
    if (pressed && bpm > 0) {
        const ms = PioneerDDJ800.slipReverseBeats * 60000 / bpm;
        state.slipReverseTimer = engine.beginTimer(ms, function() {
            state.slipReverseTimer = 0;
            engine.setValue(group, "reverseroll", 0);
        }, true);
    }
};

PioneerDDJ800.callCue = function(deck, direction) {
    const key = direction < 0 ? "memory_cue_prev" : "memory_cue_next";
    engine.setValue(PioneerDDJ800.deckGroup(deck), key, 1);
};

// MEMORY: store the active loop as a memory loop, otherwise the current
// position as a memory cue.
PioneerDDJ800.memoryCue = function(deck) {
    engine.setValue(PioneerDDJ800.deckGroup(deck), "memory_cue_set", 1);
};

// DELETE: delete the memory cue just stored or called.
PioneerDDJ800.deleteCue = function(deck) {
    engine.setValue(PioneerDDJ800.deckGroup(deck), "memory_cue_delete", 1);
};

//
// Jog wheels
//

PioneerDDJ800.initJog = function(deck) {
    const S = PioneerDDJ800.STATUS;
    const note = S.deckNote + deck;
    const cc = S.deckCC + deck;
    const group = PioneerDDJ800.deckGroup(deck);

    const turn = function(_channel, _control, value) {
        PioneerDDJ800.jogTurn(deck, value - 0x40);
    };
    PioneerDDJ800.bind(cc, 0x22, turn); // top, vinyl mode
    PioneerDDJ800.bind(cc, 0x23, turn); // top, CDJ mode
    PioneerDDJ800.bind(cc, 0x21, turn); // side

    // SHIFT + jog slides the beat grid, as in rekordbox.
    const slide = function(_channel, _control, value) {
        PioneerDDJ800.gridTurn(deck, value - 0x40, true);
    };
    PioneerDDJ800.bind(cc, 0x1F, slide); // SHIFT + top
    PioneerDDJ800.bind(cc, 0x26, slide); // SHIFT + side

    // TRACK SEARCH + top: Super Fast Search (manual p.20). The firmware sends
    // CC 0x29 while a TRACK SEARCH button is held (FIRMWARE.md 2.2); not yet
    // confirmed on the deck.
    PioneerDDJ800.bind(cc, 0x29, function(_channel, _control, value) {
        const duration = engine.getValue(group, "duration");
        if (duration > 0) {
            const seconds = (value - 0x40) / PioneerDDJ800.jogResolution * PioneerDDJ800.superFastSearchSecondsPerTurn;
            const position = engine.getValue(group, "playposition") + seconds / duration;
            engine.setValue(group, "playposition", Math.min(1, Math.max(0, position)));
        }
    });

    PioneerDDJ800.bind(note, 0x36, function(_channel, _control, value) {
        PioneerDDJ800.jogTouch(deck, value > 0);
    });
    PioneerDDJ800.bind(note, 0x67, function(_channel, _control, value) {
        // SHIFT + touch slides the grid, it never scratches.
        if (value === 0) {
            engine.scratchDisable(deck + 1);
            engine.setValue(group, "platter_touch", 0);
        }
    });
};

// Scratch algorithm "Rekordbox" (Preferences > Decks > Platter options): the
// engine's platter ([ChannelN],platter_touch / platter_turn) does the
// scratching, as in rekordbox; "Mixxx" keeps Mixxx's own scratching.
PioneerDDJ800.usesPlatter = function() {
    return engine.getValue("[App]", "scratch_algorithm") === 1;
};

PioneerDDJ800.jogTouch = function(deck, touched) {
    const group = PioneerDDJ800.deckGroup(deck);
    const state = PioneerDDJ800.decks[deck];
    if (state.loopAdjustIn || state.loopAdjustOut || state.gridAdjust || state.gridSlide) {
        return;
    }
    const scratch = touched && state.vinylMode;
    if (PioneerDDJ800.usesPlatter()) {
        engine.scratchDisable(deck + 1);
        engine.setValue(group, "platter_touch", scratch ? 1 : 0);
    } else if (scratch) {
        const alpha = 1.0 / 8;
        engine.scratchEnable(deck + 1, PioneerDDJ800.jogResolution, 33 + 1 / 3, alpha, alpha / 32);
    } else {
        engine.scratchDisable(deck + 1);
        engine.setValue(group, "platter_touch", 0);
    }
};

// GRID SLIDE (or SHIFT + jog) moves the beat grid with every tick, nothing
// held back; GRID ADJUST changes its tempo, one step per gridAdjustTicks.
PioneerDDJ800.gridTurn = function(deck, ticks, slide) {
    const group = PioneerDDJ800.deckGroup(deck);
    const state = PioneerDDJ800.decks[deck];
    if (slide) {
        engine.setValue(group, "beats_translate_ms", ticks * PioneerDDJ800.gridSlideMsPerTick);
        return;
    }
    state.gridTicks += ticks;
    while (Math.abs(state.gridTicks) >= PioneerDDJ800.gridAdjustTicks) {
        const forward = state.gridTicks > 0;
        engine.setValue(group, forward ? "beats_adjust_faster" : "beats_adjust_slower", 1);
        state.gridTicks += forward ? -PioneerDDJ800.gridAdjustTicks : PioneerDDJ800.gridAdjustTicks;
    }
};

PioneerDDJ800.jogTurn = function(deck, ticks) {
    const group = PioneerDDJ800.deckGroup(deck);
    const state = PioneerDDJ800.decks[deck];

    if (state.gridSlide || state.gridAdjust) {
        PioneerDDJ800.gridTurn(deck, ticks, state.gridSlide);
        return;
    }

    if (engine.getValue(group, "loop_enabled") && (state.loopAdjustIn || state.loopAdjustOut)) {
        const key = state.loopAdjustIn ? "loop_start_position" : "loop_end_position";
        engine.setValue(group, key,
            engine.getValue(group, key) + ticks * PioneerDDJ800.loopAdjustMultiply);
        return;
    }

    // Every turn goes to the engine's platter, whatever the scratch algorithm,
    // except while Mixxx's own scratch is on: untouched, a playing deck bends
    // with rekordbox's rule (3 % per 1x platter speed) and a paused deck
    // scratches (1.8 s per turn), as in rekordbox. Mixxx's "jog" is far too
    // strong at 6480 ticks per turn.
    if (engine.isScratching(deck + 1)) {
        engine.scratchTick(deck + 1, ticks);
    } else {
        engine.setValue(group, "platter_turn", ticks / PioneerDDJ800.jogResolution);
    }
};

//
// Performance pads
//

PioneerDDJ800.initPads = function(deck) {
    const S = PioneerDDJ800.STATUS;
    const note = S.deckNote + deck;
    const group = PioneerDDJ800.deckGroup(deck);
    const padStatus = S.padNote + deck * 2;
    const shiftPadStatus = padStatus + 1;

    PioneerDDJ800.padModeButtons.forEach(function(midino, mode) {
        PioneerDDJ800.bindButton(note, midino, function() {
            if (mode === PioneerDDJ800.PAD_MODE.keyboard) {
                // SHIFT + HOT CUE: pick the keyboard hot cue again (manual p.53).
                PioneerDDJ800.decks[deck].keyboardCue = 0;
            }
            PioneerDDJ800.selectPadMode(deck, mode);
        });
    });
    for (let mode = 0; mode < 8; mode++) {
        PioneerDDJ800.bindButton(note, PioneerDDJ800.pageLeftButtons[mode], function() {
            PioneerDDJ800.changePage(deck, mode, -1);
        });
        PioneerDDJ800.bindButton(note, PioneerDDJ800.pageRightButtons[mode], function() {
            PioneerDDJ800.changePage(deck, mode, 1);
        });
        PioneerDDJ800.bindButton(note, PioneerDDJ800.shiftPageLeftButtons[mode], function() {
            PioneerDDJ800.changeSamplerBank(deck, -1);
        });
        PioneerDDJ800.bindButton(note, PioneerDDJ800.shiftPageRightButtons[mode], function() {
            PioneerDDJ800.changeSamplerBank(deck, 1);
        });
    }

    for (let midino = 0x00; midino <= 0x7F; midino++) {
        const mode = midino >> 4;
        // The deck pages the hot cue pads itself (PAGE, allowed by the
        // permission flags): page 2 sends 0x08-0x0F = hot cues 9-16.
        const pad = mode === PioneerDDJ800.PAD_MODE.hotCue ? midino & 0x0F : midino & 0x07;
        PioneerDDJ800.bind(padStatus, midino, function(_channel, _control, value) {
            PioneerDDJ800.padPressed(deck, mode, pad, value > 0, false);
        });
        PioneerDDJ800.bind(shiftPadStatus, midino, function(_channel, _control, value) {
            PioneerDDJ800.padPressed(deck, mode, pad, value > 0, true);
        });
    }

    for (let i = 1; i <= PioneerDDJ800.numHotCues; i++) {
        PioneerDDJ800.connect(group, "hotcue_" + i + "_status", function() {
            PioneerDDJ800.refreshPads(deck, PioneerDDJ800.PAD_MODE.hotCue);
            PioneerDDJ800.refreshPads(deck, PioneerDDJ800.PAD_MODE.keyboard);
        });
    }
    PioneerDDJ800.connect(group, "loop_enabled", function() {
        PioneerDDJ800.refreshPads(deck, PioneerDDJ800.PAD_MODE.beatLoop);
    });
    PioneerDDJ800.connect(group, "beatloop_size", function() {
        PioneerDDJ800.refreshPads(deck, PioneerDDJ800.PAD_MODE.beatLoop);
    });
    PioneerDDJ800.connect(group, "pitch", function() {
        PioneerDDJ800.refreshPads(deck, PioneerDDJ800.PAD_MODE.keyShift);
        PioneerDDJ800.refreshPads(deck, PioneerDDJ800.PAD_MODE.keyboard);
    });
    const numSamplers = engine.getValue("[App]", "num_samplers");
    for (let i = 1; i <= numSamplers; i++) {
        const refreshSampler = function() {
            PioneerDDJ800.refreshPads(deck, PioneerDDJ800.PAD_MODE.sampler);
        };
        PioneerDDJ800.connect("[Sampler" + i + "]", "track_loaded", refreshSampler);
        PioneerDDJ800.connect("[Sampler" + i + "]", "play", refreshSampler);
    }
};

PioneerDDJ800.selectPadMode = function(deck, mode) {
    const note = PioneerDDJ800.STATUS.deckNote + deck;
    PioneerDDJ800.decks[deck].padMode = mode;
    PioneerDDJ800.padModeButtons.forEach(function(midino, buttonMode) {
        PioneerDDJ800.setLed(note, midino, buttonMode === mode);
    });
    PioneerDDJ800.refreshPads(deck, mode);
};

PioneerDDJ800.changePage = function(deck, mode, direction) {
    const state = PioneerDDJ800.decks[deck];
    const group = PioneerDDJ800.deckGroup(deck);
    const factor = direction > 0 ? 2 : 0.5;
    const clamp = function(value, min, max) { return Math.min(max, Math.max(min, value)); };
    switch (mode) {
    case PioneerDDJ800.PAD_MODE.hotCue:
        // rekordbox: PAGE DOWN / UP. The deck switches between hot cues
        // A-H and I-P by itself and sends the other pad notes.
        break;
    case PioneerDDJ800.PAD_MODE.padFx1:
        state.rollScale = clamp(state.rollScale * factor, 0.5, 4);
        break;
    case PioneerDDJ800.PAD_MODE.beatJump:
        state.beatJumpScale = clamp(state.beatJumpScale * factor, 1 / 16, 16);
        break;
    case PioneerDDJ800.PAD_MODE.sampler:
        PioneerDDJ800.changeSamplerBank(deck, direction);
        break;
    case PioneerDDJ800.PAD_MODE.keyboard:
        state.keyboardBase = clamp(state.keyboardBase + direction * 8, -16, 8);
        break;
    case PioneerDDJ800.PAD_MODE.beatLoop:
        state.beatLoopScale = clamp(state.beatLoopScale * factor, 1 / 8, 8);
        break;
    case PioneerDDJ800.PAD_MODE.keyShift:
        state.keyShiftBase = clamp(state.keyShiftBase + direction * 8, -16, 8);
        break;
    }
    PioneerDDJ800.refreshPads(deck, mode);
};

PioneerDDJ800.changeSamplerBank = function(deck, direction) {
    const state = PioneerDDJ800.decks[deck];
    const banks = Math.max(1, Math.ceil(engine.getValue("[App]", "num_samplers") / 8));
    state.samplerBank = (state.samplerBank + direction + banks) % banks;
    PioneerDDJ800.refreshPads(deck, PioneerDDJ800.PAD_MODE.sampler);
};

PioneerDDJ800.samplerGroup = function(deck, pad) {
    return "[Sampler" + (PioneerDDJ800.decks[deck].samplerBank * 8 + pad + 1) + "]";
};

PioneerDDJ800.padPressed = function(deck, mode, pad, pressed, shift) {
    const M = PioneerDDJ800.PAD_MODE;
    const group = PioneerDDJ800.deckGroup(deck);
    const state = PioneerDDJ800.decks[deck];

    // The note encodes the controller's pad mode, so follow it if the mode
    // buttons were pressed before Mixxx started.
    if (pressed && state.padMode !== mode) {
        PioneerDDJ800.selectPadMode(deck, mode);
    }

    switch (mode) {
    case M.hotCue:
        engine.setValue(group, "hotcue_" + (pad + 1) + (shift ? "_clear" : "_activate"), pressed ? 1 : 0);
        break;
    case M.padFx1: {
        const size = PioneerDDJ800.rollSizes[pad] * state.rollScale;
        engine.setValue(group, "beatlooproll_" + size + "_activate", pressed ? 1 : 0);
        PioneerDDJ800.setPadLed(deck, mode, pad, pressed);
        break;
    }
    case M.beatJump:
        if (pressed) {
            const size = PioneerDDJ800.beatJumpSizes[pad] * state.beatJumpScale;
            engine.setValue(group, "beatjump", pad % 2 === 0 ? -size : size);
        }
        break;
    case M.sampler: {
        if (!pressed) {
            break;
        }
        const sampler = PioneerDDJ800.samplerGroup(deck, pad);
        if (shift) {
            engine.setValue(sampler, "cue_gotoandstop", 1);
        } else if (engine.getValue(sampler, "track_loaded")) {
            engine.setValue(sampler, "cue_gotoandplay", 1);
        } else {
            engine.setValue(sampler, "LoadSelectedTrack", 1);
        }
        break;
    }
    case M.keyboard:
        if (!state.keyboardCue) {
            // The first pad picks the hot cue to play; with no hot cue
            // there, the current position becomes one (manual p.53).
            if (pressed && !shift) {
                state.keyboardCue = pad + 1;
                if (!engine.getValue(group, "hotcue_" + state.keyboardCue + "_status")) {
                    engine.setValue(group, "hotcue_" + state.keyboardCue + "_set", 1);
                }
                PioneerDDJ800.refreshPads(deck, mode);
            }
            break;
        }
        if (pressed) {
            if (shift) {
                engine.setValue(group, "reset_key", 1);
            } else {
                engine.setValue(group, "pitch", state.keyboardBase + PioneerDDJ800.keyboardSemitones[pad]);
                engine.setValue(group, "hotcue_" + state.keyboardCue + "_activate", 1);
            }
        } else {
            engine.setValue(group, "hotcue_" + state.keyboardCue + "_activate", 0);
        }
        break;
    case M.padFx2:
        PioneerDDJ800.padFx2(deck, pad, pressed);
        PioneerDDJ800.setPadLed(deck, mode, pad, pressed);
        break;
    case M.beatLoop:
        if (pressed) {
            // Starts a loop of this size, or ends it when it is the active one.
            const size = PioneerDDJ800.beatLoopSizes[pad] * state.beatLoopScale;
            engine.setValue(group, "beatloop_" + size + "_toggle", 1);
        }
        break;
    case M.keyShift:
        if (pressed) {
            if (shift) {
                engine.setValue(group, "reset_key", 1);
            } else {
                engine.setValue(group, "pitch", state.keyShiftBase + PioneerDDJ800.keyShiftSemitones[pad]);
            }
        }
        break;
    }
};

// Pads 1-3 / 5-7 hold effect 1-3 of effect unit 2 / 3 on this deck; pads 4
// and 8 hold the whole unit.
PioneerDDJ800.padFx2 = function(deck, pad, pressed) {
    const state = PioneerDDJ800.decks[deck];
    const unit = PioneerDDJ800.padFx2Units[pad < 4 ? 0 : 1];
    const routeKey = "group_" + PioneerDDJ800.deckGroup(deck) + "_enable";
    const slotIndex = pad % 4;
    const slots = slotIndex === 3 ? [1, 2, 3] : [slotIndex + 1];

    if (pressed) {
        state.padFx2Held[pad] = engine.getValue(unit, routeKey);
        engine.setValue(unit, routeKey, 1);
    } else if (state.padFx2Held[pad] !== undefined) {
        engine.setValue(unit, routeKey, state.padFx2Held[pad]);
        delete state.padFx2Held[pad];
    }
    for (const slot of slots) {
        const slotGroup = unit.slice(0, -1) + "_Effect" + slot + "]";
        engine.setValue(slotGroup, "enabled", pressed ? 1 : 0);
    }
};

PioneerDDJ800.setPadLed = function(deck, mode, pad, on) {
    const status = PioneerDDJ800.STATUS.padNote + deck * 2;
    const midino = mode * 0x10 + pad;
    PioneerDDJ800.setLed(status, midino, on);
    PioneerDDJ800.setLed(status + 1, midino, on);
};

PioneerDDJ800.refreshPads = function(deck, mode) {
    const M = PioneerDDJ800.PAD_MODE;
    const state = PioneerDDJ800.decks[deck];
    const group = PioneerDDJ800.deckGroup(deck);
    if (state.padMode !== mode) {
        return;
    }
    // Hot cue LEDs go out for both pages; the deck shows the page it is on.
    const numPads = mode === M.hotCue ? PioneerDDJ800.numHotCues : 8;
    for (let pad = 0; pad < numPads; pad++) {
        let on = false;
        switch (mode) {
        case M.hotCue:
            on = engine.getValue(group, "hotcue_" + (pad + 1) + "_status") > 0;
            break;
        case M.beatJump:
            on = true;
            break;
        case M.sampler:
            on = engine.getValue(PioneerDDJ800.samplerGroup(deck, pad), "track_loaded") > 0;
            break;
        case M.keyboard:
            // Before a hot cue is picked, show which pads have one.
            on = state.keyboardCue ?
                engine.getValue(group, "pitch") === state.keyboardBase + PioneerDDJ800.keyboardSemitones[pad] :
                engine.getValue(group, "hotcue_" + (pad + 1) + "_status") > 0;
            break;
        case M.beatLoop:
            on = engine.getValue(group, "loop_enabled") > 0 &&
                engine.getValue(group, "beatloop_size") === PioneerDDJ800.beatLoopSizes[pad] * state.beatLoopScale;
            break;
        case M.keyShift:
            on = engine.getValue(group, "pitch") === state.keyShiftBase + PioneerDDJ800.keyShiftSemitones[pad];
            break;
        }
        PioneerDDJ800.setPadLed(deck, mode, pad, on);
    }
};
