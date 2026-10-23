#include "PitchDetector.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace vocal2midi
{
void PitchDetector::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 1000.0 ? newSampleRate : 48000.0;

    // About 80 ms gives at least four cycles at 50 Hz, with a 2048-sample minimum.
    const auto desiredSize = std::max (2048, static_cast<int> (sampleRate * 0.08));
    analysisSize = 2048;
    while (analysisSize < desiredSize)
        analysisSize <<= 1;

    fftSize = analysisSize * 2; // Zero padding gives linear rather than circular autocorrelation.
    hopSize = std::max (256, analysisSize / 8);

    circularBuffer.assign (static_cast<std::size_t> (analysisSize), 0.0f);
    analysisBuffer.assign (static_cast<std::size_t> (analysisSize), 0.0f);
    prefixEnergy.assign (static_cast<std::size_t> (analysisSize + 1), 0.0f);
    difference.assign (static_cast<std::size_t> (analysisSize / 2 + 2), 0.0f);
    cumulativeDifference.assign (difference.size(), 1.0f);
    fftData.assign (static_cast<std::size_t> (fftSize), std::complex<float> {});

    reset();
}

void PitchDetector::reset()
{
    std::fill (circularBuffer.begin(), circularBuffer.end(), 0.0f);
    std::fill (analysisBuffer.begin(), analysisBuffer.end(), 0.0f);
    writePosition = 0;
    samplesUntilAnalysis = 0;
    lastResult = {};
}

void PitchDetector::setFrequencyRange (float newMinimumHz, float newMaximumHz) noexcept
{
    const auto low = std::clamp (newMinimumHz, 40.0f, 1500.0f);
    const auto high = std::clamp (newMaximumHz, low + 1.0f, 1600.0f);
    minimumHz = low;
    maximumHz = high;
}

bool PitchDetector::processSample (float sample) noexcept
{
    if (analysisSize == 0 || circularBuffer.empty())
        return false;

    circularBuffer[static_cast<std::size_t> (writePosition)] = std::isfinite (sample) ? sample : 0.0f;
    writePosition = (writePosition + 1) % analysisSize;

    if (++samplesUntilAnalysis < hopSize)
        return false;

    samplesUntilAnalysis = 0;
    analyse();
    return true;
}

void PitchDetector::fft (std::vector<std::complex<float>>& data, bool inverse) noexcept
{
    const auto count = data.size();
    if (count < 2)
        return;

    for (std::size_t i = 1, j = 0; i < count; ++i)
    {
        auto bit = count >> 1;
        for (; (j & bit) != 0; bit >>= 1)
            j ^= bit;
        j ^= bit;

        if (i < j)
            std::swap (data[i], data[j]);
    }

    constexpr auto pi = 3.14159265358979323846;
    for (std::size_t length = 2; length <= count; length <<= 1)
    {
        const auto angle = (inverse ? 2.0 : -2.0) * pi / static_cast<double> (length);
        const std::complex<float> step (static_cast<float> (std::cos (angle)),
                                        static_cast<float> (std::sin (angle)));
        const auto half = length >> 1;

        for (std::size_t start = 0; start < count; start += length)
        {
            std::complex<float> twiddle (1.0f, 0.0f);
            for (std::size_t j = 0; j < half; ++j)
            {
                const auto even = data[start + j];
                const auto odd = data[start + j + half] * twiddle;
                data[start + j] = even + odd;
                data[start + j + half] = even - odd;
                twiddle *= step;
            }
        }
    }

    if (inverse)
    {
        const auto scale = 1.0f / static_cast<float> (count);
        for (auto& value : data)
            value *= scale;
    }
}

void PitchDetector::analyse() noexcept
{
    // Copy the circular history into chronological order and remove DC.
    auto mean = 0.0;
    for (int i = 0; i < analysisSize; ++i)
    {
        const auto source = (writePosition + i) % analysisSize;
        const auto value = circularBuffer[static_cast<std::size_t> (source)];
        analysisBuffer[static_cast<std::size_t> (i)] = value;
        mean += value;
    }
    mean /= static_cast<double> (analysisSize);

    auto sumSquares = 0.0;
    auto peak = 0.0f;
    prefixEnergy[0] = 0.0f;

    for (int i = 0; i < analysisSize; ++i)
    {
        const auto index = static_cast<std::size_t> (i);
        auto value = analysisBuffer[index] - static_cast<float> (mean);
        analysisBuffer[index] = value;
        peak = std::max (peak, std::abs (value));
        sumSquares += static_cast<double> (value) * value;
        prefixEnergy[index + 1] = static_cast<float> (sumSquares);
        fftData[index] = { value, 0.0f };
    }

    const auto rms = static_cast<float> (std::sqrt (sumSquares / static_cast<double> (analysisSize)));
    if (rms < 1.0e-6f || peak < 1.0e-5f)
    {
        lastResult = { 0.0f, 0.0f, rms, peak, false };
        return;
    }

    std::fill (fftData.begin() + analysisSize, fftData.end(), std::complex<float> {});
    fft (fftData, false);
    for (auto& value : fftData)
        value = { std::norm (value), 0.0f };
    fft (fftData, true); // fftData[tau].real() is now autocorrelation at tau.

    const auto minTau = std::max (2, static_cast<int> (std::floor (sampleRate / maximumHz)));
    const auto maxTau = std::min (analysisSize / 2 - 2,
                                  static_cast<int> (std::ceil (sampleRate / minimumHz)));
    if (maxTau <= minTau + 2)
    {
        lastResult = { 0.0f, 0.0f, rms, peak, false };
        return;
    }

    difference[0] = 0.0f;
    cumulativeDifference[0] = 1.0f;
    auto runningSum = 0.0f;
    auto bestTau = minTau;
    auto bestValue = 1.0e20f;

    for (auto tau = 1; tau <= maxTau; ++tau)
    {
        const auto index = static_cast<std::size_t> (tau);
        const auto firstEnergy = prefixEnergy[static_cast<std::size_t> (analysisSize - tau)];
        const auto secondEnergy = prefixEnergy[static_cast<std::size_t> (analysisSize)] - prefixEnergy[index];
        const auto correlation = fftData[index].real();
        const auto d = std::max (0.0f, firstEnergy + secondEnergy - 2.0f * correlation);
        difference[index] = d;
        runningSum += d;
        cumulativeDifference[index] = runningSum > 1.0e-20f
                                    ? d * static_cast<float> (tau) / runningSum
                                    : 1.0f;

        if (tau >= minTau && cumulativeDifference[index] < bestValue)
        {
            bestValue = cumulativeDifference[index];
            bestTau = tau;
        }
    }

    // YIN: use the first local minimum below the threshold, not merely the global minimum.
    constexpr float yinThreshold = 0.18f;
    auto selectedTau = bestTau;
    for (auto tau = minTau; tau < maxTau; ++tau)
    {
        if (cumulativeDifference[static_cast<std::size_t> (tau)] >= yinThreshold)
            continue;

        selectedTau = tau;
        while (selectedTau + 1 <= maxTau
               && cumulativeDifference[static_cast<std::size_t> (selectedTau + 1)]
                < cumulativeDifference[static_cast<std::size_t> (selectedTau)])
            ++selectedTau;
        break;
    }

    auto refinedTau = static_cast<float> (selectedTau);
    if (selectedTau > 1 && selectedTau < maxTau)
    {
        const auto left = cumulativeDifference[static_cast<std::size_t> (selectedTau - 1)];
        const auto centre = cumulativeDifference[static_cast<std::size_t> (selectedTau)];
        const auto right = cumulativeDifference[static_cast<std::size_t> (selectedTau + 1)];
        const auto denominator = left - 2.0f * centre + right;
        if (std::abs (denominator) > 1.0e-12f)
            refinedTau += std::clamp (0.5f * (left - right) / denominator, -0.5f, 0.5f);
    }

    const auto frequency = refinedTau > 0.0f ? static_cast<float> (sampleRate) / refinedTau : 0.0f;
    const auto confidence = std::clamp (1.0f - cumulativeDifference[static_cast<std::size_t> (selectedTau)], 0.0f, 1.0f);
    const auto valid = frequency >= minimumHz * 0.9f && frequency <= maximumHz * 1.1f && confidence > 0.05f;
    lastResult = { valid ? frequency : 0.0f, valid ? confidence : 0.0f, rms, peak,
                   valid && confidence >= 0.25f };
}
} // namespace vocal2midi
