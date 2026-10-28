#pragma once

#include "controllers/midi/midimessage.h"
#include "util/duration.h"

class MidiUtils {
  public:
    static inline uint8_t channelFromStatus(uint8_t status) {
        return status & 0x0F;
    }

    static inline uint8_t channelFromStatus(MidiOpCode status) {
        return channelFromStatus(static_cast<uint8_t>(status));
    }

    static inline MidiOpCode opCodeFromStatus(uint8_t status) {
        uint8_t opCode = status & 0xF0;
        // MidiOpCode::SystemExclusive and higher don't have a channel and occupy the entire byte.
        if (opCode == 0xF0) {
            opCode = status;
        }
        return static_cast<MidiOpCode>(opCode);
    }

    static inline uint8_t opCodeValue(MidiOpCode opCode) {
        return static_cast<uint8_t>(opCode);
    }

    static inline uint8_t statusFromOpCodeAndChannel(MidiOpCode opCode, uint8_t channel) {
        return opCodeValue(opCode) | (channel & 0x0F);
    }

    static inline bool isMessageTwoBytes(MidiOpCode opCode) {
        switch (opCode) {
        case MidiOpCode::SongSelect:
        case MidiOpCode::NoteOff:
        case MidiOpCode::NoteOn:
        case MidiOpCode::PolyphonicKeyPressure:
        case MidiOpCode::ControlChange:
            return true;
        default:
            return false;
        }
    }

    static inline bool isMessageTwoBytes(uint8_t status) {
        return isMessageTwoBytes(opCodeFromStatus(status));
    }

    static inline bool isClockSignal(const MidiKey& mappingKey) {
        return (mappingKey.key &
                       static_cast<uint16_t>(MidiOpCode::TimingClock)) ==
                static_cast<uint16_t>(MidiOpCode::TimingClock);
    }

    static QString opCodeToTranslatedString(MidiOpCode code);
    static QString formatByteAsHex(unsigned char value);
    static QString midiOptionToTranslatedString(MidiOption option);
    static QString formatMidiOpCode(const QString& controllerName,
            unsigned char status,
            unsigned char control,
            unsigned char value,
            unsigned char channel,
            MidiOpCode opCode,
            mixxx::Duration timestamp = mixxx::Duration::fromMillis(0));
    static QString formatSysexMessage(const QString& controllerName,
                              const QByteArray& data,
                              mixxx::Duration timestamp = mixxx::Duration::fromMillis(0));

    /// Strips the decorations that MIDI APIs add to a device's port name so
    /// the same hardware gets the same name on every platform, e.g.
    /// "2- DDJ-800" (Windows, second instance), "DDJ-800 MIDI 1" and
    /// "DDJ-800:DDJ-800 MIDI 1 20:0" (ALSA) all become "DDJ-800".
    static QString canonicalDeviceName(const QString& deviceName);

    template<class... Ts>
    struct overloaded : Ts... {
        using Ts::operator()...;
    };
#if __cplusplus < 202002L || defined(__clang__)
    // explicit deduction guide (not needed as of C++20)
    template<class... Ts>
    overloaded(Ts...) -> overloaded<Ts...>;
#endif
};
