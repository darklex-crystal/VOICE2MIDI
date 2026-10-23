#pragma once

#include "MidiNoteTracker.h"
#include "PitchDetector.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

class Vocal2MIDITuneAudioProcessor final : public juce::AudioProcessor
{
public:
    Vocal2MIDITuneAudioProcessor();
    ~Vocal2MIDITuneAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState parameters;
    void requestMidiPanic() noexcept { panicRequested.store (true); }

    std::atomic<float> detectedFrequency { 0.0f };
    std::atomic<float> detectionConfidence { 0.0f };
    std::atomic<float> inputRms { 0.0f };
    std::atomic<float> deviationCents { 0.0f };
    std::atomic<int> inputMidiNote { -1 };
    std::atomic<int> targetMidiNote { -1 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    vocal2midi::MidiNoteTracker::Config makeTrackerConfig() const noexcept;

    vocal2midi::PitchDetector pitchDetector;
    vocal2midi::MidiNoteTracker noteTracker;
    juce::SmoothedValue<float> outputGain;
    std::atomic<bool> panicRequested { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Vocal2MIDITuneAudioProcessor)
};
