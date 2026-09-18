#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>
#include <atomic>
#include "BaseDiscovery.h"
#include "PvAnalysisMath.h"
#include "SpectrumBridge.h"
template <int MaxBins, int MaxBases>
class SidechainDetector
{
public:
    using BridgeType = SpectrumBridge<MaxBins, MaxBases>;
    struct Params
    {
        float fMin = 20.0f;
        float fMax = 20000.0f;
        float thr = 0.5f;
        float centsTolerance = 35.0f;
        bool moreBases = false;
        bool hysteresis = false;
        float pitchRatio = 1.0f;
    };
    void prepare(int maxFftSize)
    {
        fftData.assign((size_t) (2 * maxFftSize), 0.0f);
        for(int channel = 0; channel < maxChannels; ++channel)
        {
            scFifo[(size_t) channel].assign((size_t) maxFftSize, 0.0f);
            channelMag[(size_t) channel].assign((size_t) MaxBins, 0.0f);
            channelFreq[(size_t) channel].assign((size_t) MaxBins, 0.0f);
            prevPhase[(size_t) channel].assign((size_t) MaxBins, 0.0f);
        }
        peakFreq.assign((size_t) maxPeaks, 0.0f);
        peakAmp.assign((size_t) maxPeaks, 0.0f);
        peakWork.assign((size_t) maxPeaks, 0.0f);
        peakBin.assign((size_t) maxPeaks, 0);
        harmonicMap.assign((size_t) maxPeaks * (size_t) maxPeaks, 0);
        rawPeaks.reserve((size_t) MaxBins);
        baseFreq.assign((size_t) MaxBases, 0.0f);
        baseSal.assign((size_t) MaxBases, 0.0f);
        baseConf.assign((size_t) MaxBases, 0.0f);
        analysisSeed.fill(true);
        prevPrimary.fill(0.0f);
        tracked = {};
        channelNumBases.fill(0);
        numPeaks = numBases = 0;
    }
    void configure(juce::dsp::FFT* engine, const float* windowPtr,
                    int fftSize_, int hopSize_, int numBins_,
                    float binWidth_, double sampleRate_, float spectrumNorm_)
    {
        fft = engine;
        window = windowPtr;
        fftSize = fftSize_;
        fftMask = fftSize_ - 1;
        hopSize = hopSize_;
        numBins = numBins_;
        binWidth = binWidth_;
        sampleRate = sampleRate_;
        spectrumNorm = spectrumNorm_;
        for(int channel = 0; channel < maxChannels; ++channel)
        {
            std::fill(scFifo[(size_t) channel].begin(), scFifo[(size_t) channel].end(), 0.0f);
            std::fill(prevPhase[(size_t) channel].begin(), prevPhase[(size_t) channel].end(), 0.0f);
        }
        analysisSeed.fill(true);
        activeChannels = 0;
        prevPrimary.fill(0.0f);
        tracked = {};
        channelNumBases.fill(0);
        numPeaks = numBases = 0;
    }
    void pushSample(int channel, int pos, float sample) noexcept
    {
        if(channel >= 0 && channel < maxChannels)
            scFifo[(size_t) channel][(size_t) pos] = sample;
    }
    void processHop(int pos, int numChannels, const Params& p) noexcept
    {
        if(fft == nullptr || window == nullptr || numBins <= 0)
            return;
        analyzeChannels(pos, numChannels);
        for(int channel = 0; channel < activeChannels; ++channel)
            detect(channelMag[(size_t) channel], channelFreq[(size_t) channel], channel, p);
        mergeDetectedBases(p.centsTolerance);
        publish(p.pitchRatio);
    }
    void processBypassHop(int pos, int numChannels) noexcept
    {
        if(fft == nullptr || window == nullptr || numBins <= 0)
            return;
        analyzeChannels(pos, numChannels);
        numBases = 0;
        channelNumBases.fill(0);
        lastNumBases.store(0, std::memory_order_relaxed);
        publishBypassed();
    }
    BridgeType& getBridge() noexcept { return bridge; }
    int debugNumBases() const noexcept { return lastNumBases.load(std::memory_order_relaxed); }
    const std::atomic<int>& numBasesAtomic() const noexcept { return lastNumBases; }
    int copyTargets(float* dst, int maxDst) const noexcept
    {
        const int n = juce::jmin(numBases, maxDst);
        for(int i = 0; i < n; ++i) dst[i] = baseFreq[(size_t) i];
        return n;
    }
    void publishGatedFrame(bool bypassed) noexcept
    {
        auto& s = bridge.startWrite();
        s.numChannels = activeChannels;
        s.numBins = numBins;
        s.binWidth = binWidth;
        s.sampleRate = sampleRate;
        s.pvBypassed = bypassed;
        const int nb = juce::jmin(numBins, (int) s.mag[0].size());
        for(int channel = 0; channel < activeChannels; ++channel)
            for(int j = 0; j < nb; ++j)
            {
                s.mag [(size_t) channel][(size_t) j] = channelMag [(size_t) channel][(size_t) j] * spectrumNorm;
                s.freq[(size_t) channel][(size_t) j] = channelFreq[(size_t) channel][(size_t) j];
            }
        s.numBases.fill(0);
        bridge.publish();
    }
private:
    static constexpr int maxPeaks = 128;
    static constexpr float tonalRefScale = 0.35f;
    static constexpr int defaultMaxPeaks = 64;
    static constexpr int defaultMaxBases = 16;
    void analyzeChannel(int channel) noexcept
    {
        float* fd = fftData.data();
        auto& magnitudes = channelMag[(size_t) channel];
        auto& frequencies = channelFreq[(size_t) channel];
        auto& previousPhases = prevPhase[(size_t) channel];
        const float twoPi = juce::MathConstants<float>::twoPi;
        const float expectPerBin = twoPi * (float) hopSize / (float) fftSize;
        const float freqScale = (float) (sampleRate / (twoPi * hopSize));
        const bool seed = analysisSeed[(size_t) channel];
        for(int k = 0; k < numBins; ++k)
        {
            const float re = fd[2 * k];
            const float im = fd[2 * k + 1];
            const float phase = pv_analysis::fastAtan2(im, re);
            magnitudes[(size_t) k] = std::sqrt(re * re + im * im);
            if(seed)
                previousPhases[(size_t) k] = pv_analysis::wrapPhase(
                    phase - expectPerBin * (float) k);
            const float dev = pv_analysis::wrapPhase(
                (phase - previousPhases[(size_t) k]) - expectPerBin * (float) k);
            frequencies[(size_t) k] = (float) k * binWidth + dev * freqScale;
            previousPhases[(size_t) k] = phase;
        }
        analysisSeed[(size_t) channel] = false;
    }
    void analyzeChannels(int pos, int numChannels) noexcept
    {
        activeChannels = juce::jlimit(0, maxChannels, numChannels);
        float* fd = fftData.data();
        for(int channel = 0; channel < activeChannels; ++channel)
        {
            for(int i = 0; i < fftSize; ++i)
                fd[i] = scFifo[(size_t) channel][(size_t) ((pos + i) & fftMask)] * window[i];
            fft->performRealOnlyForwardTransform(fd, false);
            analyzeChannel(channel);
        }
    }
    void detect(const std::vector<float>& magnitudes,
                 const std::vector<float>& frequencies,
                 int channel, const Params& p) noexcept
    {
        channelNumBases[(size_t) channel] = 0;
        base_detection::Config config;
        config.sampleRate = sampleRate;
        config.binWidth = binWidth;
        config.pitchRatio = p.pitchRatio;
        config.fMin = p.fMin;
        config.fMax = p.fMax;
        config.centsTolerance = p.centsTolerance;
        config.gatedThreshold = juce::jmax(0.0005f, p.thr * (p.moreBases ? 0.85f : 1.0f));
        config.previousPrimary = prevPrimary[(size_t) channel];
        config.peakLimit = p.moreBases ? maxPeaks : defaultMaxPeaks;
        config.baseLimit = p.moreBases ? MaxBases : defaultMaxBases;
        config.detectionPeakCapacity = maxPeaks;
        config.rawPeakLimit = 0;
        const auto result = base_detection::discover(
            magnitudes.data(), numBins, config,
            [&frequencies, &p] (int sourceBin, int& analysisBin, float& frequency)
            {
                analysisBin = sourceBin;
                frequency = frequencies[(size_t) sourceBin] * p.pitchRatio;
                return std::isfinite(frequency) && frequency >= 20.0f;
            },
            peakBin, peakFreq, peakAmp, peakWork, harmonicMap, rawPeaks, baseFreq, baseSal);
        numPeaks = result.numPeaks;
        numBases = result.numBases;
        const float tonal = juce::jlimit(0.0f, 1.0f,
                                          result.firstSalience
                                          / (tonalRefScale * (result.totalPeak + 1.0e-12f)));
        for(int b = 0; b < numBases; ++b)
        {
            const float rel = (result.firstSalience > 0.0f)
                                ? juce::jlimit(0.0f, 1.0f,
                                    baseSal[(size_t) b] / result.firstSalience) : 0.0f;
            baseConf[(size_t) b] = rel * tonal;
        }
        applyHysteresis(channel, p.hysteresis);
        if(numBases == 0) return;
        prevPrimary[(size_t) channel] = baseFreq[0];
        channelNumBases[(size_t) channel] = numBases;
        for(int base = 0; base < numBases; ++base)
        {
            channelBaseFreq[(size_t) channel][(size_t) base] = baseFreq[(size_t) base];
            channelBaseConf[(size_t) channel][(size_t) base] = baseConf[(size_t) base];
        }
    }
    void applyHysteresis(int channel, bool enabled) noexcept
    {
        auto& tracks = tracked[(size_t) channel];
        if(! enabled)
        {
            tracks = {};
            return;
        }
        constexpr float claimMs = 20.0f;
        constexpr float releaseMs = 50.0f;
        constexpr float claimHi = 0.6f;
        constexpr float claimLo = 0.45f;
        constexpr float salHoldDecay = 0.80f;
        constexpr float dedupeToleranceCents = 60.0f;
        const float hopSeconds = (float) hopSize / (float) juce::jmax(1.0, sampleRate);
        const float claimRise = juce::jlimit(0.01f, 1.0f, hopSeconds / (claimMs * 0.001f));
        const float releaseDecay = juce::jlimit(0.01f, 1.0f,
            hopSeconds / juce::jmax(1.0e-4f, releaseMs * 0.001f));
        std::array<float, MaxBases> candidateFreq {};
        std::array<float, MaxBases> candidateSal {};
        std::array<bool, MaxBases> candidateMatched {};
        const int candidateCount = juce::jmin(numBases, MaxBases);
        for(int candidate = 0; candidate < candidateCount; ++candidate)
        {
            candidateFreq[(size_t) candidate] = baseFreq[(size_t) candidate];
            candidateSal [(size_t) candidate] = baseConf[(size_t) candidate];
        }
        std::array<bool, MaxBases> trackMatched {};
        struct Match { float distanceCents; int track; int candidate; };
        std::array<Match, (size_t) MaxBases * (size_t) MaxBases> matches {};
        int matchCount = 0;
        for(int track = 0; track < MaxBases; ++track)
            if(tracks[(size_t) track].active)
                for(int candidate = 0; candidate < candidateCount; ++candidate)
                {
                    const float distance = 1200.0f * std::abs(std::log2(
                        candidateFreq[(size_t) candidate]
                        / juce::jmax(1.0e-6f, tracks[(size_t) track].frequency)));
                    matches[(size_t) matchCount++] = { distance, track, candidate };
                }
        std::sort(matches.begin(), matches.begin() + matchCount,
                  [] (const Match& a, const Match& b) { return a.distanceCents < b.distanceCents; });
        for(int match = 0; match < matchCount; ++match)
        {
            const auto& candidateMatch = matches[(size_t) match];
            if(trackMatched[(size_t) candidateMatch.track]
               || candidateMatched[(size_t) candidateMatch.candidate])
                continue;
            auto& track = tracks[(size_t) candidateMatch.track];
            trackMatched[(size_t) candidateMatch.track] = true;
            candidateMatched[(size_t) candidateMatch.candidate] = true;
            track.frequency = candidateFreq[(size_t) candidateMatch.candidate];
            track.salience = candidateSal[(size_t) candidateMatch.candidate];
            track.confidence = juce::jlimit(0.0f, 1.0f, track.confidence + claimRise);
            if(track.confidence >= claimHi) track.claimed = true;
        }
        for(int candidate = 0; candidate < candidateCount; ++candidate)
        {
            if(candidateMatched[(size_t) candidate]) continue;
            for(int slot = 0; slot < MaxBases; ++slot)
                if(! tracks[(size_t) slot].active)
                {
                    tracks[(size_t) slot] = { candidateFreq[(size_t) candidate],
                                              claimRise, candidateSal[(size_t) candidate], true, false };
                    trackMatched[(size_t) slot] = true;
                    break;
                }
        }
        for(int track = 0; track < MaxBases; ++track)
        {
            auto& state = tracks[(size_t) track];
            if(! state.active || trackMatched[(size_t) track]) continue;
            state.confidence -= releaseDecay;
            state.salience *= salHoldDecay;
            if(state.confidence <= claimLo) state.claimed = false;
            if(state.confidence <= 0.0f) state = {};
        }
        numBases = 0;
        auto appendUnique = [&] (float frequency, float salience)
        {
            for(int base = 0; base < numBases; ++base)
                if(std::abs(1200.0f * std::log2(frequency / baseFreq[(size_t) base]))
                     < dedupeToleranceCents)
                    return;
            if(numBases < MaxBases)
            {
                baseFreq[(size_t) numBases] = frequency;
                baseConf[(size_t) numBases] = salience;
                baseSal [(size_t) numBases] = salience;
                ++numBases;
            }
        };
        std::array<int, MaxBases> claimedOrder {};
        int claimedCount = 0;
        for(int track = 0; track < MaxBases; ++track)
            if(tracks[(size_t) track].active && tracks[(size_t) track].claimed)
                claimedOrder[(size_t) claimedCount++] = track;
        std::sort(claimedOrder.begin(), claimedOrder.begin() + claimedCount,
                  [&tracks] (int a, int b)
                  {
                      return tracks[(size_t) a].confidence > tracks[(size_t) b].confidence;
                  });
        for(int index = 0; index < claimedCount; ++index)
        {
            const auto& track = tracks[(size_t) claimedOrder[(size_t) index]];
            appendUnique(track.frequency, track.salience);
        }
        for(int candidate = 0; candidate < candidateCount; ++candidate)
            appendUnique(candidateFreq[(size_t) candidate], candidateSal[(size_t) candidate]);
    }
    void mergeDetectedBases(float centsTolerance) noexcept
    {
        numBases = 0;
        for(int channel = 0; channel < activeChannels; ++channel)
            for(int base = 0; base < channelNumBases[(size_t) channel] && numBases < MaxBases; ++base)
            {
                const float candidate = channelBaseFreq[(size_t) channel][(size_t) base];
                bool duplicate = false;
                for(int merged = 0; merged < numBases; ++merged)
                      if(std::abs(1200.0f * std::log2 (candidate / baseFreq[(size_t) merged]))
                          < centsTolerance)
                    {
                        duplicate = true;
                        break;
                    }
                if(! duplicate)
                    baseFreq[(size_t) numBases++] = candidate;
            }
        lastNumBases.store(numBases, std::memory_order_relaxed);
    }
    void publish(float pitchRatio) noexcept
    {
        auto& s = bridge.startWrite();
        s.numChannels = activeChannels;
        s.numBins = numBins;
        s.binWidth = binWidth;
        s.sampleRate = sampleRate;
        s.pvBypassed = false;
        const int nb = juce::jmin(numBins, (int) s.mag[0].size());
        for(int channel = 0; channel < activeChannels; ++channel)
            for(int j = 0; j < nb; ++j)
            {
                s.mag [(size_t) channel][(size_t) j] = channelMag [(size_t) channel][(size_t) j] * spectrumNorm;
                s.freq[(size_t) channel][(size_t) j] = channelFreq[(size_t) channel][(size_t) j] * pitchRatio;
            }
        for(int channel = 0; channel < activeChannels; ++channel)
        {
            s.numBases[(size_t) channel] = channelNumBases[(size_t) channel];
            for(int base = 0; base < s.numBases[(size_t) channel]; ++base)
            {
                s.baseHz [(size_t) channel][(size_t) base] = channelBaseFreq[(size_t) channel][(size_t) base];
                s.baseConf[(size_t) channel][(size_t) base] = channelBaseConf[(size_t) channel][(size_t) base];
            }
        }
        bridge.publish();
    }
    void publishBypassed() noexcept
    {
        auto& s = bridge.startWrite();
        s.numChannels = activeChannels;
        s.numBins = numBins;
        s.binWidth = binWidth;
        s.sampleRate = sampleRate;
        s.pvBypassed = true;
        const int nb = juce::jmin(numBins, (int) s.mag[0].size());
        for(int channel = 0; channel < activeChannels; ++channel)
            for(int j = 0; j < nb; ++j)
            {
                s.mag [(size_t) channel][(size_t) j] = channelMag [(size_t) channel][(size_t) j] * spectrumNorm;
                s.freq[(size_t) channel][(size_t) j] = channelFreq[(size_t) channel][(size_t) j];
            }
        s.numBases.fill(0);
        bridge.publish();
    }
    juce::dsp::FFT* fft = nullptr;
    const float* window = nullptr;
    int fftSize = 0;
    int fftMask = 0;
    int hopSize = 0;
    int numBins = 0;
    float binWidth = 0.0f;
    float spectrumNorm = 1.0f;
    double sampleRate = 44100.0;
    static constexpr int maxChannels = 2;
    struct TrackedBase
    {
        float frequency = 0.0f;
        float confidence = 0.0f;
        float salience = 0.0f;
        bool active = false;
        bool claimed = false;
    };
    std::array<std::vector<float>, maxChannels> scFifo;
    std::vector<float> fftData;
    std::array<std::vector<float>, maxChannels> channelMag, channelFreq, prevPhase;
    std::vector<int> peakBin;
    std::vector<float> peakFreq, peakAmp, peakWork;
    std::vector<unsigned char> harmonicMap;
    std::vector<base_detection::RawPeak> rawPeaks;
    std::vector<float> baseFreq, baseSal, baseConf;
    std::array<std::array<float, MaxBases>, maxChannels> channelBaseFreq {};
    std::array<std::array<float, MaxBases>, maxChannels> channelBaseConf {};
    std::array<int, maxChannels> channelNumBases {};
    int numPeaks = 0;
    int numBases = 0;
    std::array<float, maxChannels> prevPrimary {};
    std::array<std::array<TrackedBase, MaxBases>, maxChannels> tracked {};
    int activeChannels = 0;
    std::array<bool, maxChannels> analysisSeed {};
    std::atomic<int> lastNumBases { 0 };
    BridgeType bridge;
};