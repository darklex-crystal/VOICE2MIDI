#include "../Source/MidiUtils.h"
#include "../Source/PitchDetector.h"
#include "../Source/ScaleQuantizer.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
int failures = 0;

void expect (bool condition, const std::string& message)
{
    if (! condition)
    {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
    else
    {
        std::cout << "PASS: " << message << '\n';
    }
}

vocal2midi::PitchDetector::Result analyseSine (float frequency, double sampleRate = 48000.0)
{
    vocal2midi::PitchDetector detector;
    detector.prepare (sampleRate);
    detector.setFrequencyRange (50.0f, 1500.0f);

    vocal2midi::PitchDetector::Result result;
    auto phase = 0.0;
    const auto phaseDelta = 2.0 * 3.14159265358979323846 * frequency / sampleRate;
    const auto count = static_cast<int> (sampleRate * 0.65);

    for (auto i = 0; i < count; ++i)
    {
        const auto envelope = std::min (1.0, static_cast<double> (i) / 512.0);
        const auto sample = static_cast<float> (0.5 * envelope * std::sin (phase));
        phase += phaseDelta;
        if (detector.processSample (sample))
            result = detector.getLastResult();
    }
    return result;
}
} // namespace

int main()
{
    using namespace vocal2midi;

    expect (std::abs (midiNoteToFrequency (69.0f) - 440.0f) < 0.0001f, "A4 MIDI to frequency");
    expect (std::abs (frequencyToMidiFloat (440.0f) - 69.0f) < 0.0001f, "A4 frequency to MIDI");
    expect (frequencyToMidiNote (220.0f) == 57, "A3 nearest MIDI note");
    expect (pitchBendTo14Bit (0.0f, 2.0f) == 8192, "Pitch bend centre");

    for (const auto frequency : { 50.0f, 110.0f, 220.0f, 440.0f, 880.0f, 1500.0f })
    {
        const auto result = analyseSine (frequency);
        const auto error = std::abs (result.frequencyHz - frequency) / frequency;
        std::cout << "  sine " << frequency << " Hz -> " << result.frequencyHz
                  << " Hz, confidence " << result.confidence << '\n';
        expect (result.voiceDetected && error < 0.012f && result.confidence > 0.75f,
                "YIN detects " + std::to_string (static_cast<int> (frequency)) + " Hz");
    }

    PitchDetector silence;
    silence.prepare (48000.0);
    PitchDetector::Result silentResult;
    for (auto i = 0; i < 12000; ++i)
        if (silence.processSample (0.0f))
            silentResult = silence.getLastResult();
    expect (! silentResult.voiceDetected && silentResult.frequencyHz == 0.0f && silentResult.rms == 0.0f,
            "Silence produces no voiced result");

    expect (ScaleQuantizer::quantize (61.0f, 0, ScaleType::major) == 62, "C major nearest-note tie resolves upward");
    expect (ScaleQuantizer::quantize (60.6f, 0, ScaleType::major) == 60, "C major scale lock");
    expect (ScaleQuantizer::quantize (61.0f, 0, ScaleType::chromatic) == 61, "Chromatic bypass");
    expect (ScaleQuantizer::quantize (62.0f, 9, ScaleType::naturalMinor) == 62, "A natural minor keeps D");
    expect (ScaleQuantizer::quantize (61.0f, 0, ScaleType::major, false) == 61, "Scale lock off");

    if (failures != 0)
    {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All DSP tests passed.\n";
    return EXIT_SUCCESS;
}
