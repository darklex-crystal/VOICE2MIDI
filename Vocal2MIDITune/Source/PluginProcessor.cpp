#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "MidiUtils.h"
#include "ParameterIDs.h"

#include <array>

namespace
{
using ParameterLayout = juce::AudioProcessorValueTreeState::ParameterLayout;

std::unique_ptr<juce::AudioParameterFloat> floatParameter (const juce::String& id, const juce::String& name,
                                                            float minimum, float maximum, float initial,
                                                            float interval = 0.0f)
{
    return std::make_unique<juce::AudioParameterFloat> (
        id, name, juce::NormalisableRange<float> (minimum, maximum, interval), initial);
}

std::unique_ptr<juce::AudioParameterInt> intParameter (const juce::String& id, const juce::String& name,
                                                        int minimum, int maximum, int initial)
{
    return std::make_unique<juce::AudioParameterInt> (id, name, minimum, maximum, initial);
}
} // namespace

Vocal2MIDITuneAudioProcessor::Vocal2MIDITuneAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

ParameterLayout Vocal2MIDITuneAudioProcessor::createParameterLayout()
{
    using namespace vocal2midi::param;
    ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        rootNote, "Root Note", juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        scaleType, "Scale", juce::StringArray { "Chromatic", "Major", "Natural Minor", "Harmonic Minor",
                                                 "Melodic Minor", "Major Pentatonic", "Minor Pentatonic", "Blues",
                                                 "Dorian", "Phrygian", "Lydian", "Mixolydian", "Locrian" }, 0));
    layout.add (std::make_unique<juce::AudioParameterBool> (scaleLock, "Scale Lock", false));
    layout.add (intParameter (transpose, "Transpose", -24, 24, 0));
    layout.add (intParameter (octaveShift, "Octave Shift", -2, 2, 0));

    layout.add (floatParameter (inputThresholdDb, "Input Threshold", -80.0f, 0.0f, -60.0f, 0.1f));
    layout.add (floatParameter (confidenceThreshold, "Confidence Threshold", 0.0f, 1.0f, 0.65f, 0.01f));
    layout.add (floatParameter (minFrequency, "Minimum Frequency", 50.0f, 1500.0f, 50.0f, 1.0f));
    layout.add (floatParameter (maxFrequency, "Maximum Frequency", 50.0f, 1500.0f, 1500.0f, 1.0f));
    layout.add (floatParameter (noteStabilityMs, "Note Stability Time", 0.0f, 500.0f, 35.0f, 1.0f));
    layout.add (floatParameter (noteReleaseMs, "Note Release Time", 0.0f, 1000.0f, 100.0f, 1.0f));

    layout.add (intParameter (minMidiNote, "Minimum MIDI Note", 0, 127, 0));
    layout.add (intParameter (maxMidiNote, "Maximum MIDI Note", 0, 127, 127));
    layout.add (intParameter (midiChannel, "MIDI Channel", 1, 16, 1));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        velocityMode, "Velocity Mode", juce::StringArray { "Dynamic", "Fixed" }, 0));
    layout.add (intParameter (fixedVelocity, "Fixed Velocity", 1, 127, 100));
    layout.add (intParameter (minVelocity, "Minimum Velocity", 1, 127, 20));
    layout.add (intParameter (maxVelocity, "Maximum Velocity", 1, 127, 127));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        velocityCurve, "Velocity Curve", juce::StringArray { "Linear", "Soft", "Hard" }, 0));
    layout.add (std::make_unique<juce::AudioParameterBool> (legato, "Legato", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (midiOutputEnabled, "MIDI Output Enabled", true));

    layout.add (std::make_unique<juce::AudioParameterBool> (glideEnabled, "Glide Enabled", false));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        glideMode, "Glide Mode", juce::StringArray { "Off", "Legato Only", "Always" }, 0));
    layout.add (floatParameter (glideTimeMs, "Glide Time", 0.0f, 1000.0f, 120.0f, 1.0f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        glideCurve, "Glide Curve", juce::StringArray { "Linear", "Exponential", "Smooth" }, 0));
    layout.add (intParameter (glideMinInterval, "Glide Minimum Interval", 0, 24, 0));

    layout.add (std::make_unique<juce::AudioParameterBool> (pitchBendEnabled, "Pitch Bend Enabled", true));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        pitchBendRange, "Pitch Bend Range", juce::StringArray { "+/-2 semitones", "+/-3 semitones", "+/-5 semitones",
                                                                 "+/-12 semitones", "+/-24 semitones" }, 0));
    layout.add (floatParameter (pitchBendAmount, "Bend Amount", 0.0f, 1.0f, 1.0f, 0.01f));
    layout.add (floatParameter (pitchBendSmoothingMs, "Bend Smoothing", 0.0f, 300.0f, 30.0f, 1.0f));
    layout.add (floatParameter (pitchBendDeadZoneCents, "Bend Dead Zone", 0.0f, 100.0f, 3.0f, 0.1f));

    layout.add (floatParameter (outputGainDb, "Output Gain", -24.0f, 24.0f, 0.0f, 0.1f));
    layout.add (floatParameter (dryWet, "Dry/Wet", 0.0f, 1.0f, 0.0f, 0.01f));
    return layout;
}

void Vocal2MIDITuneAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    pitchDetector.prepare (sampleRate);
    noteTracker.prepare (sampleRate);
    outputGain.reset (sampleRate, 0.02);
    outputGain.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (parameters.getRawParameterValue (vocal2midi::param::outputGainDb)->load()));
}

void Vocal2MIDITuneAudioProcessor::releaseResources()
{
}

bool Vocal2MIDITuneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& input = layouts.getMainInputChannelSet();
    const auto& output = layouts.getMainOutputChannelSet();
    if (input != output)
        return false;
    return output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo();
}

vocal2midi::MidiNoteTracker::Config Vocal2MIDITuneAudioProcessor::makeTrackerConfig() const noexcept
{
    using namespace vocal2midi::param;
    vocal2midi::MidiNoteTracker::Config config;
    const auto value = [this] (const char* id) { return parameters.getRawParameterValue (id)->load(); };

    config.rootNote = static_cast<int> (value (rootNote));
    config.scale = static_cast<vocal2midi::ScaleType> (juce::jlimit (0, 12, static_cast<int> (value (scaleType))));
    config.scaleLock = value (scaleLock) >= 0.5f;
    config.transpose = static_cast<int> (value (transpose));
    config.octaveShift = static_cast<int> (value (octaveShift));
    config.inputThresholdDb = value (inputThresholdDb);
    config.confidenceThreshold = value (confidenceThreshold);
    config.stabilityMs = value (noteStabilityMs);
    config.releaseMs = value (noteReleaseMs);
    config.minMidiNote = static_cast<int> (value (minMidiNote));
    config.maxMidiNote = static_cast<int> (value (maxMidiNote));
    config.channel = juce::jlimit (1, 16, static_cast<int> (value (midiChannel)));
    config.velocityMode = static_cast<int> (value (velocityMode));
    config.fixedVelocity = static_cast<int> (value (fixedVelocity));
    config.minVelocity = static_cast<int> (value (minVelocity));
    config.maxVelocity = static_cast<int> (value (maxVelocity));
    config.velocityCurve = static_cast<int> (value (velocityCurve));
    config.legato = value (legato) >= 0.5f;
    config.outputEnabled = value (midiOutputEnabled) >= 0.5f;
    config.glideEnabled = value (glideEnabled) >= 0.5f;
    config.glideMode = static_cast<int> (value (glideMode));
    config.glideTimeMs = value (glideTimeMs);
    config.glideCurve = static_cast<int> (value (glideCurve));
    config.glideMinInterval = static_cast<int> (value (glideMinInterval));
    config.pitchBendEnabled = value (pitchBendEnabled) >= 0.5f;
    constexpr float bendRanges[] = { 2.0f, 3.0f, 5.0f, 12.0f, 24.0f };
    config.pitchBendRange = bendRanges[juce::jlimit (0, 4, static_cast<int> (value (pitchBendRange)))];
    config.pitchBendAmount = value (pitchBendAmount);
    config.pitchBendSmoothingMs = value (pitchBendSmoothingMs);
    config.pitchBendDeadZoneCents = value (pitchBendDeadZoneCents);
    return config;
}

void Vocal2MIDITuneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    midiMessages.clear(); // This V1 generates MIDI rather than forwarding incoming MIDI.
    if (panicRequested.exchange (false))
    {
        noteTracker.panic (midiMessages);
        targetMidiNote.store (-1);
        deviationCents.store (0.0f);
    }
    if (buffer.getNumSamples() == 0)
        return;

    const auto config = makeTrackerConfig();
    pitchDetector.setFrequencyRange (parameters.getRawParameterValue (vocal2midi::param::minFrequency)->load(),
                                     parameters.getRawParameterValue (vocal2midi::param::maxFrequency)->load());

    const auto channelsToMix = juce::jlimit (1, 2, buffer.getNumChannels());
    auto samplesSinceUpdate = 0;

    for (auto sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        auto mono = buffer.getReadPointer (0)[sample];
        if (channelsToMix == 2)
            mono = 0.5f * (mono + buffer.getReadPointer (1)[sample]);

        ++samplesSinceUpdate;
        if (pitchDetector.processSample (mono))
        {
            const auto& pitch = pitchDetector.getLastResult();
            noteTracker.update (pitch, config, samplesSinceUpdate, sample, midiMessages);
            samplesSinceUpdate = 0;

            detectedFrequency.store (pitch.frequencyHz);
            detectionConfidence.store (pitch.confidence);
            inputRms.store (pitch.rms);
            inputMidiNote.store (vocal2midi::frequencyToMidiNote (pitch.frequencyHz));
            targetMidiNote.store (noteTracker.getActiveNote());
            deviationCents.store (noteTracker.getLastDeviationCents());
        }
    }

    // V1 is transparent apart from output gain. Dry/Wet is reserved for the future corrected-audio path.
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (
        parameters.getRawParameterValue (vocal2midi::param::outputGainDb)->load()));
    outputGain.applyGain (buffer, buffer.getNumSamples());
}

void Vocal2MIDITuneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = parameters.copyState();
    const std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr)
        copyXmlToBinary (*xml, destData);
}

void Vocal2MIDITuneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (parameters.state.getType()))
        parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* Vocal2MIDITuneAudioProcessor::createEditor()
{
    return new Vocal2MIDITuneAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Vocal2MIDITuneAudioProcessor();
}
