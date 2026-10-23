#pragma once

namespace vocal2midi::param
{
inline constexpr auto rootNote = "rootNote";
inline constexpr auto scaleType = "scaleType";
inline constexpr auto scaleLock = "scaleLock";
inline constexpr auto transpose = "transpose";
inline constexpr auto octaveShift = "octaveShift";
inline constexpr auto inputThresholdDb = "inputThresholdDb";
inline constexpr auto confidenceThreshold = "confidenceThreshold";
inline constexpr auto minFrequency = "minFrequency";
inline constexpr auto maxFrequency = "maxFrequency";
inline constexpr auto noteStabilityMs = "noteStabilityMs";
inline constexpr auto noteReleaseMs = "noteReleaseMs";
inline constexpr auto minMidiNote = "minMidiNote";
inline constexpr auto maxMidiNote = "maxMidiNote";
inline constexpr auto midiChannel = "midiChannel";
inline constexpr auto velocityMode = "velocityMode";
inline constexpr auto fixedVelocity = "fixedVelocity";
inline constexpr auto minVelocity = "minVelocity";
inline constexpr auto maxVelocity = "maxVelocity";
inline constexpr auto velocityCurve = "velocityCurve";
inline constexpr auto legato = "legato";
inline constexpr auto midiOutputEnabled = "midiOutputEnabled";
inline constexpr auto glideEnabled = "glideEnabled";
inline constexpr auto glideMode = "glideMode";
inline constexpr auto glideTimeMs = "glideTimeMs";
inline constexpr auto glideCurve = "glideCurve";
inline constexpr auto glideMinInterval = "glideMinInterval";
inline constexpr auto pitchBendEnabled = "pitchBendEnabled";
inline constexpr auto pitchBendRange = "pitchBendRange";
inline constexpr auto pitchBendAmount = "pitchBendAmount";
inline constexpr auto pitchBendSmoothingMs = "pitchBendSmoothingMs";
inline constexpr auto pitchBendDeadZoneCents = "pitchBendDeadZoneCents";
inline constexpr auto outputGainDb = "outputGainDb";
inline constexpr auto dryWet = "dryWet";
} // namespace vocal2midi::param
