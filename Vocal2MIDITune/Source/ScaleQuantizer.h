#pragma once

#include <cstddef>

namespace vocal2midi
{
enum class ScaleType
{
    chromatic = 0,
    major,
    naturalMinor,
    harmonicMinor,
    melodicMinor,
    majorPentatonic,
    minorPentatonic,
    blues,
    dorian,
    phrygian,
    lydian,
    mixolydian,
    locrian
};

struct ScalePattern
{
    const int* intervals = nullptr;
    std::size_t size = 0;
};

class ScaleQuantizer
{
public:
    static ScalePattern getPattern (ScaleType scale) noexcept;
    static bool isNoteInScale (int midiNote, int rootNote, ScaleType scale) noexcept;
    static int quantize (float midiNote, int rootNote, ScaleType scale, bool scaleLock = true) noexcept;
};
} // namespace vocal2midi
