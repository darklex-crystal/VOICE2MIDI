#include "ScaleQuantizer.h"

#include <algorithm>
#include <cmath>

namespace vocal2midi
{
namespace
{
constexpr int chromatic[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
constexpr int major[] = { 0, 2, 4, 5, 7, 9, 11 };
constexpr int naturalMinor[] = { 0, 2, 3, 5, 7, 8, 10 };
constexpr int harmonicMinor[] = { 0, 2, 3, 5, 7, 8, 11 };
constexpr int melodicMinor[] = { 0, 2, 3, 5, 7, 9, 11 };
constexpr int majorPentatonic[] = { 0, 2, 4, 7, 9 };
constexpr int minorPentatonic[] = { 0, 3, 5, 7, 10 };
constexpr int blues[] = { 0, 3, 5, 6, 7, 10 };
constexpr int dorian[] = { 0, 2, 3, 5, 7, 9, 10 };
constexpr int phrygian[] = { 0, 1, 3, 5, 7, 8, 10 };
constexpr int lydian[] = { 0, 2, 4, 6, 7, 9, 11 };
constexpr int mixolydian[] = { 0, 2, 4, 5, 7, 9, 10 };
constexpr int locrian[] = { 0, 1, 3, 5, 6, 8, 10 };

template <std::size_t size>
ScalePattern makePattern (const int (&values)[size]) noexcept
{
    return { values, size };
}
} // namespace

ScalePattern ScaleQuantizer::getPattern (ScaleType scale) noexcept
{
    switch (scale)
    {
        case ScaleType::chromatic:       return makePattern (chromatic);
        case ScaleType::major:           return makePattern (major);
        case ScaleType::naturalMinor:    return makePattern (naturalMinor);
        case ScaleType::harmonicMinor:   return makePattern (harmonicMinor);
        case ScaleType::melodicMinor:    return makePattern (melodicMinor);
        case ScaleType::majorPentatonic: return makePattern (majorPentatonic);
        case ScaleType::minorPentatonic: return makePattern (minorPentatonic);
        case ScaleType::blues:           return makePattern (blues);
        case ScaleType::dorian:          return makePattern (dorian);
        case ScaleType::phrygian:        return makePattern (phrygian);
        case ScaleType::lydian:          return makePattern (lydian);
        case ScaleType::mixolydian:      return makePattern (mixolydian);
        case ScaleType::locrian:         return makePattern (locrian);
    }

    return makePattern (chromatic);
}

bool ScaleQuantizer::isNoteInScale (int midiNote, int rootNote, ScaleType scale) noexcept
{
    const auto pattern = getPattern (scale);
    const auto pitchClass = ((midiNote - rootNote) % 12 + 12) % 12;

    for (std::size_t i = 0; i < pattern.size; ++i)
        if (pattern.intervals[i] == pitchClass)
            return true;

    return false;
}

int ScaleQuantizer::quantize (float midiNote, int rootNote, ScaleType scale, bool scaleLock) noexcept
{
    if (! std::isfinite (midiNote))
        return 60;

    const auto clamped = std::clamp (midiNote, 0.0f, 127.0f);
    const auto nearest = static_cast<int> (std::lround (clamped));

    if (! scaleLock || scale == ScaleType::chromatic)
        return nearest;

    const auto first = std::max (0, nearest - 12);
    const auto last = std::min (127, nearest + 12);
    auto bestNote = nearest;
    auto bestDistance = 1000.0f;

    for (auto candidate = first; candidate <= last; ++candidate)
    {
        if (! isNoteInScale (candidate, rootNote, scale))
            continue;

        const auto distance = std::abs (clamped - static_cast<float> (candidate));
        if (distance < bestDistance - 0.0001f
            || (std::abs (distance - bestDistance) <= 0.0001f && candidate > bestNote))
        {
            bestDistance = distance;
            bestNote = candidate;
        }
    }

    return bestNote;
}
} // namespace vocal2midi
