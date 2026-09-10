#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>
#include <atomic>
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
        bool moreBases = false;
        float pitchRatio = 1.0f;
    };
    void prepare(int maxFftSize)
    {
        fftData.assign((size_t) (2 * maxFftSize), 0.0f);
        pvMag.assign((size_t) MaxBins, 0.0f);
        pvFreq.assign((size_t) MaxBins, 0.0f);
        for(int channel = 0; channel < maxChannels; ++channel)
        {
            scFifo[(size_t) channel].assign((size_t) maxFftSize, 0.0f);
            channelMag[(size_t) channel].assign((size_t) MaxBins, 0.0f);
            channelFreq[(size_t) channel].assign((size_t) MaxBins, 0.0f);
            channelPhase[(size_t) channel].assign((size_t) MaxBins, 0.0f);
            prevPhase[(size_t) channel].assign((size_t) MaxBins, 0.0f);
        }
        peakFreq.assign((size_t) maxPeaks, 0.0f);
        peakAmp.assign((size_t) maxPeaks, 0.0f);
        peakWork.assign((size_t) maxPeaks, 0.0f);
        baseFreq.assign((size_t) MaxBases, 0.0f);
        baseSal.assign((size_t) MaxBases, 0.0f);
        baseConf.assign((size_t) MaxBases, 0.0f);
        analysisSeed.fill(true);
        prevPrimary.fill(0.0f);
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
        {
            std::copy(channelMag[(size_t) channel].begin(),
                       channelMag[(size_t) channel].begin() + numBins, pvMag.begin());
            std::copy(channelFreq[(size_t) channel].begin(),
                       channelFreq[(size_t) channel].begin() + numBins, pvFreq.begin());
            detect(channel, p);
        }
        mergeDetectedBases();
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
    static constexpr int harmonicsMax = 32;
    static constexpr float centsTol = 45.0f;
    static constexpr float salAlpha = 52.0f;
    static constexpr float salBeta = 320.0f;
    static constexpr float octaveBias = 0.80f;
    static constexpr float continuityBias = 0.30f;
    static constexpr float peakFloorRel = 0.02f;
    static constexpr float silenceMagFloor = 1.0e-4f;
    static constexpr float tonalRefScale = 0.35f;
    static constexpr int defaultMaxPeaks = 64;
    static constexpr int defaultMaxBases = 16;
    static float wrapPhase(float x) noexcept
    {
        return x - juce::MathConstants<float>::twoPi
                   * std::round(x / juce::MathConstants<float>::twoPi);
    }
    void analyzeChannel(int channel) noexcept
    {
        float* fd = fftData.data();
        const float twoPi = juce::MathConstants<float>::twoPi;
        const float expectPerBin = twoPi * (float) hopSize / (float) fftSize;
        const float freqScale = (float) (sampleRate / (twoPi * hopSize));
        for(int k = 0; k < numBins; ++k)
        {
            const float re = fd[2 * k];
            const float im = fd[2 * k + 1];
            channelPhase[(size_t) channel][(size_t) k] = std::atan2 (im, re);
            channelMag [(size_t) channel][(size_t) k] = std::sqrt(re * re + im * im);
        }
        if(analysisSeed[(size_t) channel])
        {
            for(int k = 0; k < numBins; ++k)
                prevPhase[(size_t) channel][(size_t) k] = wrapPhase(
                    channelPhase[(size_t) channel][(size_t) k] - expectPerBin * (float) k);
            analysisSeed[(size_t) channel] = false;
        }
        for(int k = 0; k < numBins; ++k)
        {
            const float phase = channelPhase[(size_t) channel][(size_t) k];
            const float dev = wrapPhase((phase - prevPhase[(size_t) channel][(size_t) k])
                                         - expectPerBin * (float) k);
            channelFreq[(size_t) channel][(size_t) k] = (float) k * binWidth + dev * freqScale;
            prevPhase[(size_t) channel][(size_t) k] = phase;
        }
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
        for(int k = 0; k < numBins; ++k)
        {
            int strongestChannel = 0;
            for(int channel = 1; channel < activeChannels; ++channel)
                if(channelMag[(size_t) channel][(size_t) k]
                    > channelMag[(size_t) strongestChannel][(size_t) k])
                    strongestChannel = channel;
            pvMag [(size_t) k] = activeChannels > 0
                ? channelMag [(size_t) strongestChannel][(size_t) k] : 0.0f;
            pvFreq[(size_t) k] = activeChannels > 0
                ? channelFreq[(size_t) strongestChannel][(size_t) k] : 0.0f;
        }
    }
    float salience(float F) const noexcept
    {
        if(F <= 0.0f) return 0.0f;
        const float nyq = (float) (sampleRate * 0.5);
        float sal = 0.0f;
        for(int n = 1; n <= harmonicsMax; ++n)
        {
            const float target = (float) n * F;
            if(target >= nyq) break;
            float best = 0.0f;
            for(int i = 0; i < numPeaks; ++i)
            {
                if(peakWork[(size_t) i] <= 0.0f) continue;
                const float cents = 1200.0f * std::log2 (peakFreq[(size_t) i] / target);
                if(std::abs(cents) < centsTol)
                    best = std::max(best, peakWork[(size_t) i]);
            }
            sal += ((F + salAlpha) / (target + salBeta)) * best;
        }
        return sal;
    }
    void detect(int channel, const Params& p) noexcept
    {
        numPeaks = 0;
        numBases = 0;
        channelNumBases[(size_t) channel] = 0;
        const int peakLimit = p.moreBases ? maxPeaks : defaultMaxPeaks;
        const int baseLimit = p.moreBases ? MaxBases : defaultMaxBases;
        const float salienceGateScale = p.moreBases ? 0.85f : 1.0f;
        const float gatedThr = juce::jmax(0.01f, p.thr * salienceGateScale);
        float maxMag = 0.0f;
        for(int k = 0; k < numBins; ++k) maxMag = std::max(maxMag, pvMag[(size_t) k]);
        if(maxMag < silenceMagFloor) return;
        const float floorMag = maxMag * peakFloorRel;
        for(int k = 2; k + 2 < numBins && numPeaks < peakLimit; ++k)
        {
            const float m = pvMag[(size_t) k];
            if(m > floorMag
                && m > pvMag[(size_t) (k - 1)] && m > pvMag[(size_t) (k - 2)]
                && m >= pvMag[(size_t) (k + 1)] && m >= pvMag[(size_t) (k + 2)])
            {
                peakFreq[(size_t) numPeaks] = pvFreq[(size_t) k] * p.pitchRatio;
                peakAmp [(size_t) numPeaks] = m;
                ++numPeaks;
            }
        }
        if(numPeaks == 0) return;
        float totalPeak = 0.0f;
        for(int i = 0; i < numPeaks; ++i)
        {
            peakWork[(size_t) i] = peakAmp[(size_t) i];
            totalPeak += peakAmp[(size_t) i];
        }
        float firstSal = 0.0f;
        while(numBases < baseLimit)
        {
            float bestScore = 0.0f, bestSal = 0.0f, bestF = 0.0f;
            int bestIdx = -1;
            for(int c = 0; c < numPeaks; ++c)
            {
                const float Fc = peakFreq[(size_t) c];
                if(peakWork[(size_t) c] <= 0.0f) continue;
                if(Fc < p.fMin || Fc > p.fMax) continue;
                const float sal = salience(Fc);
                float score = sal;
                if(numBases == 0 && prevPrimary[(size_t) channel] > 0.0f
                    && std::abs(1200.0f * std::log2 (Fc / prevPrimary[(size_t) channel])) < centsTol)
                    score *= (1.0f + continuityBias);
                if(score > bestScore) { bestScore = score; bestSal = sal; bestF = Fc; bestIdx = c; }
            }
            if(bestIdx < 0) break;
            bool improved = true;
            while(improved)
            {
                improved = false;
                const float halfF = bestF * 0.5f;
                if(halfF >= p.fMin)
                    for(int c = 0; c < numPeaks; ++c)
                    {
                        if(peakWork[(size_t) c] <= 0.0f) continue;
                        const float fc = peakFreq[(size_t) c];
                        if(std::abs(1200.0f * std::log2 (fc / halfF)) < centsTol)
                        {
                            const float subSal = salience(fc);
                            if(subSal >= octaveBias * bestSal)
                            { bestF = fc; bestSal = subSal; bestIdx = c; improved = true; }
                            break;
                        }
                    }
            }
            if(numBases == 0) firstSal = bestSal;
            else if(bestSal < gatedThr * firstSal) break;
            baseSal [(size_t) numBases] = bestSal;
            baseFreq[(size_t) numBases] = bestF;
            ++numBases;
            for(int i = 0; i < numPeaks; ++i)
            {
                if(peakWork[(size_t) i] <= 0.0f) continue;
                const int n = (int) std::lround(peakFreq[(size_t) i] / bestF);
                if(n < 1 || n > harmonicsMax) continue;
                const float cents = 1200.0f * std::log2 (peakFreq[(size_t) i] / ((float) n * bestF));
                if(std::abs(cents) < centsTol) peakWork[(size_t) i] = 0.0f;
            }
        }
        if(numBases == 0) return;
        prevPrimary[(size_t) channel] = baseFreq[0];
        const float tonal = juce::jlimit(0.0f, 1.0f,
                                          firstSal / (tonalRefScale * (totalPeak + 1.0e-12f)));
        for(int b = 0; b < numBases; ++b)
        {
            const float rel = (firstSal > 0.0f)
                                ? juce::jlimit(0.0f, 1.0f, baseSal[(size_t) b] / firstSal) : 0.0f;
            baseConf[(size_t) b] = rel * tonal;
        }
        channelNumBases[(size_t) channel] = numBases;
        for(int base = 0; base < numBases; ++base)
        {
            channelBaseFreq[(size_t) channel][(size_t) base] = baseFreq[(size_t) base];
            channelBaseConf[(size_t) channel][(size_t) base] = baseConf[(size_t) base];
        }
    }
    void mergeDetectedBases() noexcept
    {
        numBases = 0;
        for(int channel = 0; channel < activeChannels; ++channel)
            for(int base = 0; base < channelNumBases[(size_t) channel] && numBases < MaxBases; ++base)
            {
                const float candidate = channelBaseFreq[(size_t) channel][(size_t) base];
                bool duplicate = false;
                for(int merged = 0; merged < numBases; ++merged)
                    if(std::abs(1200.0f * std::log2 (candidate / baseFreq[(size_t) merged])) < centsTol)
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
    std::array<std::vector<float>, maxChannels> scFifo;
    std::vector<float> fftData;
    std::vector<float> pvMag, pvFreq;
    std::array<std::vector<float>, maxChannels> channelMag, channelFreq, channelPhase, prevPhase;
    std::vector<float> peakFreq, peakAmp, peakWork;
    std::vector<float> baseFreq, baseSal, baseConf;
    std::array<std::array<float, MaxBases>, maxChannels> channelBaseFreq {};
    std::array<std::array<float, MaxBases>, maxChannels> channelBaseConf {};
    std::array<int, maxChannels> channelNumBases {};
    int numPeaks = 0;
    int numBases = 0;
    std::array<float, maxChannels> prevPrimary {};
    int activeChannels = 0;
    std::array<bool, maxChannels> analysisSeed {};
    std::atomic<int> lastNumBases { 0 };
    BridgeType bridge;
};