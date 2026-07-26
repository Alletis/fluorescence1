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
        scFifo.assign((size_t) maxFftSize, 0.0f);
        fftData.assign((size_t) (2 * maxFftSize), 0.0f);
        pvMag.assign((size_t) MaxBins, 0.0f);
        pvFreq.assign((size_t) MaxBins, 0.0f);
        pvPhase.assign((size_t) MaxBins, 0.0f);
        prevPhase.assign((size_t) MaxBins, 0.0f);
        peakFreq.assign((size_t) maxPeaks, 0.0f);
        peakAmp.assign((size_t) maxPeaks, 0.0f);
        peakWork.assign((size_t) maxPeaks, 0.0f);
        baseFreq.assign((size_t) MaxBases, 0.0f);
        baseSal.assign((size_t) MaxBases, 0.0f);
        baseConf.assign((size_t) MaxBases, 0.0f);
        analysisSeed = true;
        prevPrimary = 0.0f;
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
        std::fill(scFifo.begin(), scFifo.end(), 0.0f);
        std::fill(prevPhase.begin(), prevPhase.end(), 0.0f);
        analysisSeed = true;
        prevPrimary = 0.0f;
        numPeaks = numBases = 0;
    }
    void pushSample(int pos, float s) noexcept
    {
        scFifo[(size_t) pos] = s;
    }
    void processHop(int pos, const Params& p) noexcept
    {
        if(fft == nullptr || window == nullptr || numBins <= 0)
            return;
        float* fd = fftData.data();
        for(int i = 0; i < fftSize; ++i)
            fd[i] = scFifo[(size_t) ((pos + i) & fftMask)] * window[i];
        fft->performRealOnlyForwardTransform(fd, false);
        analyze();
        detect(p);
        publish(p.pitchRatio);
    }
    void processBypassHop(int pos) noexcept
    {
        if(fft == nullptr || window == nullptr || numBins <= 0)
            return;
        float* fd = fftData.data();
        for(int i = 0; i < fftSize; ++i)
            fd[i] = scFifo[(size_t) ((pos + i) & fftMask)] * window[i];
        fft->performRealOnlyForwardTransform(fd, false);
        analyze();
        numBases = 0;
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
        s.numBins = numBins;
        s.binWidth = binWidth;
        s.sampleRate = sampleRate;
        s.pvBypassed = bypassed;
        const int nb = juce::jmin(numBins, (int) s.mag.size());
        for(int j = 0; j < nb; ++j)
        {
            s.mag [(size_t) j] = pvMag [(size_t) j] * spectrumNorm;
            s.freq[(size_t) j] = pvFreq[(size_t) j];
        }
        s.numBases = 0;
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
    void analyze() noexcept
    {
        float* fd = fftData.data();
        const float twoPi = juce::MathConstants<float>::twoPi;
        const float expectPerBin = twoPi * (float) hopSize / (float) fftSize;
        const float freqScale = (float) (sampleRate / (twoPi * hopSize));
        for(int k = 0; k < numBins; ++k)
        {
            const float re = fd[2 * k];
            const float im = fd[2 * k + 1];
            pvPhase[(size_t) k] = std::atan2 (im, re);
            pvMag [(size_t) k] = std::sqrt(re * re + im * im);
        }
        if(analysisSeed)
        {
            for(int k = 0; k < numBins; ++k)
                prevPhase[(size_t) k] = wrapPhase(pvPhase[(size_t) k] - expectPerBin * (float) k);
            analysisSeed = false;
        }
        for(int k = 0; k < numBins; ++k)
        {
            const float dev = wrapPhase((pvPhase[(size_t) k] - prevPhase[(size_t) k]) - expectPerBin * (float) k);
            pvFreq[(size_t) k] = (float) k * binWidth + dev * freqScale;
            prevPhase[(size_t) k] = pvPhase[(size_t) k];
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
    void detect(const Params& p) noexcept
    {
        numPeaks = 0;
        numBases = 0;
        const int peakLimit = p.moreBases ? maxPeaks : defaultMaxPeaks;
        const int baseLimit = p.moreBases ? MaxBases : defaultMaxBases;
        const float salienceGateScale = p.moreBases ? 0.85f : 1.0f;
        const float gatedThr = juce::jmax(0.01f, p.thr * salienceGateScale);
        float maxMag = 0.0f;
        for(int k = 0; k < numBins; ++k) maxMag = std::max(maxMag, pvMag[(size_t) k]);
        if(maxMag < silenceMagFloor) { lastNumBases.store(0, std::memory_order_relaxed); return; }
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
        if(numPeaks == 0) { lastNumBases.store(0, std::memory_order_relaxed); return; }
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
                if(numBases == 0 && prevPrimary > 0.0f
                    && std::abs(1200.0f * std::log2 (Fc / prevPrimary)) < centsTol)
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
        if(numBases == 0) { lastNumBases.store(0, std::memory_order_relaxed); return; }
        prevPrimary = baseFreq[0];
        const float tonal = juce::jlimit(0.0f, 1.0f,
                                          firstSal / (tonalRefScale * (totalPeak + 1.0e-12f)));
        for(int b = 0; b < numBases; ++b)
        {
            const float rel = (firstSal > 0.0f)
                                ? juce::jlimit(0.0f, 1.0f, baseSal[(size_t) b] / firstSal) : 0.0f;
            baseConf[(size_t) b] = rel * tonal;
        }
        lastNumBases.store(numBases, std::memory_order_relaxed);
    }
    void publish(float pitchRatio) noexcept
    {
        auto& s = bridge.startWrite();
        s.numBins = numBins;
        s.binWidth = binWidth;
        s.sampleRate = sampleRate;
        s.pvBypassed = false;
        const int nb = juce::jmin(numBins, (int) s.mag.size());
        for(int j = 0; j < nb; ++j)
        {
            s.mag [(size_t) j] = pvMag [(size_t) j] * spectrumNorm;
            s.freq[(size_t) j] = pvFreq[(size_t) j] * pitchRatio;
        }
        const int nbases = juce::jmin(numBases, (int) s.baseHz.size());
        for(int i = 0; i < nbases; ++i)
        {
            s.baseHz [(size_t) i] = baseFreq[(size_t) i];
            s.baseConf[(size_t) i] = baseConf[(size_t) i];
        }
        s.numBases = nbases;
        bridge.publish();
    }
    void publishBypassed() noexcept
    {
        auto& s = bridge.startWrite();
        s.numBins = numBins;
        s.binWidth = binWidth;
        s.sampleRate = sampleRate;
        s.pvBypassed = true;
        const int nb = juce::jmin(numBins, (int) s.mag.size());
        for(int j = 0; j < nb; ++j)
        {
            s.mag [(size_t) j] = pvMag [(size_t) j] * spectrumNorm;
            s.freq[(size_t) j] = pvFreq[(size_t) j];
        }
        s.numBases = 0;
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
    std::vector<float> scFifo;
    std::vector<float> fftData;
    std::vector<float> pvMag, pvFreq, pvPhase, prevPhase;
    std::vector<float> peakFreq, peakAmp, peakWork;
    std::vector<float> baseFreq, baseSal, baseConf;
    int numPeaks = 0;
    int numBases = 0;
    float prevPrimary = 0.0f;
    bool analysisSeed = true;
    std::atomic<int> lastNumBases { 0 };
    BridgeType bridge;
};