#pragma once

#include "LookAndFeel.h"
#include "PluginProcessor.h"

#include <juce_gui_extra/juce_gui_extra.h>

class Vocal2MIDITuneAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                 private juce::Timer
{
public:
    explicit Vocal2MIDITuneAudioProcessorEditor (Vocal2MIDITuneAudioProcessor&);
    ~Vocal2MIDITuneAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void applyPreset (int presetIndex);

    Vocal2MIDITuneAudioProcessor& audioProcessor;
    vocal2midi::TuneLookAndFeel lookAndFeel;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label versionLabel;
    juce::Label footerLabel;
    juce::Label presetLabel;
    juce::ComboBox presetSelector;
    juce::TextButton applyPresetButton { "APPLY" };
    juce::TextButton panicButton { "MIDI PANIC" };

    std::unique_ptr<juce::Component> monitor;
    juce::OwnedArray<juce::Component> panels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Vocal2MIDITuneAudioProcessorEditor)
};
