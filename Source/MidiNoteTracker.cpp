#include "MidiNoteTracker.h"
#include "MidiUtils.h"

#include <algorithm>
#include <cmath>

namespace vocal2midi
{
void MidiNoteTracker::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 1000.0 ? newSampleRate : 48000.0;
    reset();
}

void MidiNoteTracker::reset() noexcept
{
    activeNote = -1;
    activeChannel = 1;
    candidateNote = -1;
    lastNote = -1;
    candidateMs = 0.0f;
    unvoicedMs = 0.0f;
    vocalBendSemitones = 0.0f;
    lastDeviationCents = 0.0f;
    lastSentBend = 8192;
    bendSendAccumulatorMs = 1000.0f;
    glideActive = false;
    glideElapsedMs = 0.0f;
    glideDurationMs = 0.0f;
    glideStartSemitones = 0.0f;
    glideCurrentSemitones = 0.0f;
}

void MidiNoteTracker::panic (juce::MidiBuffer& midi, int sampleOffset)
{
    if (activeNote >= 0)
        midi.addEvent (juce::MidiMessage::noteOff (activeChannel, activeNote), sampleOffset);
    activeNote = -1;
    candidateNote = -1;
    centrePitchBend (activeChannel, sampleOffset, midi, true);
    glideActive = false;
    glideCurrentSemitones = 0.0f;
}

int MidiNoteTracker::calculateVelocity (const PitchDetector::Result& pitch, const Config& config) const noexcept
{
    const auto low = std::max (1, std::min (config.minVelocity, config.maxVelocity));
    const auto high = std::min (127, std::max (config.minVelocity, config.maxVelocity));
    if (config.velocityMode == 1)
        return std::clamp (config.fixedVelocity, low, high);

    const auto db = 20.0f * std::log10 (std::max (pitch.rms, 1.0e-12f));
    auto amount = std::clamp ((db - config.inputThresholdDb) / std::max (1.0f, -config.inputThresholdDb), 0.0f, 1.0f);
    if (config.velocityCurve == 1)
        amount = std::sqrt (amount);
    else if (config.velocityCurve == 2)
        amount = amount * amount;

    return std::clamp (static_cast<int> (std::lround (static_cast<float> (low)
                       + amount * static_cast<float> (high - low))), 1, 127);
}

void MidiNoteTracker::sendPitchBend (float semitones, const Config& config, int sampleOffset,
                                     juce::MidiBuffer& midi, bool force)
{
    const auto value = pitchBendTo14Bit (semitones, config.pitchBendRange);
    if (! force && std::abs (value - lastSentBend) < 4)
        return;

    const auto channel = activeNote >= 0 ? activeChannel : config.channel;
    midi.addEvent (juce::MidiMessage::pitchWheel (channel, value), sampleOffset);
    lastSentBend = value;
}

void MidiNoteTracker::centrePitchBend (int channel, int sampleOffset, juce::MidiBuffer& midi, bool force)
{
    if (! force && lastSentBend == 8192)
        return;
    midi.addEvent (juce::MidiMessage::pitchWheel (channel, 8192), sampleOffset);
    lastSentBend = 8192;
}

void MidiNoteTracker::startNote (int note, const PitchDetector::Result& pitch, float rawMidi,
                                 const Config& config, int sampleOffset, juce::MidiBuffer& midi)
{
    const auto oldNote = activeNote;
    const auto oldChannel = activeChannel;
    const auto sourceNote = oldNote >= 0 ? oldNote : lastNote;
    const auto modeAllowsGlide = config.glideMode == 2 || (config.glideMode == 1 && oldNote >= 0);
    const auto interval = sourceNote >= 0 ? std::abs (sourceNote - note) : 0;

    glideActive = config.glideEnabled && config.glideMode != 0 && modeAllowsGlide
               && sourceNote >= 0 && config.glideTimeMs > 0.0f
               && interval >= config.glideMinInterval && interval > 0;
    glideElapsedMs = 0.0f;
    glideDurationMs = config.glideTimeMs;
    glideCurve = config.glideCurve;
    glideStartSemitones = glideActive
                        ? std::clamp (static_cast<float> (sourceNote - note),
                                      -0.95f * config.pitchBendRange, 0.95f * config.pitchBendRange)
                        : 0.0f;
    glideCurrentSemitones = glideStartSemitones;
    vocalBendSemitones = 0.0f;

    if (glideActive)
    {
        // Set the old pitch before Note On so the new note begins at the previous pitch.
        midi.addEvent (juce::MidiMessage::pitchWheel (config.channel,
                       pitchBendTo14Bit (glideCurrentSemitones, config.pitchBendRange)), sampleOffset);
        lastSentBend = pitchBendTo14Bit (glideCurrentSemitones, config.pitchBendRange);
    }
    else
    {
        centrePitchBend (config.channel, sampleOffset, midi);
    }

    const auto velocity = static_cast<juce::uint8> (calculateVelocity (pitch, config));
    const auto noteOn = juce::MidiMessage::noteOn (config.channel, note, velocity);
    if (oldNote >= 0 && config.legato)
    {
        // Overlap by one event: target instruments can interpret this as a legato transition.
        midi.addEvent (noteOn, sampleOffset);
        midi.addEvent (juce::MidiMessage::noteOff (oldChannel, oldNote), sampleOffset);
    }
    else
    {
        if (oldNote >= 0)
            midi.addEvent (juce::MidiMessage::noteOff (oldChannel, oldNote), sampleOffset);
        midi.addEvent (noteOn, sampleOffset);
    }

    activeNote = note;
    activeChannel = config.channel;
    lastNote = note;
    candidateNote = -1;
    candidateMs = 0.0f;
    unvoicedMs = 0.0f;
    lastDeviationCents = (rawMidi - static_cast<float> (note)) * 100.0f;
    bendSendAccumulatorMs = 1000.0f;
}

void MidiNoteTracker::stopActiveNote (const Config& config, int sampleOffset, juce::MidiBuffer& midi)
{
    juce::ignoreUnused (config);
    const auto channel = activeChannel;
    if (activeNote >= 0)
    {
        midi.addEvent (juce::MidiMessage::noteOff (channel, activeNote), sampleOffset);
        lastNote = activeNote;
    }
    activeNote = -1;
    candidateNote = -1;
    candidateMs = 0.0f;
    vocalBendSemitones = 0.0f;
    glideActive = false;
    glideCurrentSemitones = 0.0f;
    lastDeviationCents = 0.0f;
    centrePitchBend (channel, sampleOffset, midi);
}

void MidiNoteTracker::updatePitchBend (float rawMidi, const Config& config, float elapsedMs,
                                       int sampleOffset, juce::MidiBuffer& midi, bool force)
{
    if (glideActive)
    {
        glideElapsedMs += elapsedMs;
        const auto progress = std::clamp (glideElapsedMs / std::max (1.0f, glideDurationMs), 0.0f, 1.0f);
        auto remaining = 1.0f - progress;
        if (glideCurve == 1)
        {
            const auto e = std::exp (-5.0f * progress);
            const auto end = std::exp (-5.0f);
            remaining = std::clamp ((e - end) / (1.0f - end), 0.0f, 1.0f);
        }
        else if (glideCurve == 2)
        {
            remaining = 1.0f - progress * progress * (3.0f - 2.0f * progress);
        }
        glideCurrentSemitones = glideStartSemitones * remaining;
        if (progress >= 1.0f)
        {
            glideActive = false;
            glideCurrentSemitones = 0.0f;
        }
    }

    auto desiredBend = 0.0f;
    if (config.pitchBendEnabled && activeNote >= 0 && candidateNote < 0)
    {
        const auto deviation = rawMidi - static_cast<float> (activeNote);
        const auto deadZone = config.pitchBendDeadZoneCents / 100.0f;
        const auto magnitude = std::max (0.0f, std::abs (deviation) - deadZone);
        desiredBend = std::copysign (magnitude, deviation) * config.pitchBendAmount;
        desiredBend = std::clamp (desiredBend, -config.pitchBendRange, config.pitchBendRange);
    }

    const auto smoothing = std::max (0.0f, config.pitchBendSmoothingMs);
    const auto coefficient = smoothing <= 0.0f ? 1.0f : 1.0f - std::exp (-elapsedMs / smoothing);
    vocalBendSemitones += (desiredBend - vocalBendSemitones) * coefficient;
    bendSendAccumulatorMs += elapsedMs;

    constexpr float minimumSendIntervalMs = 10.0f; // At most 100 pitch-bend messages per second.
    if (force || bendSendAccumulatorMs >= minimumSendIntervalMs)
    {
        sendPitchBend (vocalBendSemitones + glideCurrentSemitones, config, sampleOffset, midi, force);
        bendSendAccumulatorMs = 0.0f;
    }
}

void MidiNoteTracker::update (const PitchDetector::Result& pitch, const Config& config,
                              int elapsedSamples, int sampleOffset, juce::MidiBuffer& midi)
{
    const auto elapsedMs = static_cast<float> (std::max (0, elapsedSamples)) * 1000.0f
                         / static_cast<float> (sampleRate);

    if (! config.outputEnabled)
    {
        if (activeNote >= 0)
            stopActiveNote (config, sampleOffset, midi);
        else
            centrePitchBend (config.channel, sampleOffset, midi);
        candidateNote = -1;
        candidateMs = 0.0f;
        return;
    }

    const auto inputDb = 20.0f * std::log10 (std::max (pitch.rms, 1.0e-12f));
    const auto voiced = pitch.voiceDetected && pitch.frequencyHz > 0.0f
                     && pitch.confidence >= config.confidenceThreshold
                     && inputDb >= config.inputThresholdDb;

    if (! voiced)
    {
        candidateNote = -1;
        candidateMs = 0.0f;
        unvoicedMs += elapsedMs;

        if (activeNote >= 0 && unvoicedMs >= config.releaseMs)
            stopActiveNote (config, sampleOffset, midi);
        else if (activeNote < 0)
            centrePitchBend (config.channel, sampleOffset, midi);
        return;
    }

    unvoicedMs = 0.0f;
    const auto rawMidi = frequencyToMidiFloat (pitch.frequencyHz)
                       + static_cast<float> (config.transpose + 12 * config.octaveShift);
    const auto lowNote = std::min (config.minMidiNote, config.maxMidiNote);
    const auto highNote = std::max (config.minMidiNote, config.maxMidiNote);
    const auto quantised = ScaleQuantizer::quantize (rawMidi, config.rootNote, config.scale, config.scaleLock);
    const auto targetNote = std::clamp (quantised, lowNote, highNote);
    lastDeviationCents = (rawMidi - static_cast<float> (targetNote)) * 100.0f;

    if (targetNote != activeNote)
    {
        if (candidateNote != targetNote)
        {
            candidateNote = targetNote;
            candidateMs = 0.0f;
        }
        else
        {
            candidateMs += elapsedMs;
        }

        if (candidateMs + 0.001f >= config.stabilityMs)
            startNote (targetNote, pitch, rawMidi, config, sampleOffset, midi);
    }
    else
    {
        candidateNote = -1;
        candidateMs = 0.0f;
    }

    if (activeNote >= 0)
        updatePitchBend (rawMidi, config, elapsedMs, sampleOffset, midi);
}
} // namespace vocal2midi
