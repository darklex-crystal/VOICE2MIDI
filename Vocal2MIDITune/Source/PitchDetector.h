#pragma once

#include <complex>
#include <cstddef>
#include <vector>

namespace vocal2midi
{
class PitchDetector
{
public:
    struct Result
    {
        float frequencyHz = 0.0f;
        float confidence = 0.0f;
        float rms = 0.0f;
        float peak = 0.0f;
        bool voiceDetected = false;
    };

    PitchDetector() = default;

    // Allocates all working memory. Call from prepareToPlay, never from the audio callback.
    void prepare (double newSampleRate);
    void reset();
    void setFrequencyRange (float minimumHz, float maximumHz) noexcept;

    // Returns true only when a new analysis result is available (once per hop).
    bool processSample (float sample) noexcept;
    const Result& getLastResult() const noexcept { return lastResult; }
    int getAnalysisSize() const noexcept { return analysisSize; }
    int getHopSize() const noexcept { return hopSize; }

private:
    static void fft (std::vector<std::complex<float>>& data, bool inverse) noexcept;
    void analyse() noexcept;

    double sampleRate = 48000.0;
    float minimumHz = 50.0f;
    float maximumHz = 1500.0f;
    int analysisSize = 0;
    int fftSize = 0;
    int hopSize = 0;
    int writePosition = 0;
    int samplesUntilAnalysis = 0;

    std::vector<float> circularBuffer;
    std::vector<float> analysisBuffer;
    std::vector<float> prefixEnergy;
    std::vector<float> difference;
    std::vector<float> cumulativeDifference;
    std::vector<std::complex<float>> fftData;

    Result lastResult;
};
} // namespace vocal2midi
