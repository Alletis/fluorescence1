#pragma once
#include <juce_core/juce_core.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
namespace base_detection
{
struct RawPeak
{
    int sourceBin = 0;
    int analysisBin = 0;
    float hz = 0.0f;
    float mag = 0.0f;
    float log2Hz = 0.0f;
};
struct Config
{
    double sampleRate = 44100.0;
    float binWidth = 0.0f;
    float pitchRatio = 1.0f;
    float fMin = 20.0f;
    float fMax = 20000.0f;
    float centsTolerance = 35.0f;
    float gatedThreshold = 0.0f;
    float previousPrimary = 0.0f;
    int peakLimit = 64;
    int baseLimit = 16;
    int detectionPeakCapacity = 128;
    int rawPeakLimit = 2048;
};
struct Result
{
    int numPeaks = 0;
    int numBases = 0;
    float maxMagnitude = 0.0f;
    float totalPeak = 0.0f;
    float firstSalience = 0.0f;
};
template <typename ResolveObservation>
Result discover(const float* magnitude, int numBins, const Config& config,
                ResolveObservation&& resolveObservation,
                std::vector<int>& peakBin,
                std::vector<float>& peakFreq,
                std::vector<float>& peakAmp,
                std::vector<float>& peakWork,
                std::vector<unsigned char>& harmonicMap,
                std::vector<RawPeak>& rawPeaks,
                std::vector<float>& baseFreq,
                std::vector<float>& baseSal) noexcept
{
    constexpr int salienceHarmonics = 32;
    constexpr float salAlpha = 52.0f;
    constexpr float salBeta = 320.0f;
    constexpr float octaveBias = 0.80f;
    constexpr float continuityBias = 0.30f;
    constexpr float peakFloorRel = 0.02f;
    constexpr float partialPeakFloorRel = 0.0005f;
    constexpr float silenceMagFloor = 1.0e-4f;
    constexpr float membershipSigmaCents = 45.0f;
    constexpr float membershipGapFrac = 0.35f;
    Result result;
    rawPeaks.clear();
    for(int bin = 0; bin < numBins; ++bin)
        result.maxMagnitude = std::max(result.maxMagnitude, magnitude[(size_t) bin]);
    if(result.maxMagnitude < silenceMagFloor)
        return result;
    const int peakCapacity = std::min({ config.detectionPeakCapacity, (int) peakBin.size(),
                                        (int) peakFreq.size(), (int) peakAmp.size(),
                                        (int) peakWork.size() });
    const int mapStride = peakCapacity;
    if(peakCapacity <= 0 || (int) harmonicMap.size() < mapStride * mapStride)
        return result;
    const float floorMagnitude = result.maxMagnitude * peakFloorRel;
    const float harmonicFloor = juce::jmax(1.0e-12f,
                                            result.maxMagnitude * partialPeakFloorRel);
    const float transformedBinWidth = juce::jmax(1.0e-6f,
                                                   config.binWidth * config.pitchRatio);
    const int subLimit = juce::jmin(config.peakLimit / 2, peakCapacity / 4);
    const int superLimit = juce::jmin(config.peakLimit / 2, peakCapacity / 4);
    int inBandCount = 0;
    int subCount = 0;
    int superCount = 0;
    for(int sourceBin = 1; sourceBin + 1 < numBins; ++sourceBin)
    {
        const float localMaxMagnitude = magnitude[(size_t) sourceBin];
        if(! (localMaxMagnitude > magnitude[(size_t) (sourceBin - 1)]
              && localMaxMagnitude >= magnitude[(size_t) (sourceBin + 1)]))
            continue;
        int analysisBin = sourceBin;
        float frequency = 0.0f;
        if(! resolveObservation(sourceBin, analysisBin, frequency))
            continue;
        const float resolvedMagnitude = magnitude[(size_t) analysisBin];
        if(resolvedMagnitude > harmonicFloor
           && (int) rawPeaks.size() < config.rawPeakLimit)
            rawPeaks.push_back({ sourceBin, analysisBin, frequency, resolvedMagnitude,
                                 std::log2(juce::jmax(1.0e-6f, frequency)) });
        if(result.numPeaks >= peakCapacity || localMaxMagnitude <= floorMagnitude)
            continue;
        if(frequency < config.fMin)
        {
            if(subCount >= subLimit) continue;
        }
        else if(frequency > config.fMax)
        {
            if(superCount >= superLimit) continue;
        }
        else if(inBandCount >= config.peakLimit)
        {
            continue;
        }
        int duplicatePeak = -1;
        for(int peak = result.numPeaks - 1; peak >= 0; --peak)
        {
            if(sourceBin - peakBin[(size_t) peak] > 3)
                break;
            const float distanceHz = std::abs(frequency - peakFreq[(size_t) peak]);
            const int distanceBins = std::abs(analysisBin - peakBin[(size_t) peak]);
            if(distanceBins <= 1
               || (distanceBins <= 2 && distanceHz < 0.55f * transformedBinWidth))
            {
                duplicatePeak = peak;
                break;
            }
        }
        if(duplicatePeak >= 0)
        {
            if(resolvedMagnitude > peakAmp[(size_t) duplicatePeak])
            {
                peakBin [(size_t) duplicatePeak] = analysisBin;
                peakFreq[(size_t) duplicatePeak] = frequency;
                peakAmp [(size_t) duplicatePeak] = resolvedMagnitude;
            }
            continue;
        }
        const int peak = result.numPeaks++;
        peakBin [(size_t) peak] = analysisBin;
        peakFreq[(size_t) peak] = frequency;
        peakAmp [(size_t) peak] = resolvedMagnitude;
        if(frequency < config.fMin) ++subCount;
        else if(frequency > config.fMax) ++superCount;
        else ++inBandCount;
    }
    if(result.numPeaks == 0)
        return result;
    const float toleranceLo = std::exp2(-config.centsTolerance / 1200.0f);
    const float toleranceHi = std::exp2( config.centsTolerance / 1200.0f);
    for(int candidate = 0; candidate < result.numPeaks; ++candidate)
    {
        const float fundamental = peakFreq[(size_t) candidate];
        unsigned char* row = harmonicMap.data() + (size_t) candidate * (size_t) mapStride;
        const float inverseFundamental = 1.0f / fundamental;
        for(int peak = 0; peak < result.numPeaks; ++peak)
        {
            const float partial = peakFreq[(size_t) peak];
            const int harmonic = (int) (partial * inverseFundamental + 0.5f);
            unsigned char mappedHarmonic = 0;
            if(harmonic >= 1 && harmonic <= salienceHarmonics)
            {
                const float target = (float) harmonic * fundamental;
                const float ratio = partial / target;
                if(target < (float) (config.sampleRate * 0.5)
                   && ratio > toleranceLo && ratio < toleranceHi)
                    mappedHarmonic = (unsigned char) harmonic;
            }
            row[peak] = mappedHarmonic;
        }
    }
    for(int peak = 0; peak < result.numPeaks; ++peak)
    {
        peakWork[(size_t) peak] = peakAmp[(size_t) peak];
        result.totalPeak += peakAmp[(size_t) peak];
    }
    auto salience = [&] (int candidate)
    {
        const float fundamental = peakFreq[(size_t) candidate];
        const unsigned char* row = harmonicMap.data()
                                 + (size_t) candidate * (size_t) mapStride;
        std::array<float, 33> bestByHarmonic {};
        for(int peak = 0; peak < result.numPeaks; ++peak)
        {
            const int harmonic = row[peak];
            if(harmonic > 0 && peakWork[(size_t) peak] > bestByHarmonic[(size_t) harmonic])
                bestByHarmonic[(size_t) harmonic] = peakWork[(size_t) peak];
        }
        float sum = 0.0f;
        for(int harmonic = 1; harmonic <= salienceHarmonics; ++harmonic)
        {
            const float target = (float) harmonic * fundamental;
            if(target >= (float) (config.sampleRate * 0.5)) break;
            sum += ((fundamental + salAlpha) / (target + salBeta))
                 * bestByHarmonic[(size_t) harmonic];
        }
        return sum;
    };
    while(result.numBases < juce::jmin(config.baseLimit, (int) baseFreq.size()))
    {
        float bestScore = 0.0f;
        float bestSalience = 0.0f;
        float bestFrequency = 0.0f;
        int bestIndex = -1;
        for(int candidate = 0; candidate < result.numPeaks; ++candidate)
        {
            const float frequency = peakFreq[(size_t) candidate];
            if(peakWork[(size_t) candidate] <= 0.0f
               || frequency < config.fMin || frequency > config.fMax)
                continue;
            const float candidateSalience = salience(candidate);
            float score = candidateSalience;
                if(result.numBases == 0 && config.previousPrimary > 0.0f
                    && std::abs(1200.0f * std::log2(frequency / config.previousPrimary))
                          < config.centsTolerance)
                score *= 1.0f + continuityBias;
            if(score > bestScore)
            {
                bestScore = score;
                bestSalience = candidateSalience;
                bestFrequency = frequency;
                bestIndex = candidate;
            }
        }
        if(bestIndex < 0) break;
        bool improved = true;
        while(improved)
        {
            improved = false;
            const float halfFrequency = bestFrequency * 0.5f;
            if(halfFrequency >= config.fMin)
                for(int candidate = 0; candidate < result.numPeaks; ++candidate)
                {
                    if(peakWork[(size_t) candidate] <= 0.0f) continue;
                    const float frequency = peakFreq[(size_t) candidate];
                    if(std::abs(1200.0f * std::log2(frequency / halfFrequency))
                         < config.centsTolerance)
                    {
                        const float subSalience = salience(candidate);
                        if(subSalience >= octaveBias * bestSalience)
                        {
                            bestFrequency = frequency;
                            bestSalience = subSalience;
                            bestIndex = candidate;
                            improved = true;
                        }
                        break;
                    }
                }
        }
        if(result.numBases == 0) result.firstSalience = bestSalience;
        else if(bestSalience < config.gatedThreshold * result.firstSalience) break;
        baseFreq[(size_t) result.numBases] = bestFrequency;
        baseSal [(size_t) result.numBases] = bestSalience;
        ++result.numBases;
        const float depth = juce::jlimit(0.0f, 1.0f,
            result.firstSalience > 0.0f ? bestSalience / result.firstSalience : 1.0f);
        for(int peak = 0; peak < result.numPeaks; ++peak)
        {
            if(peakWork[(size_t) peak] <= 0.0f) continue;
            const float partial = peakFreq[(size_t) peak];
            const float ratioToBase = partial / bestFrequency;
            const int harmonic = (int) (ratioToBase + 0.5f);
            if(harmonic < 1) continue;
            const float ratio = ratioToBase / (float) harmonic;
            if(ratio < 0.94f || ratio > 1.06f) continue;
            const float gapCents = 1200.0f
                                 * std::log2((float) (harmonic + 1) / (float) harmonic);
            const float resolutionFloor = 1200.0f
                                        * std::log2(1.0f + config.binWidth
                                                           / juce::jmax(1.0f, partial));
            const float low = juce::jmax(5.0f, resolutionFloor);
            const float sigma = juce::jlimit(low, juce::jmax(low, membershipSigmaCents),
                                              membershipGapFrac * gapCents);
            const float z = 1200.0f * std::log2(ratio) / sigma;
            peakWork[(size_t) peak] *= 1.0f - depth * std::exp(-z * z);
        }
    }
    return result;
}
}