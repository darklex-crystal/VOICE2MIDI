#pragma once

#include "PitchDetector.h"
#include "ScaleQuantizer.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace vocal2midi
{
class MidiNoteTracker
{
public:
    struct Config
    {
        int rootNote = 0;
        ScaleType scale = ScaleType::chromatic;
        bool scaleLock = false;
        int transpose = 0;
        int octaveShift = 0;
        float inputThresholdDb = -60.0f;
        float confidenceThreshold = 0.65f;
        float stabilityMs = 35.0f;
        float releaseMs = 100.0f;
        int minMidiNote = 0;
        int maxMidiNote = 127;
        int channel = 1;
        int velocityMode = 0; // 0 dynamic, 1 fixed
        int fixedVelocity = 100;
        int minVelocity = 1;
        int maxVelocity = 127;
        int velocityCurve = 0; // 0 linear, 1 soft, 2 hard
        bool legato = false;
        bool outputEnabled = true;
        bool glideEnabled = false;
        int glideMode = 0; // 0 off, 1 legato only, 2 always
        float glideTimeMs = 120.0f;
        int glideCurve = 0; // 0 linear, 1 exponential, 2 smooth
        int glideMinInterval = 0;
        bool pitchBendEnabled = true;
        float pitchBendRange = 2.0f;
        float pitchBendAmount = 1.0f;
        float pitchBendSmoothingMs = 30.0f;
        float pitchBendDeadZoneCents = 3.0f;
    };

    void prepare (double newSampleRate) noexcept;
    void reset() noexcept;
    void panic (juce::MidiBuffer& midi, int sampleOffset = 0);

    void update (const PitchDetector::Result& pitch,
                 const Config& config,
                 int elapsedSamples,
                 int sampleOffset,
                 juce::MidiBuffer& midi);

    int getActiveNote() const noexcept { return activeNote; }
    float getLastDeviationCents() const noexcept { return lastDeviationCents; }

private:
    void startNote (int note, const PitchDetector::Result& pitch, float rawMidi,
                    const Config& config, int sampleOffset, juce::MidiBuffer& midi);
    void stopActiveNote (const Config& config, int sampleOffset, juce::MidiBuffer& midi);
    int calculateVelocity (const PitchDetector::Result& pitch, const Config& config) const noexcept;
    void updatePitchBend (float rawMidi, const Config& config, float elapsedMs,
                          int sampleOffset, juce::MidiBuffer& midi, bool force = false);
    void sendPitchBend (float semitones, const Config& config, int sampleOffset,
                        juce::MidiBuffer& midi, bool force);
    void centrePitchBend (int channel, int sampleOffset, juce::MidiBuffer& midi, bool force = false);

    double sampleRate = 48000.0;
    int activeNote = -1;
    int activeChannel = 1;
    int candidateNote = -1;
    int lastNote = -1;
    float candidateMs = 0.0f;
    float unvoicedMs = 0.0f;
    float vocalBendSemitones = 0.0f;
    float lastDeviationCents = 0.0f;
    int lastSentBend = 8192;
    float bendSendAccumulatorMs = 1000.0f;

    bool glideActive = false;
    float glideElapsedMs = 0.0f;
    float glideDurationMs = 0.0f;
    float glideStartSemitones = 0.0f;
    float glideCurrentSemitones = 0.0f;
    int glideCurve = 0;
};
} // namespace vocal2midi
