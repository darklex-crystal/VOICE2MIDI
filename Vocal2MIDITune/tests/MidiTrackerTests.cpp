#include "../Source/MidiNoteTracker.h"
#include "../Source/MidiUtils.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
int failures = 0;

void expect (bool condition, const std::string& message)
{
    std::cout << (condition ? "PASS: " : "FAIL: ") << message << '\n';
    if (! condition) ++failures;
}

struct Events
{
    int noteOn = 0;
    int noteOff = 0;
    int bendCentre = 0;
    int lastNoteOn = -1;
    int lastNoteOff = -1;
};

Events readEvents (const juce::MidiBuffer& buffer)
{
    Events events;
    for (const auto item : buffer)
    {
        const auto message = item.getMessage();
        if (message.isNoteOn()) { ++events.noteOn; events.lastNoteOn = message.getNoteNumber(); }
        if (message.isNoteOff()) { ++events.noteOff; events.lastNoteOff = message.getNoteNumber(); }
        if (message.isPitchWheel() && message.getPitchWheelValue() == 8192) ++events.bendCentre;
    }
    return events;
}

vocal2midi::PitchDetector::Result pitched (float frequency)
{
    return { frequency, 0.99f, 0.4f, 0.6f, true };
}
} // namespace

int main()
{
    using namespace vocal2midi;
    MidiNoteTracker tracker;
    tracker.prepare (48000.0);
    MidiNoteTracker::Config config;
    config.stabilityMs = 0.0f;
    config.releaseMs = 50.0f;
    config.inputThresholdDb = -70.0f;
    config.confidenceThreshold = 0.5f;
    config.pitchBendSmoothingMs = 0.0f;

    juce::MidiBuffer midi;
    tracker.update (pitched (440.0f), config, 512, 0, midi);
    auto events = readEvents (midi);
    expect (events.noteOn == 1 && events.lastNoteOn == 69 && events.noteOff == 0,
            "First voiced frame emits one A4 Note On");

    midi.clear();
    for (int i = 0; i < 8; ++i)
        tracker.update (pitched (440.0f), config, 512, 0, midi);
    events = readEvents (midi);
    expect (events.noteOn == 0 && events.noteOff == 0, "Sustained note does not retrigger");

    midi.clear();
    tracker.update (pitched (523.251f), config, 512, 0, midi);
    events = readEvents (midi);
    expect (events.noteOn == 1 && events.lastNoteOn == 72 && events.noteOff == 1 && events.lastNoteOff == 69,
            "New note replaces old note cleanly");

    midi.clear();
    tracker.update (pitched (530.0f), config, 512, 0, midi); // Sharp C5: force a non-centre bend first.
    expect (readEvents (midi).bendCentre == 0, "Expressive deviation can leave pitch bend off-centre");
    midi.clear();
    tracker.update ({}, config, 2400, 0, midi); // 50 ms at 48 kHz.
    events = readEvents (midi);
    expect (events.noteOff == 1 && events.lastNoteOff == 72, "Release time emits Note Off on silence");
    expect (events.bendCentre >= 1, "Silence returns pitch bend to centre");

    midi.clear();
    tracker.update (pitched (440.0f), config, 512, 0, midi);
    midi.clear();
    config.outputEnabled = false;
    tracker.update (pitched (440.0f), config, 512, 0, midi);
    events = readEvents (midi);
    expect (events.noteOff == 1 && events.lastNoteOff == 69, "Disabling MIDI output closes active note");

    config.outputEnabled = true;
    config.stabilityMs = 100.0f;
    midi.clear();
    tracker.update (pitched (440.0f), config, 512, 0, midi);
    events = readEvents (midi);
    expect (events.noteOn == 0, "Stability time delays a new Note On");
    midi.clear();
    tracker.update (pitched (440.0f), config, 4800, 0, midi);
    events = readEvents (midi);
    expect (events.noteOn == 1, "Stable candidate starts after stability time");
    midi.clear();
    tracker.panic (midi);
    events = readEvents (midi);
    expect (events.noteOff == 1 && events.bendCentre >= 1, "Panic closes note and centres bend");

    if (failures != 0)
    {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All MIDI tracker tests passed.\n";
    return EXIT_SUCCESS;
}
