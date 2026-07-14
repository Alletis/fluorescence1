#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "SpectrumBridge.h"
#include "SidechainDetector.h"
#include "TransientSplitter.h"
class NewProjectAudioProcessor : public juce::AudioProcessor,
                                  private juce::AsyncUpdater
{
public:
    NewProjectAudioProcessor();
    ~NewProjectAudioProcessor() override;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;
    juce::AudioProcessorValueTreeState apvts;
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void reconfigure(int newOrder, int newOverlap);
    void processFrame(int channel);
    void analyze(int channel);
    void processPV(int channel);
    void detect(int channel);
    float salience(float F, float centsTolerance) const;
    float salienceIdx(int candPeak, int harmHorizon = harmonicsMax) const;
    void buildHarmonicMap(float centsTolerance);
    std::vector<unsigned char> harmonicMap;
    float snapBaseToScale(float baseHz, float amount, float fineCents) const;
    float nearestTargetMidi(float baseHz) const;
    float snapToTargetMidi(float baseHz, float targetMidi, float amount) const;
    void applyEnvelopeCompensation();
    void applyFormantShift(int channel);
    void mapToDestination(int channel, bool computePhase);
    void buildSynthesisKernel();
    void synthesizeAdditive(int channel, bool reseed);
    float detectTransient(int channel);
    void updateTransientBypass(int channel);
    void publishSpectrum();
    float destinationFrequency(float sourceFreq, int bin) const;
    static float wrapPhase(float x) noexcept
    {
        return x - juce::MathConstants<float>::twoPi
                   * std::round(x / juce::MathConstants<float>::twoPi);
    }
    static inline float fastAtan2(float y, float x) noexcept
    {
        if(x == 0.0f && y == 0.0f) return 0.0f;
        const float ax = std::abs(x), ay = std::abs(y);
        const float a = std::min(ax, ay) / (std::max(ax, ay) + 1.0e-20f);
        const float s = a * a;
        float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
        if(ay > ax) r = 1.57079637f - r;
        if(x < 0.0f) r = 3.14159274f - r;
        if(y < 0.0f) r = -r;
        return r;
    }
    static inline void fastSinCos(float x, float& s, float& c) noexcept
    {
        constexpr float twoPi = 6.28318531f, invTwoPi = 0.159154943f;
        x -= twoPi * std::round(x * invTwoPi);
        const float x2 = x * x;
        s = x * (1.0f + x2 * (-0.166666667f + x2 * (0.00833333333f + x2 * (-0.000198412698f))));
        c = 1.0f + x2 * (-0.5f + x2 * (0.0416666667f + x2 * (-0.00138888889f + x2 * 0.0000248015873f)));
    }
    inline bool kernelAt(float d, float& kre, float& kim) const noexcept
    {
        const float tf = d * (float) kernelOS + (float) (kernelHalf * kernelOS);
        const int ti = (int) std::floor(tf);
        if(ti < 0 || ti + 1 >= (int) synKernelRe.size()) return false;
        const float fr = tf - (float) ti;
        kre = synKernelRe[(size_t) ti] + (synKernelRe[(size_t)(ti+1)] - synKernelRe[(size_t) ti]) * fr;
        kim = synKernelIm[(size_t) ti] + (synKernelIm[(size_t)(ti+1)] - synKernelIm[(size_t) ti]) * fr;
        return true;
    }
    static inline float fastLog2(float x) noexcept
    {
        union { float f; juce::uint32 i; } vx = { x };
        union { juce::uint32 i; float f; } mx = { (vx.i & 0x007FFFFFu) | 0x3f000000u };
        float y = (float) vx.i;
        y *= 1.1920928955078125e-7f;
        return y - 124.22551499f - 1.498030302f * mx.f
                 - 1.72587999f / (0.3520887068f + mx.f);
    }
    static inline float fastPow2(float p) noexcept
    {
        const float offset = (p < 0.0f) ? 1.0f : 0.0f;
        const float clipp = (p < -126.0f) ? -126.0f : p;
        const int w = (int) clipp;
        const float z = clipp - (float) w + offset;
        union { juce::uint32 i; float f; } v = {
            (juce::uint32) ((1 << 23) * (clipp + 121.2740575f
                            + 27.7280233f / (4.84252568f - z)
                            - 1.49012907f * z)) };
        return v.f;
    }
    static inline float fastPow(float x, float p) noexcept { return fastPow2(p * fastLog2(x)); }
    static constexpr const char* stateVersion = "1.1.2";
    static constexpr int minOrder = 9;
    static constexpr int maxOrder = 13;
    static constexpr int maxFftSize = 1 << maxOrder;
    static constexpr int maxBins = maxFftSize / 2 + 1;
    static constexpr int maxChannels = 2;
    static constexpr float peakRelThreshold = 0.01f;
    static constexpr float transientReseedThreshold = 0.35f;
    static constexpr float fluxBaselineAlpha = 0.1f;
    static constexpr float fluxThresholdMult = 2.5f;
    static constexpr float fluxThresholdFloor = 0.01f;
    static constexpr int fluxHoldFrames = 3;
    static constexpr float fluxHoldDecay = 0.6f;
    static constexpr float fluxLogScale = 1.0f;
    static constexpr float transientBinRatioScale = 1.0f;
    static constexpr int defaultMaxPeaks = 64;
    static constexpr int extendedMaxPeaks = 128;
    static constexpr int defaultMaxBases = 16;
    static constexpr int extendedMaxBases = 48;
    static constexpr int maxPeaks = extendedMaxPeaks;
    static constexpr int maxBases = extendedMaxBases;
    static constexpr int harmonicsMax = 32;
    static constexpr int nonMidHarmonics = 16;
    static constexpr int harmonicsTagMax = 256;
    static constexpr float centsTol = 35.0f;
    static constexpr float centsTolMax = 70.0f;
    static constexpr float proximitySigmaCents = 100.0f;
    static constexpr float membershipSigmaCents = 45.0f;
    static constexpr float membershipGapFrac = 0.35f;
    static constexpr float shiftGapFrac = 0.15f;
    static constexpr float identifyGapFrac = 0.25f;
    static constexpr float pullGapFrac = 0.75f;
    static constexpr float salAlpha = 52.0f;
    static constexpr float salBeta = 320.0f;
    static constexpr float octaveBias = 0.80f;
    static constexpr float continuityBias = 0.30f;
    static constexpr float peakFloorRel = 0.02f;
    static constexpr float silenceMagFloor = 1.0e-4f;
    static constexpr float tonalRefScale = 0.35f;
    static constexpr int formantEnvIters = 4;
    static constexpr int formantMinIters = 2;
    static constexpr float formantConvergeLog = 0.01f;
    static constexpr float formantDynRangeDb = 55.0f;
    static constexpr float formantF0MinHz = 55.0f;
    static constexpr float formantF0MaxHz = 1000.0f;
    static constexpr float formantLiftSafety = 0.6f;
    static constexpr float formantDefaultDetailHz = 380.0f;
    std::vector<std::unique_ptr<juce::dsp::FFT>> fftEngines;
    double sampleRate = 44100.0;
    int currentOrder = -1;
    int currentOverlap = -1;
    int fftSize = 0;
    int fftMask = 0;
    int hopSize = 0;
    int numBins = 0;
    float binWidth = 0.0f;
    float windowCorrection = 1.0f;
    float spectrumNorm = 1.0f;
    float pitchRatio = 1.0f;
    float formantRatio = 1.0f;
    std::vector<float> window;
    std::vector<float> fftData;
    std::array<std::vector<float>, maxChannels> inputFifo;
    std::array<std::vector<float>, maxChannels> outputFifo;
    std::array<std::vector<float>, maxChannels> prevPhase;
    std::array<std::vector<float>, maxChannels> outPhase;
    std::array<std::vector<float>, maxChannels> prevLogMag;
    std::array<std::vector<float>, maxChannels> prevDstMag;
    std::array<std::vector<float>, maxChannels> prevDstFreq;
    std::array<std::vector<float>, maxChannels> prevPreFbMag;
    std::array<std::vector<float>, maxChannels> capHoldMag;
    std::array<std::vector<float>, maxChannels> prevPvMag;
    std::array<std::vector<float>, maxChannels> transientBypassRe;
    std::array<std::vector<float>, maxChannels> transientBypassIm;
    std::array<std::vector<unsigned char>, maxChannels> transientBypassMask;
    transientsplit::TransientMask tsMask[maxChannels];
    std::vector<float> tsMaskBuf;
    std::vector<float> tsScratch;
    std::vector<float> tsDispMag;
    bool tsActive = false;
    std::array<float, maxChannels> fluxBaseline {};
    std::array<float, maxChannels> heldStrength {};
    std::array<int, maxChannels> holdRemaining {};
    std::array<bool, maxChannels> transientInit {};
    std::array<bool, maxChannels> analysisSeed {};
    std::array<bool, maxChannels> synthSeed {};
    std::vector<float> pvMag, pvFreq, pvPhase;
    std::vector<float> curLogMag;
    std::vector<float> dstMag, dstFreqNum, dstFreq, dstPhase, dstBestMag;
    std::vector<float> dstHitCount;
    std::vector<float> dstRetuneWeight;
    std::vector<float> dstRetunePeak;
    std::vector<float> feedbackAddedMag;
    std::vector<float> preFeedbackMag;
    std::vector<float> envRefMag, envRefPrefix, envOutPrefix;
    std::vector<float> fmtRawLog, fmtLog, fmtEnvLog, fmtEnv;
    std::array<std::vector<float>, maxChannels> formantGain;
    std::array<std::vector<float>, maxChannels> nextFormantGain;
    std::array<int, maxChannels> formantHopCounter {};
    std::array<float, maxChannels> formantLastSemis {};
    std::array<bool, maxChannels> formantGainValid {};
    std::vector<juce::dsp::Complex<float>> fmtCepIn, fmtCepOut;
    std::vector<int> peaks;
    std::vector<int> peakBin;
    std::vector<float> peakFreq, peakAmp, peakAmpWork, peakRatio;
    std::vector<float> peakTargetAffinity;
    std::vector<int> peakBaseIdx, peakHarmonic;
    struct Partial
    {
        int id = 0;
        float freqOut = 0.0f;
        float freqNat = 0.0f;
        float freqAna = 0.0f;
        float amp = 0.0f;
        float phase = 0.0f;
        float anaPhase = 0.0f;
        float targetFreq = 0.0f;
        float targetAmp = 0.0f;
        int baseIdx = -1;
        int harmonic = 0;
        int age = 0;
        int dying = 0;
        bool matched = false;
    };
    struct BasePhaseRef
    {
        float f0Ana = 0.0f;
        float f0Out = 0.0f;
        float delta = 0.0f;
        int missing = 0;
        bool matched = false;
    };
    struct MatchCand { float dist; int part; int peak; };
    static constexpr float partialMatchTolCents = 60.0f;
    static constexpr float partialMatchTolHz = 15.0f;
    static constexpr int partialDeathFrames = 4;
    std::array<std::vector<Partial>, maxChannels> partials;
    std::array<int, maxChannels> nextPartialId {};
    std::vector<int> peakMatchedPartial;
    std::vector<MatchCand> matchCands;
    int64_t frameCounter = 0;
    static constexpr int kernelHalf = 3;
    static constexpr int kernelOS = 512;
    static constexpr int partialBirthFrames = 4;
    static constexpr float basePhaseMatchCents = 60.0f;
    static constexpr int basePhaseDeathFrames = 4;
    std::array<std::vector<BasePhaseRef>, maxChannels> basePhase;
    std::vector<float> synKernelRe, synKernelIm;
    std::vector<float> synKernelMag;
    std::vector<float> accRe, accIm;
    std::vector<float> accCov;
    void updatePartials(int channel);
    void identifyHarmonic(float freqAnalysis, int& baseIdxOut, int& harmonicOut) const;
    std::vector<float> baseFreq;
    std::vector<float> baseSal;
    std::vector<float> baseDisplayHz;
    std::vector<float> baseConf;
    std::vector<float> baseTargetMidi;
    std::vector<float> baseInvFreq, baseSnapRatio, baseSnapHz, baseAffinityV;
    std::vector<float> gapCentsTable;
    std::vector<float> invIntTable;
    std::vector<float> log2IntTable;
    std::vector<float> baseLog2Hz;
    std::vector<float> baseSnapLog2Hz;
    std::vector<float> baseSnapLog2Ratio;
    std::vector<float> binBestDev;
    struct TrackedBase
    {
        float freq = 0.0f;
        float conf = 0.0f;
        bool active = false;
        bool claimed = false;
        int age = 0;
        int missing = 0;
        float heldTargetMidi = -1000.0f;
        float sal = 0.0f;
    };
    static constexpr int maxTracked = maxBases;
    std::array<std::array<TrackedBase, maxTracked>, maxChannels> tracked {};
    void updateBaseTracker(int channel, float frameTonal);
    float membershipSigma(int harmonic, float freqHz) const;
    static constexpr int subBaseBase = 100000;
    std::vector<float> subBaseFreq;
    std::vector<float> subBaseSal;
    int subNumBases = 0;
    std::array<float, maxChannels> subPrevPrimary {};
    static constexpr int hiBaseBase = 200000;
    std::vector<float> hiBaseFreq;
    std::vector<float> hiBaseSal;
    int hiNumBases = 0;
    std::array<float, maxChannels> hiPrevPrimary {};
    std::vector<float> binDestFreq;
    std::vector<float> binTargetAffinity;
    int numPeaks = 0;
    int numBases = 0;
    bool scaleOn[12] = { false };
    std::array<float, maxChannels> prevPrimary {};
    int pos = 0;
    int count = 0;
    std::atomic<float>* sizeParam = nullptr;
    std::atomic<float>* overlapParam = nullptr;
    std::atomic<float>* msParam = nullptr;
    std::atomic<float>* transientParam = nullptr;
    std::atomic<float>* enhanceTransientParam = nullptr;
    std::atomic<float>* morphParam = nullptr;
    std::atomic<float>* pitchParam = nullptr;
    std::atomic<float>* formantParam = nullptr;
    std::atomic<float>* emphasisParam = nullptr;
    std::atomic<float>* attractionParam = nullptr;
    std::atomic<float>* densityParam = nullptr;
    std::atomic<float>* feedbackParam = nullptr;
    std::atomic<float>* fineTuneParam = nullptr;
    std::atomic<float>* envCompParam = nullptr;
    std::atomic<float>* centerParam = nullptr;
    std::atomic<float>* spreadParam = nullptr;
    std::atomic<float>* noteParam[12] = { nullptr };
    std::atomic<float>* bypassParam = nullptr;
    std::atomic<float>* moreBasesParam = nullptr;
    std::atomic<float>* scPitchParam = nullptr;
    std::atomic<float>* fullMidiParam = nullptr;
    std::atomic<float>* targetModeParam = nullptr;
    std::atomic<float>* hysteresisParam = nullptr;
    int midiHeld[12] = { 0 };
    int midiHeldNote[128] = { 0 };
    int lastMidiMask = -1;
    std::atomic<int> midiScaleMask { 0 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NewProjectAudioProcessor)
    void handleAsyncUpdate() override;
    std::atomic<int> pendingLatency { 0 };
public:
    using SpectrumBridgeType = SpectrumBridge<maxBins, maxBases>;
    SpectrumBridgeType& getSpectrumBridge() noexcept { return spectrumBridge; }
    SpectrumBridgeType& getSidechainSpectrumBridge() noexcept { return scDetector.getBridge(); }
    int getSidechainBaseCount() const noexcept { return scDetector.debugNumBases(); }
    const std::atomic<int>& getSidechainTargetCount() const noexcept { return scDetector.numBasesAtomic(); }
    const std::atomic<int>& getMidiScaleMask() const noexcept { return midiScaleMask; }
    float getSidechainInputPeak() const noexcept
        { return scInputPeak.load(std::memory_order_relaxed); }
private:
    SpectrumBridgeType spectrumBridge;
    std::atomic<float> scInputPeak { 0.0f };
    SidechainDetector<maxBins, maxBases> scDetector;
    std::array<float, maxBases> scTargetHz {};
    int scNumTargets = 0;
};