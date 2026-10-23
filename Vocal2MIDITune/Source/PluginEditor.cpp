#include "PluginEditor.h"
#include "MidiUtils.h"
#include "ParameterIDs.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

namespace
{
using APVTS = juce::AudioProcessorValueTreeState;
const auto accent = juce::Colour (0xff68e0c2);
const auto muted = juce::Colour (0xff8fa4ba);
const auto panel = juce::Colour (0xff121a24);

class ControlPanel final : public juce::Component
{
public:
    explicit ControlPanel (APVTS& state, const juce::String& title, int columnCount = 2)
        : parameters (state), group (juce::String {}, title), columns (columnCount)
    {
        addAndMakeVisible (group);
    }

    void addSlider (const juce::String& id, const juce::String& text, const juce::String& suffix = {},
                    double displayFactor = 1.0, int decimals = 0)
    {
        auto* row = new SliderRow (parameters, id, text, suffix, displayFactor, decimals);
        rows.add (row);
        addAndMakeVisible (*row);
    }

    void addChoice (const juce::String& id, const juce::String& text, const juce::StringArray& choices)
    {
        auto* row = new ChoiceRow (parameters, id, text, choices);
        rows.add (row);
        addAndMakeVisible (*row);
    }

    void addToggle (const juce::String& id, const juce::String& text)
    {
        auto* row = new ToggleRow (parameters, id, text);
        rows.add (row);
        addAndMakeVisible (*row);
    }

    void resized() override
    {
        group.setBounds (getLocalBounds());
        auto area = getLocalBounds().reduced (14, 10);
        area.removeFromTop (28);
        const auto rowCount = juce::jmax (1, (rows.size() + columns - 1) / columns);
        const auto gap = 10;
        const auto cellWidth = (area.getWidth() - gap * (columns - 1)) / columns;
        const auto cellHeight = juce::jmin (56, area.getHeight() / rowCount);

        for (int i = 0; i < rows.size(); ++i)
        {
            const auto column = i % columns;
            const auto line = i / columns;
            rows[i]->setBounds (area.getX() + column * (cellWidth + gap),
                                area.getY() + line * cellHeight, cellWidth, cellHeight - 3);
        }
    }

private:
    struct Row : public juce::Component
    {
        ~Row() override = default;
    };

    struct SliderRow final : Row
    {
        SliderRow (APVTS& state, const juce::String& id, const juce::String& text,
                   const juce::String& suffix, double factor, int decimals)
        {
            label.setText (text, juce::dontSendNotification);
            label.setFont (juce::Font (11.5f));
            label.setColour (juce::Label::textColourId, muted);
            addAndMakeVisible (label);

            slider.setSliderStyle (juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
            slider.setTextValueSuffix (suffix);
            if (factor != 1.0 || decimals > 0)
            {
                slider.textFromValueFunction = [factor, decimals, suffix] (double value)
                {
                    return juce::String (value * factor, decimals) + suffix;
                };
                slider.valueFromTextFunction = [factor] (const juce::String& textValue)
                {
                    return textValue.getDoubleValue() / factor;
                };
            }
            addAndMakeVisible (slider);
            attachment = std::make_unique<APVTS::SliderAttachment> (state, id, slider);
        }

        void resized() override
        {
            label.setBounds (0, 0, getWidth(), 16);
            slider.setBounds (0, 17, getWidth(), juce::jmax (0, getHeight() - 17));
        }

        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<APVTS::SliderAttachment> attachment;
    };

    struct ChoiceRow final : Row
    {
        ChoiceRow (APVTS& state, const juce::String& id, const juce::String& text,
                   const juce::StringArray& choices)
        {
            label.setText (text, juce::dontSendNotification);
            label.setFont (juce::Font (11.5f));
            label.setColour (juce::Label::textColourId, muted);
            addAndMakeVisible (label);
            for (int i = 0; i < choices.size(); ++i)
                box.addItem (choices[i], i + 1);
            addAndMakeVisible (box);
            attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, id, box);
        }

        void resized() override
        {
            label.setBounds (0, 0, getWidth(), 16);
            box.setBounds (0, 19, getWidth(), 26);
        }

        juce::Label label;
        juce::ComboBox box;
        std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
    };

    struct ToggleRow final : Row
    {
        ToggleRow (APVTS& state, const juce::String& id, const juce::String& text)
            : toggle (text)
        {
            addAndMakeVisible (toggle);
            attachment = std::make_unique<APVTS::ButtonAttachment> (state, id, toggle);
        }

        void resized() override { toggle.setBounds (0, 15, getWidth(), 28); }

        juce::ToggleButton toggle;
        std::unique_ptr<APVTS::ButtonAttachment> attachment;
    };

    APVTS& parameters;
    juce::GroupComponent group;
    juce::OwnedArray<Row> rows;
    int columns = 2;
};

class MonitorPanel final : public juce::Component
{
public:
    explicit MonitorPanel (Vocal2MIDITuneAudioProcessor& owner) : processor (owner)
    {
        rawHistory.fill (-1.0f);
        targetHistory.fill (-1.0f);
    }

    void refresh()
    {
        std::move (rawHistory.begin() + 1, rawHistory.end(), rawHistory.begin());
        std::move (targetHistory.begin() + 1, targetHistory.end(), targetHistory.begin());
        const auto frequency = processor.detectedFrequency.load();
        rawHistory.back() = frequency > 0.0f ? vocal2midi::frequencyToMidiFloat (frequency) : -1.0f;
        targetHistory.back() = static_cast<float> (processor.targetMidiNote.load());
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().reduced (2);
        g.setColour (panel);
        g.fillRoundedRectangle (bounds.toFloat(), 12.0f);
        g.setColour (juce::Colour (0xff253142));
        g.drawRoundedRectangle (bounds.toFloat(), 12.0f, 1.0f);

        auto area = bounds.reduced (14);
        auto cards = area.removeFromTop (76);
        const auto gap = 10;
        const auto cardWidth = (cards.getWidth() - 3 * gap) / 4;

        const auto frequency = processor.detectedFrequency.load();
        const auto inputNote = processor.inputMidiNote.load();
        const auto targetNote = processor.targetMidiNote.load();
        const auto confidence = processor.detectionConfidence.load();
        const auto inputName = juce::String (vocal2midi::midiNoteName (inputNote));
        const auto targetName = targetNote >= 0
                              ? juce::String (vocal2midi::midiNoteName (targetNote)) + " / " + juce::String (targetNote)
                              : juce::String ("--");

        drawCard (g, cards.removeFromLeft (cardWidth), "DETECTED FREQUENCY",
                  frequency > 0.0f ? juce::String (frequency, 1) + " Hz" : "-- Hz", accent);
        cards.removeFromLeft (gap);
        drawCard (g, cards.removeFromLeft (cardWidth), "INPUT NOTE", inputName, juce::Colour (0xff8fb7ff));
        cards.removeFromLeft (gap);
        drawCard (g, cards.removeFromLeft (cardWidth), "TARGET MIDI NOTE", targetName, juce::Colour (0xffb8f57f));
        cards.removeFromLeft (gap);
        drawCard (g, cards.removeFromLeft (cardWidth), "CONFIDENCE",
                  juce::String (juce::roundToInt (confidence * 100.0f)) + " %", juce::Colour (0xffffc879));

        area.removeFromTop (10);
        auto lower = area;
        auto status = lower.removeFromLeft (250);
        lower.removeFromLeft (12);
        drawStatus (g, status);
        drawHistory (g, lower);
    }

private:
    static void drawCard (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title,
                          const juce::String& value, juce::Colour valueColour)
    {
        g.setColour (juce::Colour (0xff0e151e));
        g.fillRoundedRectangle (area.toFloat(), 8.0f);
        g.setColour (muted);
        g.setFont (juce::Font (10.5f));
        g.drawText (title, area.reduced (12, 8).removeFromTop (16), juce::Justification::centredLeft);
        g.setColour (valueColour);
        g.setFont (juce::Font (23.0f, juce::Font::bold));
        g.drawText (value, area.reduced (12, 4).removeFromBottom (34), juce::Justification::centredLeft);
    }

    void drawStatus (juce::Graphics& g, juce::Rectangle<int> area)
    {
        g.setColour (juce::Colour (0xff0e151e));
        g.fillRoundedRectangle (area.toFloat(), 8.0f);
        auto content = area.reduced (12, 10);
        g.setColour (muted);
        g.setFont (juce::Font (10.5f));
        g.drawText ("INPUT LEVEL", content.removeFromTop (15), juce::Justification::centredLeft);

        auto meter = content.removeFromTop (10).toFloat();
        g.setColour (juce::Colour (0xff253142));
        g.fillRoundedRectangle (meter, 4.0f);
        const auto db = 20.0f * std::log10 (std::max (processor.inputRms.load(), 1.0e-12f));
        const auto level = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        g.setColour (level > 0.9f ? juce::Colour (0xffff8b7c) : accent);
        g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * level), 4.0f);

        content.removeFromTop (10);
        const auto cents = processor.deviationCents.load();
        g.setColour (muted);
        g.drawText ("PITCH DEVIATION", content.removeFromTop (15), juce::Justification::centredLeft);
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (16.0f, juce::Font::bold));
        g.drawText ((cents >= 0.0f ? "+" : "") + juce::String (cents, 1) + " cents",
                    content, juce::Justification::centredLeft);
    }

    void drawHistory (juce::Graphics& g, juce::Rectangle<int> area)
    {
        g.setColour (juce::Colour (0xff0e151e));
        g.fillRoundedRectangle (area.toFloat(), 8.0f);
        auto graph = area.reduced (10, 8).toFloat();
        g.setColour (juce::Colour (0xff1c2936));
        for (int i = 1; i < 4; ++i)
        {
            const auto y = graph.getY() + graph.getHeight() * static_cast<float> (i) / 4.0f;
            g.drawHorizontalLine (juce::roundToInt (y), graph.getX(), graph.getRight());
        }

        const auto drawLine = [&] (const auto& history, juce::Colour colour, float thickness)
        {
            juce::Path path;
            auto started = false;
            for (std::size_t i = 0; i < history.size(); ++i)
            {
                const auto note = history[i];
                if (note < 20.0f)
                {
                    started = false;
                    continue;
                }
                const auto x = graph.getX() + graph.getWidth() * static_cast<float> (i) / static_cast<float> (history.size() - 1);
                const auto normalized = juce::jlimit (0.0f, 1.0f, (note - 36.0f) / 48.0f);
                const auto y = graph.getBottom() - normalized * graph.getHeight();
                if (! started)
                {
                    path.startNewSubPath (x, y);
                    started = true;
                }
                else
                    path.lineTo (x, y);
            }
            g.setColour (colour);
            g.strokePath (path, juce::PathStrokeType (thickness));
        };

        drawLine (rawHistory, juce::Colour (0xff8fb7ff), 1.3f);
        drawLine (targetHistory, accent, 2.0f);
    }

    Vocal2MIDITuneAudioProcessor& processor;
    std::array<float, 180> rawHistory;
    std::array<float, 180> targetHistory;
};

ControlPanel* addPanel (Vocal2MIDITuneAudioProcessorEditor& editor,
                        juce::OwnedArray<juce::Component>& panels,
                        APVTS& state, const juce::String& title, int columns = 2)
{
    auto* newPanel = new ControlPanel (state, title, columns);
    panels.add (newPanel);
    editor.addAndMakeVisible (*newPanel);
    return newPanel;
}
} // namespace

Vocal2MIDITuneAudioProcessorEditor::Vocal2MIDITuneAudioProcessorEditor (Vocal2MIDITuneAudioProcessor& owner)
    : AudioProcessorEditor (owner), audioProcessor (owner)
{
    setLookAndFeel (&lookAndFeel);
    setSize (1180, 920);

    titleLabel.setText ("VOCAL2MIDI TUNE", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (25.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, accent);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("MONOPHONIC VOICE-TO-MIDI ENGINE", juce::dontSendNotification);
    subtitleLabel.setFont (juce::Font (11.0f));
    subtitleLabel.setColour (juce::Label::textColourId, muted);
    addAndMakeVisible (subtitleLabel);

    versionLabel.setText ("VERSION 0.1.0  |  VST3 + STANDALONE", juce::dontSendNotification);
    versionLabel.setJustificationType (juce::Justification::centredRight);
    versionLabel.setColour (juce::Label::textColourId, muted);
    addAndMakeVisible (versionLabel);

    footerLabel.setText ("Voice to MIDI for Ableton Live", juce::dontSendNotification);
    footerLabel.setJustificationType (juce::Justification::centred);
    footerLabel.setColour (juce::Label::textColourId, muted);
    addAndMakeVisible (footerLabel);

    presetLabel.setText ("PRESET", juce::dontSendNotification);
    presetLabel.setColour (juce::Label::textColourId, muted);
    addAndMakeVisible (presetLabel);
    presetSelector.addItemList ({ "Natural Vocal MIDI", "Hard Tune MIDI", "808 Glide",
                                  "Expressive Synth Lead", "Piano Notes", "Bass Vocal" }, 1);
    presetSelector.setSelectedItemIndex (0, juce::dontSendNotification);
    addAndMakeVisible (presetSelector);
    addAndMakeVisible (applyPresetButton);
    applyPresetButton.onClick = [this] { applyPreset (presetSelector.getSelectedItemIndex()); };
    addAndMakeVisible (panicButton);
    panicButton.onClick = [this] { audioProcessor.requestMidiPanic(); };

    monitor = std::make_unique<MonitorPanel> (audioProcessor);
    addAndMakeVisible (*monitor);

    using namespace vocal2midi::param;
    auto& state = audioProcessor.parameters;
    auto* scale = addPanel (*this, panels, state, "SCALE & TRANSPOSITION");
    scale->addChoice (rootNote, "Root Note", { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" });
    scale->addChoice (scaleType, "Scale", { "Chromatic", "Major", "Natural Minor", "Harmonic Minor", "Melodic Minor",
                                            "Major Pentatonic", "Minor Pentatonic", "Blues", "Dorian", "Phrygian",
                                            "Lydian", "Mixolydian", "Locrian" });
    scale->addToggle (scaleLock, "Scale Lock");
    scale->addSlider (transpose, "Transpose", " st");
    scale->addSlider (octaveShift, "Octave Shift", " oct");

    auto* detection = addPanel (*this, panels, state, "DETECTION");
    detection->addSlider (inputThresholdDb, "Input Threshold", " dB", 1.0, 1);
    detection->addSlider (confidenceThreshold, "Confidence Threshold", " %", 100.0, 0);
    detection->addSlider (minFrequency, "Min Frequency", " Hz");
    detection->addSlider (maxFrequency, "Max Frequency", " Hz");
    detection->addSlider (noteStabilityMs, "Stability Time", " ms");
    detection->addSlider (noteReleaseMs, "Release Time", " ms");

    auto* midi = addPanel (*this, panels, state, "MIDI OUTPUT");
    midi->addSlider (midiChannel, "MIDI Channel");
    midi->addChoice (velocityMode, "Velocity Mode", { "Dynamic", "Fixed" });
    midi->addSlider (fixedVelocity, "Fixed Velocity");
    midi->addChoice (velocityCurve, "Velocity Curve", { "Linear", "Soft", "Hard" });
    midi->addSlider (minVelocity, "Min Velocity");
    midi->addSlider (maxVelocity, "Max Velocity");
    midi->addSlider (minMidiNote, "Min MIDI Note");
    midi->addSlider (maxMidiNote, "Max MIDI Note");
    midi->addToggle (legato, "Legato Mode");
    midi->addToggle (midiOutputEnabled, "MIDI Output Enabled");

    auto* glide = addPanel (*this, panels, state, "GLIDE / PORTAMENTO");
    glide->addToggle (glideEnabled, "Glide Enabled");
    glide->addChoice (glideMode, "Glide Mode", { "Off", "Legato Only", "Always" });
    glide->addSlider (glideTimeMs, "Glide Time", " ms");
    glide->addChoice (glideCurve, "Glide Curve", { "Linear", "Exponential", "Smooth" });
    glide->addSlider (glideMinInterval, "Minimum Interval", " st");

    auto* bend = addPanel (*this, panels, state, "VOCAL PITCH BEND");
    bend->addToggle (pitchBendEnabled, "Pitch Bend Enabled");
    bend->addChoice (pitchBendRange, "Pitch Bend Range", { "+/-2 semitones", "+/-3 semitones", "+/-5 semitones",
                                                            "+/-12 semitones", "+/-24 semitones" });
    bend->addSlider (pitchBendAmount, "Bend Amount", " %", 100.0, 0);
    bend->addSlider (pitchBendSmoothingMs, "Bend Smoothing", " ms");
    bend->addSlider (pitchBendDeadZoneCents, "Bend Dead Zone", " cents", 1.0, 1);

    auto* output = addPanel (*this, panels, state, "AUDIO OUTPUT");
    output->addSlider (outputGainDb, "Output Gain", " dB", 1.0, 1);
    output->addSlider (dryWet, "Dry/Wet (reserved)", " % wet", 100.0, 0);

    startTimerHz (30);
}

Vocal2MIDITuneAudioProcessorEditor::~Vocal2MIDITuneAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void Vocal2MIDITuneAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0c1118));
    g.setColour (accent.withAlpha (0.65f));
    g.fillRect (20, 72, getWidth() - 40, 1);
}

void Vocal2MIDITuneAudioProcessorEditor::resized()
{
    titleLabel.setBounds (20, 14, 400, 32);
    subtitleLabel.setBounds (21, 45, 430, 18);
    versionLabel.setBounds (getWidth() - 440, 24, 270, 24);
    panicButton.setBounds (getWidth() - 150, 20, 130, 30);
    monitor->setBounds (20, 84, getWidth() - 40, 200);

    presetLabel.setBounds (22, 296, 58, 28);
    presetSelector.setBounds (84, 294, 270, 30);
    applyPresetButton.setBounds (366, 294, 90, 30);

    panels[0]->setBounds (20, 338, 280, 320);
    panels[1]->setBounds (312, 338, 410, 320);
    panels[2]->setBounds (734, 338, 426, 320);
    panels[3]->setBounds (20, 670, 370, 204);
    panels[4]->setBounds (402, 670, 450, 204);
    panels[5]->setBounds (864, 670, 296, 204);
    footerLabel.setBounds (20, 888, getWidth() - 40, 20);
}

void Vocal2MIDITuneAudioProcessorEditor::timerCallback()
{
    if (auto* display = dynamic_cast<MonitorPanel*> (monitor.get()))
        display->refresh();
}

void Vocal2MIDITuneAudioProcessorEditor::applyPreset (int presetIndex)
{
    using namespace vocal2midi::param;
    auto set = [this] (const char* id, float value)
    {
        if (auto* parameter = audioProcessor.parameters.getParameter (id))
        {
            const auto range = audioProcessor.parameters.getParameterRange (id);
            parameter->setValueNotifyingHost (range.convertTo0to1 (value));
        }
    };

    // Natural Vocal MIDI is the common starting point; other presets override only their character controls.
    set (rootNote, 0); set (scaleType, 0); set (scaleLock, 0);
    set (transpose, 0); set (octaveShift, 0);
    set (inputThresholdDb, -60); set (confidenceThreshold, 0.65f);
    set (minFrequency, 50); set (maxFrequency, 1500);
    set (noteStabilityMs, 35); set (noteReleaseMs, 100);
    set (minMidiNote, 0); set (maxMidiNote, 127);
    set (velocityMode, 0); set (fixedVelocity, 100); set (minVelocity, 20); set (maxVelocity, 127);
    set (velocityCurve, 0); set (legato, 0); set (midiOutputEnabled, 1);
    set (glideEnabled, 0); set (glideMode, 0); set (glideTimeMs, 120);
    set (glideCurve, 0); set (glideMinInterval, 0);
    set (pitchBendEnabled, 1); set (pitchBendRange, 0); set (pitchBendAmount, 0.8f);
    set (pitchBendSmoothingMs, 35); set (pitchBendDeadZoneCents, 3);

    switch (presetIndex)
    {
        case 1: // Hard Tune MIDI
            set (scaleType, 1); set (scaleLock, 1); set (noteStabilityMs, 0); set (noteReleaseMs, 50);
            set (pitchBendEnabled, 0); set (pitchBendSmoothingMs, 5);
            break;
        case 2: // 808 Glide
            set (rootNote, 9); set (scaleType, 6); set (scaleLock, 1); set (octaveShift, -1);
            set (inputThresholdDb, -55); set (confidenceThreshold, 0.60f);
            set (noteStabilityMs, 15); set (noteReleaseMs, 80); set (legato, 1);
            set (glideEnabled, 1); set (glideMode, 2); set (glideTimeMs, 280); set (glideCurve, 2);
            set (pitchBendRange, 3); set (pitchBendAmount, 0.4f); set (pitchBendSmoothingMs, 25);
            break;
        case 3: // Expressive Synth Lead
            set (noteStabilityMs, 20); set (noteReleaseMs, 80); set (legato, 1);
            set (glideEnabled, 1); set (glideMode, 1); set (glideTimeMs, 90); set (glideCurve, 2);
            set (pitchBendRange, 2); set (pitchBendAmount, 1.0f);
            set (pitchBendSmoothingMs, 15); set (pitchBendDeadZoneCents, 1);
            break;
        case 4: // Piano Notes
            set (noteStabilityMs, 55); set (noteReleaseMs, 140);
            set (velocityCurve, 1); set (pitchBendEnabled, 0);
            break;
        case 5: // Bass Vocal
            set (rootNote, 9); set (scaleType, 6); set (scaleLock, 1); set (octaveShift, -1);
            set (minFrequency, 50); set (maxFrequency, 350); set (noteStabilityMs, 40); set (noteReleaseMs, 130);
            set (pitchBendAmount, 0.5f); set (pitchBendSmoothingMs, 45);
            break;
        case 0:
        default:
            break;
    }
}
