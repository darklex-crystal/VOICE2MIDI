#pragma once

#include <algorithm>
#include <cmath>
#include <string>

namespace vocal2midi
{
inline constexpr float concertA4Hz = 440.0f;
inline constexpr int concertA4MidiNote = 69;

inline float midiNoteToFrequency (float midiNote)
{
    return concertA4Hz * std::pow (2.0f, (midiNote - static_cast<float> (concertA4MidiNote)) / 12.0f);
}

inline float frequencyToMidiFloat (float frequency)
{
    if (! std::isfinite (frequency) || frequency <= 0.0f)
        return 0.0f;

    return static_cast<float> (concertA4MidiNote)
         + 12.0f * std::log2 (frequency / concertA4Hz);
}

inline int frequencyToMidiNote (float frequency)
{
    if (! std::isfinite (frequency) || frequency <= 0.0f)
        return -1;

    return std::clamp (static_cast<int> (std::lround (frequencyToMidiFloat (frequency))), 0, 127);
}

inline std::string midiNoteName (int midiNote)
{
    if (midiNote < 0 || midiNote > 127)
        return "--";

    static constexpr const char* names[] = { "C", "C#", "D", "D#", "E", "F",
                                             "F#", "G", "G#", "A", "A#", "B" };
    return std::string (names[midiNote % 12]) + std::to_string (midiNote / 12 - 1);
}

inline int pitchBendTo14Bit (float semitones, float rangeSemitones)
{
    if (! std::isfinite (semitones) || rangeSemitones <= 0.0f)
        return 8192;

    const auto normalised = std::clamp (semitones / rangeSemitones, -1.0f, 1.0f);
    const auto value = normalised >= 0.0f
                     ? 8192.0f + normalised * 8191.0f
                     : 8192.0f + normalised * 8192.0f;
    return std::clamp (static_cast<int> (std::lround (value)), 0, 16383);
}
} // namespace vocal2midi
