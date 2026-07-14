#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <complex>
namespace
{
inline int overlapFromIndex(int idx) { return idx == 0 ? 4 : (idx == 1 ? 8 : 16); }
inline float densityToSalienceThreshold(float density01)
{
    density01 = juce::jlimit(0.0f, 1.0f, density01);
    const float kneeStart = 0.85f;
    const float t = juce::jlimit(0.0f, 1.0f, (density01 - kneeStart) / (1.0f - kneeStart));
    const float s = t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    const float boost = 1.0f;
    density01 = density01 + boost * s * (1.0f - density01);
    return 0.9f - density01 * (0.9f - 0.002f);
}
}
NewProjectAudioProcessor::NewProjectAudioProcessor()
     : AudioProcessor(BusesProperties()
                       .withInput("Input", juce::AudioChannelSet::stereo(), true)
                       .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                       .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)),
       apvts(*this, nullptr, "PARAMS", createLayout())
{
    sizeParam = apvts.getRawParameterValue("fftSize");
    overlapParam = apvts.getRawParameterValue("overlap");
    msParam = apvts.getRawParameterValue("midSide");
    transientParam = apvts.getRawParameterValue("transient");
    enhanceTransientParam = apvts.getRawParameterValue("enhanceTransient");
    morphParam = apvts.getRawParameterValue("morph");
    pitchParam = apvts.getRawParameterValue("pitch");
    emphasisParam = apvts.getRawParameterValue("emphasis");
    attractionParam = apvts.getRawParameterValue("attraction");
    densityParam = apvts.getRawParameterValue("density");
    feedbackParam = apvts.getRawParameterValue("feedback");
    fineTuneParam = apvts.getRawParameterValue("fineTune");
    envCompParam = apvts.getRawParameterValue("envComp");
    centerParam = apvts.getRawParameterValue("detectCenter");
    spreadParam = apvts.getRawParameterValue("detectSpread");
    bypassParam = apvts.getRawParameterValue("pvBypass");
    moreBasesParam = apvts.getRawParameterValue("moreBases");
    scPitchParam = apvts.getRawParameterValue("scPitch");
    formantParam = apvts.getRawParameterValue("formant");
    static const char* noteIds[12] = { "note0","note1","note2","note3","note4","note5",
                                       "note6","note7","note8","note9","note10","note11" };
    for(int i = 0; i < 12; ++i)
        noteParam[i] = apvts.getRawParameterValue(noteIds[i]);
    fullMidiParam = apvts.getRawParameterValue("fullMidi");
    targetModeParam = apvts.getRawParameterValue("targetMode");
    hysteresisParam = apvts.getRawParameterValue("hysteresis");
}
NewProjectAudioProcessor::~NewProjectAudioProcessor()
{
    cancelPendingUpdate();
}
void NewProjectAudioProcessor::handleAsyncUpdate()
{
    setLatencySamples(pendingLatency.load());
}
juce::AudioProcessorValueTreeState::ParameterLayout
NewProjectAudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    juce::StringArray sizes;
    for(int order = minOrder; order <= maxOrder; ++order)
        sizes.add(juce::String(1 << order));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "fftSize", 1 }, "FFT Size", sizes, 11 - minOrder));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "overlap", 1 }, "Overlap",
        juce::StringArray { "4x", "8x", "16x" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "midSide", 1 }, "M/S Mode", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "fullMidi", 1 }, "Full MIDI", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "scPitch", 1 }, "SC Transpose",
        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.0f }, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "pvBypass", 1 }, "On/Off", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "moreBases", 1 }, "More Bases", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "transient", 1 }, "Transient",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.0f }, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "enhanceTransient", 1 }, "Enhance Transient", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "morph", 1 }, "Morph",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.0f }, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "pitch", 1 }, "Transpose",
        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.0f }, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "formant", 1 }, "Formant",
        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.0f }, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "emphasis", 1 }, "Emphasis",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.0f }, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "attraction", 1 }, "Attraction",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.0f }, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "density", 1 }, "Density",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.0f }, 0.8f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "feedback", 1 }, "Feedback",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.0f }, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "fineTune", 1 }, "Fine Tune",
        juce::NormalisableRange<float> { -100.0f, 100.0f, 0.0f }, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "envComp", 1 }, "Compensation",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.0f }, 0.2f));
    juce::NormalisableRange<float> centerRange { 20.0f, 20000.0f, 0.0f };
    centerRange.setSkewForCentre(640.0f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "detectCenter", 1 }, "Detect Center", centerRange, 640.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "detectSpread", 1 }, "Detect Spread(oct)",
        juce::NormalisableRange<float> { 0.25f, 5.0f, 0.0f }, 2.5f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "targetMode", 1 }, "Target Mode",
        juce::StringArray { "Scale", "MIDI", "Sidechain" }, 0));
    static const char* noteNames[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    for(int i = 0; i < 12; ++i)
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { juce::String("note") + juce::String(i), 1 },
            juce::String("Note ") + noteNames[i], false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "hysteresis", 1 }, "Hysteresis", false));
    return { params.begin(), params.end() };
}
const juce::String NewProjectAudioProcessor::getName() const { return "NewProject"; }
bool NewProjectAudioProcessor::acceptsMidi() const { return true; }
bool NewProjectAudioProcessor::producesMidi() const { return false; }
bool NewProjectAudioProcessor::isMidiEffect() const { return false; }
double NewProjectAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int NewProjectAudioProcessor::getNumPrograms() { return 1; }
int NewProjectAudioProcessor::getCurrentProgram() { return 0; }
void NewProjectAudioProcessor::setCurrentProgram(int) {}
const juce::String NewProjectAudioProcessor::getProgramName(int) { return {}; }
void NewProjectAudioProcessor::changeProgramName(int, const juce::String&) {}
void NewProjectAudioProcessor::prepareToPlay(double sr, int)
{
    sampleRate = sr;
    window.assign(maxFftSize, 0.0f);
    fftData.assign(2 * maxFftSize, 0.0f);
    for(auto& f : inputFifo) f.assign(maxFftSize, 0.0f);
    for(auto& f : outputFifo) f.assign(maxFftSize, 0.0f);
    for(auto& f : prevPhase) f.assign(maxBins, 0.0f);
    for(auto& f : outPhase) f.assign(maxBins, 0.0f);
    for(auto& f : prevLogMag) f.assign(maxBins, 0.0f);
    for(auto& f : prevDstMag) f.assign(maxBins, 0.0f);
    for(auto& f : prevDstFreq) f.assign(maxBins, 0.0f);
    for(auto& f : prevPreFbMag) f.assign(maxBins, 0.0f);
    for(auto& f : capHoldMag) f.assign(maxBins, 0.0f);
    for(auto& f : prevPvMag) f.assign(maxBins, 0.0f);
    for(auto& f : transientBypassRe) f.assign(maxBins, 0.0f);
    for(auto& f : transientBypassIm) f.assign(maxBins, 0.0f);
    for(auto& f : transientBypassMask) f.assign(maxBins, (unsigned char) 0);
    for(int ch = 0; ch < maxChannels; ++ch)
    {
        tsMask[(size_t) ch].setBalance(0.5f);
        tsMask[(size_t) ch].setSeparation(0.85f);
        tsMask[(size_t) ch].setHold(0.2f);
        tsMask[(size_t) ch].setSmooth(0.4f);
    }
    tsMaskBuf.assign(maxBins, 0.0f);
    tsScratch.assign(2 * maxFftSize, 0.0f);
    tsDispMag.assign(maxBins, 0.0f);
    pvMag.assign(maxBins, 0.0f);
    pvFreq.assign(maxBins, 0.0f);
    pvPhase.assign(maxBins, 0.0f);
    curLogMag.assign(maxBins, 0.0f);
    dstMag.assign(maxBins, 0.0f);
    dstFreqNum.assign(maxBins, 0.0f);
    dstFreq.assign(maxBins, 0.0f);
    dstPhase.assign(maxBins, 0.0f);
    dstBestMag.assign(maxBins, 0.0f);
    dstHitCount.assign(maxBins, 0.0f);
    dstRetuneWeight.assign(maxBins, 0.0f);
    dstRetunePeak.assign(maxBins, 0.0f);
    feedbackAddedMag.assign(maxBins, 0.0f);
    preFeedbackMag.assign(maxBins, 0.0f);
    accRe.assign(maxBins, 0.0f);
    accIm.assign(maxBins, 0.0f);
    accCov.assign(maxBins, 0.0f);
    envRefMag.assign(maxBins, 0.0f);
    envRefPrefix.assign(maxBins + 1, 0.0f);
    envOutPrefix.assign(maxBins + 1, 0.0f);
    binDestFreq.assign(maxBins, 0.0f);
    binTargetAffinity.assign(maxBins, 0.0f);
    peaks.clear();
    peaks.reserve(maxBins);
    peakBin.assign(maxPeaks, 0);
    peakFreq.assign(maxPeaks, 0.0f);
    peakAmp.assign(maxPeaks, 0.0f);
    peakAmpWork.assign(maxPeaks, 0.0f);
    peakRatio.assign(maxPeaks, 1.0f);
    peakTargetAffinity.assign(maxPeaks, 0.0f);
    peakBaseIdx.assign(maxPeaks, -1);
    peakHarmonic.assign(maxPeaks, 0);
    harmonicMap.assign((size_t) maxPeaks * maxPeaks, 0);
    for(auto& v : partials) { v.clear(); v.reserve((size_t) maxPeaks * 2); }
    nextPartialId.fill(0);
    peakMatchedPartial.assign(maxPeaks, -1);
    matchCands.clear();
    matchCands.reserve((size_t) maxPeaks * 8);
    frameCounter = 0;
    baseFreq.assign(maxBases, 0.0f);
    baseSal.assign(maxBases, 0.0f);
    baseDisplayHz.assign(maxBases, 0.0f);
    baseConf.assign(maxBases, 0.0f);
    baseTargetMidi.assign(maxBases, -1000.0f);
    baseInvFreq.assign(maxBases, 0.0f);
    baseSnapRatio.assign(maxBases, 0.0f);
    baseSnapHz.assign(maxBases, 0.0f);
    baseAffinityV.assign(maxBases, 0.0f);
    gapCentsTable.assign(harmonicsTagMax + 2, 0.0f);
    for(int n = 1; n <= harmonicsTagMax + 1; ++n)
        gapCentsTable[(size_t) n] = 1200.0f * std::log2 ((float)(n + 1) / (float) n);
    invIntTable.assign(harmonicsTagMax + 2, 0.0f);
    for(int n = 1; n <= harmonicsTagMax + 1; ++n)
        invIntTable[(size_t) n] = 1.0f / (float) n;
    log2IntTable.assign(harmonicsTagMax + 2, 0.0f);
    for(int n = 1; n <= harmonicsTagMax + 1; ++n)
        log2IntTable[(size_t) n] = std::log2 ((float) n);
    baseLog2Hz.assign(maxBases, 0.0f);
    baseSnapLog2Hz.assign(maxBases, 0.0f);
    baseSnapLog2Ratio.assign(maxBases, 0.0f);
    binBestDev.assign(maxBins, 1.0e9f);
    subBaseFreq.assign(maxBases, 0.0f);
    subBaseSal.assign(maxBases, 0.0f);
    hiBaseFreq.assign(maxBases, 0.0f);
    hiBaseSal.assign(maxBases, 0.0f);
    fmtRawLog.assign(maxBins, 0.0f);
    fmtLog.assign(maxBins, 0.0f);
    fmtEnvLog.assign(maxBins, 0.0f);
    fmtEnv.assign(maxBins, 0.0f);
    for(auto& f : formantGain) f.assign(maxBins, 1.0f);
    for(auto& f : nextFormantGain) f.assign(maxBins, 1.0f);
    formantHopCounter.fill(0);
    formantLastSemis.fill(0.0f);
    formantGainValid.fill(false);
    fmtCepIn.assign(maxFftSize, {});
    fmtCepOut.assign(maxFftSize, {});
    fftEngines.clear();
    for(int order = minOrder; order <= maxOrder; ++order)
        fftEngines.push_back(std::make_unique<juce::dsp::FFT> (order));
    currentOrder = -1;
    currentOverlap = -1;
    scDetector.prepare(maxFftSize);
    reconfigure(minOrder + (int) sizeParam->load(), overlapFromIndex((int) overlapParam->load()));
}
void NewProjectAudioProcessor::releaseResources() { }
bool NewProjectAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    using ACS = juce::AudioChannelSet;
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();
    if(mainOut != ACS::mono() && mainOut != ACS::stereo())
        return false;
    if(mainIn != mainOut)
        return false;
    const auto sc = layouts.getChannelSet(true, 1);
    if(! sc.isDisabled() && sc != ACS::mono() && sc != ACS::stereo())
        return false;
    return true;
}
void NewProjectAudioProcessor::reconfigure(int newOrder, int newOverlap)
{
    if(newOrder == currentOrder && newOverlap == currentOverlap)
        return;
    currentOrder = newOrder;
    currentOverlap = newOverlap;
    fftSize = 1 << newOrder;
    fftMask = fftSize - 1;
    hopSize = fftSize / newOverlap;
    numBins = fftSize / 2 + 1;
    binWidth = (float) (sampleRate / fftSize);
    double sumOfSquares = 0.0;
    double windowSum = 0.0;
    for(int i = 0; i < fftSize; ++i)
    {
        const float w = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi
                                                 * (float) i / (float) fftSize));
        window[(size_t) i] = w;
        sumOfSquares += (double) w * (double) w;
        windowSum += (double) w;
    }
    windowCorrection = (float) ((double) hopSize / sumOfSquares);
    spectrumNorm = (windowSum > 0.0) ? (float) (2.0 / windowSum) : 1.0f;
    buildSynthesisKernel();
    for(auto& v : partials) v.clear();
    for(auto& v : basePhase) v.clear();
    nextPartialId.fill(0);
    for(auto& f : inputFifo) std::fill(f.begin(), f.end(), 0.0f);
    for(auto& f : outputFifo) std::fill(f.begin(), f.end(), 0.0f);
    for(auto& f : prevPhase) std::fill(f.begin(), f.end(), 0.0f);
    for(auto& f : outPhase) std::fill(f.begin(), f.end(), 0.0f);
    for(auto& f : prevLogMag) std::fill(f.begin(), f.end(), 0.0f);
    for(auto& f : prevDstMag) std::fill(f.begin(), f.end(), 0.0f);
    for(auto& f : prevDstFreq) std::fill(f.begin(), f.end(), 0.0f);
    for(auto& f : prevPreFbMag) std::fill(f.begin(), f.end(), 0.0f);
    for(auto& f : capHoldMag) std::fill(f.begin(), f.end(), 0.0f);
    for(auto& f : prevPvMag) std::fill(f.begin(), f.end(), 0.0f);
    for(auto& f : transientBypassRe) std::fill(f.begin(), f.end(), 0.0f);
    for(auto& f : transientBypassIm) std::fill(f.begin(), f.end(), 0.0f);
    for(auto& f : transientBypassMask) std::fill(f.begin(), f.end(), (unsigned char) 0);
    std::fill(binTargetAffinity.begin(), binTargetAffinity.end(), 0.0f);
    std::fill(peakTargetAffinity.begin(), peakTargetAffinity.end(), 0.0f);
    for(int ch = 0; ch < maxChannels; ++ch)
    {
        fluxBaseline[(size_t) ch] = 0.0f;
        heldStrength[(size_t) ch] = 0.0f;
        holdRemaining[(size_t) ch] = 0;
        transientInit[(size_t) ch] = false;
        analysisSeed[(size_t) ch] = true;
        synthSeed[(size_t) ch] = true;
        prevPrimary[(size_t) ch] = 0.0f;
        subPrevPrimary[(size_t) ch] = 0.0f;
        formantGainValid[(size_t) ch] = false;
        formantHopCounter[(size_t) ch] = 0;
    }
    scDetector.configure(fftEngines[(size_t) (currentOrder - minOrder)].get(),
                          window.data(), fftSize, hopSize, numBins,
                          binWidth, sampleRate, spectrumNorm);
    pos = 0;
    count = 0;
    for(int ch = 0; ch < maxChannels; ++ch) { tsMask[(size_t) ch].prepare(numBins); tsMask[(size_t) ch].setOverlap(newOverlap); }
    pendingLatency.store(fftSize);
    triggerAsyncUpdate();
}
void NewProjectAudioProcessor::buildSynthesisKernel()
{
    const int N = fftSize;
    const int L = 2 * kernelHalf * kernelOS + 1;
    const double pi = juce::MathConstants<double>::pi;
    synKernelRe.assign((size_t) L, 0.0f);
    synKernelIm.assign((size_t) L, 0.0f);
    auto D = [N, pi] (double d) -> std::complex<double>
    {
        const double ph = -pi * d * (double) (N - 1) / (double) N;
        const std::complex<double> lin(std::cos(ph), std::sin(ph));
        const double den = std::sin(pi * d / (double) N);
        const double mag = (std::abs(den) < 1.0e-12) ? (double) N
                                                      : std::sin(pi * d) / den;
        return lin * mag;
    };
    const double W0 = 0.5 * (double) N;
    for(int t = 0; t < L; ++t)
    {
        const double d = (double) (t - kernelHalf * kernelOS) / (double) kernelOS;
        std::complex<double> W = 0.5 * D(d) - 0.25 * D(d - 1.0) - 0.25 * D(d + 1.0);
        W /= W0;
        synKernelRe[(size_t) t] = (float) W.real();
        synKernelIm[(size_t) t] = (float) W.imag();
    }
    synKernelMag.assign((size_t) L, 0.0f);
    for(int t = 0; t < L; ++t)
        synKernelMag[(size_t) t] = std::sqrt(synKernelRe[(size_t) t] * synKernelRe[(size_t) t]
                                           + synKernelIm[(size_t) t] * synKernelIm[(size_t) t]);
}
void NewProjectAudioProcessor::analyze(int channel)
{
    float* fd = fftData.data();
    float* prevPh = prevPhase[(size_t) channel].data();
    const float twoPi = juce::MathConstants<float>::twoPi;
    const float expectPerBin = twoPi * (float) hopSize / (float) fftSize;
    const float freqScale = (float) (sampleRate / (twoPi * hopSize));
    if(analysisSeed[(size_t) channel])
    {
        for(int k = 0; k < numBins; ++k)
        {
            const float re = fd[2 * k];
            const float im = fd[2 * k + 1];
            pvPhase[(size_t) k] = fastAtan2 (im, re);
            pvMag[(size_t) k] = std::sqrt(re * re + im * im);
        }
        for(int k = 0; k < numBins; ++k)
            prevPh[k] = wrapPhase(pvPhase[(size_t) k] - expectPerBin * (float) k);
        analysisSeed[(size_t) channel] = false;
        for(int k = 0; k < numBins; ++k)
        {
            const float dev = wrapPhase((pvPhase[(size_t) k] - prevPh[k]) - expectPerBin * (float) k);
            pvFreq[(size_t) k] = (float) k * binWidth + dev * freqScale;
            prevPh[k] = pvPhase[(size_t) k];
        }
        return;
    }
    for(int k = 0; k < numBins; ++k)
    {
        const float re = fd[2 * k];
        const float im = fd[2 * k + 1];
        const float ph = fastAtan2 (im, re);
        pvPhase[(size_t) k] = ph;
        pvMag[(size_t) k] = std::sqrt(re * re + im * im);
        const float dev = wrapPhase((ph - prevPh[k]) - expectPerBin * (float) k);
        pvFreq[(size_t) k] = (float) k * binWidth + dev * freqScale;
        prevPh[k] = ph;
    }
}
float NewProjectAudioProcessor::detectTransient(int channel)
{
    float* prevLog = prevLogMag[(size_t) channel].data();
    if(! transientInit[(size_t) channel])
    {
        for(int k = 0; k < numBins; ++k)
            prevLog[k] = std::log1p(fluxLogScale * std::max(0.0f, pvMag[(size_t) k]));
        fluxBaseline[(size_t) channel] = 0.0f;
        heldStrength[(size_t) channel] = 0.0f;
        holdRemaining[(size_t) channel] = 0;
        transientInit[(size_t) channel] = true;
        return 0.0f;
    }
    float flux = 0.0f;
    for(int k = 0; k < numBins; ++k)
    {
        const float cur = std::log1p(fluxLogScale * std::max(0.0f, pvMag[(size_t) k]));
        flux += std::max(0.0f, cur - prevLog[k]);
        prevLog[(size_t) k] = cur;
    }
    flux /= (float) numBins;
    const float hopRef = (float) fftSize * 0.25f;
    const float overlapNorm = (hopSize > 0) ? (hopRef / (float) hopSize) : 1.0f;
    flux *= overlapNorm;
    const float baseAlpha = juce::jlimit(0.0f, 1.0f, fluxBaselineAlpha / overlapNorm);
    const int holdFrames = juce::jmax(1, (int) std::lround((float) fluxHoldFrames * overlapNorm));
    const float holdDecay = std::pow(fluxHoldDecay, 1.0f / juce::jmax(1.0f, overlapNorm));
    const float threshold = std::max(fluxThresholdFloor,
                                      fluxBaseline[(size_t) channel] * fluxThresholdMult);
    float strength = 0.0f;
    if(flux > threshold)
    {
        const float denom = std::max(threshold + fluxThresholdFloor, 1.0e-6f);
        strength = juce::jlimit(0.0f, 1.0f, (flux - threshold) / denom);
        heldStrength[(size_t) channel] = strength;
        holdRemaining[(size_t) channel] = holdFrames;
    }
    else if(holdRemaining[(size_t) channel] > 0)
    {
        heldStrength[(size_t) channel] *= holdDecay;
        --holdRemaining[(size_t) channel];
    }
    else
    {
        heldStrength[(size_t) channel] = 0.0f;
    }
    const float result = juce::jlimit(0.0f, 1.0f,
                                       std::max(strength, heldStrength[(size_t) channel]));
    fluxBaseline[(size_t) channel] += baseAlpha * (flux - fluxBaseline[(size_t) channel]);
    return result;
}
void NewProjectAudioProcessor::updateTransientBypass(int channel)
{
    auto& prevMag = prevPvMag[(size_t) channel];
    auto& bypassRe = transientBypassRe[(size_t) channel];
    auto& bypassIm = transientBypassIm[(size_t) channel];
    auto& bypassMask = transientBypassMask[(size_t) channel];
    std::fill(bypassRe.begin(), bypassRe.begin() + numBins, 0.0f);
    std::fill(bypassIm.begin(), bypassIm.begin() + numBins, 0.0f);
    std::fill(bypassMask.begin(), bypassMask.begin() + numBins, (unsigned char) 0);
    const bool enhance = (enhanceTransientParam != nullptr && enhanceTransientParam->load() >= 0.5f);
    const float unlock = enhance ? 0.0f : juce::jlimit(0.0f, 1.0f, transientParam->load());
    if(unlock <= 0.0f)
    {
        std::copy(pvMag.begin(), pvMag.begin() + numBins, prevMag.begin());
        return;
    }
    const float hopRef = (float) fftSize * 0.25f;
    const float overlapNorm = (hopSize > 0) ? (hopRef / (float) hopSize) : 1.0f;
    const float ratioBase = transientBinRatioScale / (unlock * unlock);
    const float thresh = 1.0f + (ratioBase - 1.0f) / juce::jmax(1.0f, overlapNorm);
    float* fd = fftData.data();
    for(int k = 0; k < numBins; ++k)
    {
        const float m = pvMag[(size_t) k];
        const float pm = prevMag[(size_t) k];
        if(pm > 1.0e-9f && m > thresh * pm)
        {
            bypassMask[(size_t) k] = (unsigned char) 1;
            bypassRe[(size_t) k] = fd[2 * k];
            bypassIm[(size_t) k] = fd[2 * k + 1];
        }
        prevMag[(size_t) k] = m;
    }
}
void NewProjectAudioProcessor::applyFormantShift(int channel)
{
    const float semis = (formantParam != nullptr) ? formantParam->load() : 0.0f;
    formantRatio = std::pow(2.0f, semis / 12.0f);
    if(std::abs(semis) < 1.0e-4f) { formantGainValid[(size_t) channel] = false; return; }
    auto& gain = formantGain[(size_t) channel];
    auto& nextGain = nextFormantGain[(size_t) channel];
    const int overlap = (hopSize > 0) ? (fftSize / hopSize) : 4;
    int recomputeEvery = 1;
    if(overlap >= 16) recomputeEvery = 4;
    else if(overlap >= 8) recomputeEvery = 2;
    else recomputeEvery = 1;
    bool force = ! formantGainValid[(size_t) channel];
    if(std::abs(semis - formantLastSemis[(size_t) channel]) > 0.05f) force = true;
    int& hc = formantHopCounter[(size_t) channel];
    const bool recompute = force || (hc % recomputeEvery == 0);
    ++hc;
    if(recompute)
    {
        auto* fft = fftEngines[(size_t)(currentOrder - minOrder)].get();
        const int N = fftSize;
        const float eps = 1.0e-7f;
        const float invN = 1.0f / (float) N;
        float maxLog = -1.0e30f;
        for(int k = 0; k < numBins; ++k)
        {
            const float L = std::log(std::max(eps, pvMag[(size_t) k]));
            fmtRawLog[(size_t) k] = L;
            maxLog = std::max(maxLog, L);
        }
        const float floorLog = maxLog - formantDynRangeDb * (std::log(10.0f) / 20.0f);
        for(int k = 0; k < numBins; ++k)
            fmtLog[(size_t) k] = std::max(fmtRawLog[(size_t) k], floorLog);
        auto buildCepstrum = [&]()
        {
            for(int k = 0; k < numBins; ++k) fmtCepIn[(size_t) k] = { fmtLog[(size_t) k], 0.0f };
            for(int k = 1; k < N / 2; ++k) fmtCepIn[(size_t)(N - k)] = { fmtLog[(size_t) k], 0.0f };
            fft->perform(fmtCepIn.data(), fmtCepOut.data(), false);
            for(int n = 0; n < N; ++n) fmtCepOut[(size_t) n] *= invN;
        };
        buildCepstrum();
        const int nLo = juce::jlimit(2, N / 2, (int) std::floor((float) sampleRate / formantF0MaxHz));
        const int nHi = juce::jlimit(2, N / 2, (int) std::ceil((float) sampleRate / formantF0MinHz));
        int nPeak = 0; float peakVal = 0.0f; double meanAbs = 0.0;
        for(int n = nLo; n <= nHi; ++n)
        {
            const float v = fmtCepOut[(size_t) n].real();
            meanAbs += std::abs(v);
            if(v > peakVal) { peakVal = v; nPeak = n; }
        }
        meanAbs /= std::max(1, nHi - nLo + 1);
        const bool voiced = (nPeak > 0 && peakVal > 3.0f * (float) meanAbs);
        int Q = voiced ? (int) std::lround(formantLiftSafety * (float) nPeak)
                       : (int) std::lround((float) sampleRate / formantDefaultDetailHz);
        Q = juce::jlimit(12, N / 4, Q);
        const int Qpass = std::max(6, (int) std::round((float) Q * 0.7f));
        auto softLifter = [&]()
        {
            for(int n = 0; n < N; ++n)
            {
                const int q = std::min(n, N - n);
                float w;
                if(q <= Qpass) w = 1.0f;
                else if(q >= Q) w = 0.0f;
                else { const float tt = (float)(q - Qpass) / (float)(Q - Qpass);
                       w = 0.5f * (1.0f + std::cos(juce::MathConstants<float>::pi * tt)); }
                fmtCepOut[(size_t) n] *= w;
            }
        };
        for(int it = 0; it < formantEnvIters; ++it)
        {
            if(it > 0) buildCepstrum();
            softLifter();
            fft->perform(fmtCepOut.data(), fmtCepIn.data(), false);
            float correction = 0.0f;
            for(int k = 0; k < numBins; ++k)
            {
                const float v = fmtCepIn[(size_t) k].real();
                fmtEnvLog[(size_t) k] = v;
                const float oldLog = fmtLog[(size_t) k];
                const float newLog = std::max(oldLog, v);
                correction += newLog - oldLog;
                fmtLog[(size_t) k] = newLog;
            }
            correction /= (float) juce::jmax(1, numBins);
            if(it >= formantMinIters - 1 && correction < formantConvergeLog) break;
        }
        for(int k = 0; k < numBins; ++k)
            fmtEnv[(size_t) k] = std::exp(fmtEnvLog[(size_t) k]);
        const float invF = 1.0f / juce::jmax(1.0e-6f, formantRatio);
        for(int j = 0; j < numBins; ++j)
        {
            const float src = (float) j * invF;
            const int i0 = (int) std::floor(src);
            float envW;
            if(i0 >= numBins - 1) envW = fmtEnv[(size_t)(numBins - 1)];
            else if(i0 < 0) envW = fmtEnv[0];
            else { const float f = src - (float) i0;
                   envW = fmtEnv[(size_t) i0] + (fmtEnv[(size_t)(i0 + 1)] - fmtEnv[(size_t) i0]) * f; }
            nextGain[(size_t) j] = juce::jlimit(0.1f, 10.0f, envW / (fmtEnv[(size_t) j] + eps));
        }
        formantLastSemis[(size_t) channel] = semis;
        if(! formantGainValid[(size_t) channel])
        {
            std::copy(nextGain.begin(), nextGain.begin() + numBins, gain.begin());
            formantGainValid[(size_t) channel] = true;
        }
    }
    const float smoothing = (recomputeEvery > 1) ? (1.0f / (float) recomputeEvery) : 1.0f;
    for(int k = 0; k < numBins; ++k)
    {
        gain[(size_t) k] += smoothing * (nextGain[(size_t) k] - gain[(size_t) k]);
        pvMag[(size_t) k] *= gain[(size_t) k];
    }
}
float NewProjectAudioProcessor::salience(float F, float centsTolerance) const
{
    if(F <= 0.0f) return 0.0f;
    const float nyquist = (float) (sampleRate * 0.5);
    float sal = 0.0f;
    std::array<float, harmonicsMax + 1> bestByHarmonic {};
    const float invF = 1.0f / F;
    const float loR = std::exp2(-centsTolerance / 1200.0f);
    const float hiR = std::exp2( centsTolerance / 1200.0f);
    for(int i = 0; i < numPeaks; ++i)
    {
        const float amp = peakAmpWork[(size_t) i];
        if(amp <= 0.0f) continue;
        const float pf = peakFreq[(size_t) i];
        const float q = pf * invF;
        const int n = (int) (q + 0.5f);
        if(n < 1 || n > harmonicsMax) continue;
        const float target = (float) n * F;
        if(target >= nyquist) continue;
        const float ratio = pf / target;
        if(ratio > loR && ratio < hiR)
            bestByHarmonic[(size_t) n] = std::max(bestByHarmonic[(size_t) n], amp);
    }
    for(int n = 1; n <= harmonicsMax; ++n)
    {
        const float target = (float) n * F;
        if(target >= nyquist) break;
        const float g = (F + salAlpha) / (target + salBeta);
        sal += g * bestByHarmonic[(size_t) n];
    }
    return sal;
}
void NewProjectAudioProcessor::buildHarmonicMap(float centsTolerance)
{
    const float nyquist = (float) (sampleRate * 0.5);
    const float loR = std::exp2(-centsTolerance / 1200.0f);
    const float hiR = std::exp2( centsTolerance / 1200.0f);
    for(int c = 0; c < numPeaks; ++c)
    {
        const float F = peakFreq[(size_t) c];
        unsigned char* row = &harmonicMap[(size_t) c * (size_t) maxPeaks];
        if(F <= 0.0f) { std::fill(row, row + numPeaks, (unsigned char) 0); continue; }
        const float invF = 1.0f / F;
        for(int i = 0; i < numPeaks; ++i)
        {
            const float pf = peakFreq[(size_t) i];
            const float q = pf * invF;
            const int n = (int) (q + 0.5f);
            unsigned char hn = 0;
            if(n >= 1 && n <= harmonicsMax)
            {
                const float target = (float) n * F;
                if(target < nyquist)
                {
                    const float ratio = pf / target;
                    if(ratio > loR && ratio < hiR) hn = (unsigned char) n;
                }
            }
            row[i] = hn;
        }
    }
}
float NewProjectAudioProcessor::salienceIdx(int candPeak, int harmHorizon) const
{
    const float F = peakFreq[(size_t) candPeak];
    if(F <= 0.0f) return 0.0f;
    const float nyquist = (float) (sampleRate * 0.5);
    std::array<float, harmonicsMax + 1> bestByHarmonic {};
    const unsigned char* row = &harmonicMap[(size_t) candPeak * (size_t) maxPeaks];
    for(int i = 0; i < numPeaks; ++i)
    {
        const unsigned char n = row[i];
        if(n == 0) continue;
        const float amp = peakAmpWork[(size_t) i];
        if(amp <= 0.0f) continue;
        if(amp > bestByHarmonic[(size_t) n]) bestByHarmonic[(size_t) n] = amp;
    }
    float sal = 0.0f;
    const int nMax = juce::jmin(harmHorizon, harmonicsMax);
    for(int n = 1; n <= nMax; ++n)
    {
        const float target = (float) n * F;
        if(target >= nyquist) break;
        const float g = (F + salAlpha) / (target + salBeta);
        sal += g * bestByHarmonic[(size_t) n];
    }
    return sal;
}
float NewProjectAudioProcessor::nearestTargetMidi(float baseHz) const
{
    if(baseHz <= 0.0f) return -1000.0f;
    const int mode = (targetModeParam != nullptr) ? juce::roundToInt(targetModeParam->load()) : 0;
    const float midi = 69.0f + 12.0f * std::log2 (baseHz / 440.0f);
    if(mode == 2)
    {
        if(scNumTargets <= 0) return -1000.0f;
        float best = -1000.0f, bestDist = 1.0e9f;
        for(int i = 0; i < scNumTargets; ++i)
        {
            const float tHz = scTargetHz[(size_t) i];
            if(tHz <= 0.0f) continue;
            const float tMidi = 69.0f + 12.0f * std::log2 (tHz / 440.0f);
            const float d = std::abs(tMidi - midi);
            if(d < bestDist) { bestDist = d; best = tMidi; }
        }
        return best;
    }
    const bool midiOn = (mode == 1);
    const bool fullMidiOn = midiOn && (fullMidiParam != nullptr && fullMidiParam->load() >= 0.5f);
    if(fullMidiOn)
    {
        int bestNote = -1; float bestDist = 1.0e9f;
        for(int n = 0; n < 128; ++n)
        {
            if(midiHeldNote[n] <= 0) continue;
            const float d = std::abs((float) n - midi);
            if(d < bestDist) { bestDist = d; bestNote = n; }
        }
        return(bestNote < 0) ? -1000.0f : (float) bestNote;
    }
    bool any = false;
    for(int i = 0; i < 12; ++i) any = any || scaleOn[i];
    if(! any) return -1000.0f;
    const int centre = (int) std::lround(midi);
    int bestNote = centre; float bestDist = 1.0e9f; bool found = false;
    for(int cand = centre - 12; cand <= centre + 12; ++cand)
    {
        const int pc = ((cand % 12) + 12) % 12;
        if(! scaleOn[pc]) continue;
        const float d = std::abs((float) cand - midi);
        if(d < bestDist) { bestDist = d; bestNote = cand; found = true; }
    }
    return found ? (float) bestNote : -1000.0f;
}
float NewProjectAudioProcessor::snapToTargetMidi(float baseHz, float targetMidi, float amount) const
{
    if(amount <= 0.0f || baseHz <= 0.0f || targetMidi < -900.0f) return baseHz;
    const float midi = 69.0f + 12.0f * std::log2 (baseHz / 440.0f);
    return baseHz * std::pow(2.0f, (targetMidi - midi) / 12.0f * amount);
}
float NewProjectAudioProcessor::snapBaseToScale(float baseHz, float amount, float fineCents) const
{
    if(amount <= 0.0f || baseHz <= 0.0f) return baseHz;
    float tgt = nearestTargetMidi(baseHz);
    if(tgt < -900.0f) return baseHz;
    const int mode = (targetModeParam != nullptr) ? juce::roundToInt(targetModeParam->load()) : 0;
    if(mode != 2) tgt += fineCents / 100.0f;
    return snapToTargetMidi(baseHz, tgt, amount);
}
float NewProjectAudioProcessor::membershipSigma(int harmonic, float freqHz) const
{
    const int n = juce::jmax(1, harmonic);
    const float gapCents = 1200.0f * std::log2 ((float)(n + 1) / (float) n);
    const float f = juce::jmax(1.0f, freqHz);
    const float resFloor = 1200.0f * std::log2 (1.0f + binWidth / f);
    const float lo = juce::jmax(5.0f, resFloor);
    const float hi = juce::jmax(lo, membershipSigmaCents);
    return juce::jlimit(lo, hi, membershipGapFrac * gapCents);
}
void NewProjectAudioProcessor::updateBaseTracker(int channel, float frameTonal)
{
    auto& T = tracked[(size_t) channel];
    const bool hystOn = (hysteresisParam != nullptr) && (hysteresisParam->load() >= 0.5f);
    if(! hystOn)
    {
        for(auto& t : T) t = TrackedBase{};
        std::fill(baseTargetMidi.begin(), baseTargetMidi.end(), -1000.0f);
        return;
    }
    const float claimMs = 20.0f;
    const float releaseMs = 50.0f;
    const float slewCps = 15.0f;
    const float stab = 1.0f;
    const float hopSec = (float) hopSize / (float) juce::jmax(1.0, sampleRate);
    const float claimRise = juce::jlimit(0.01f, 1.0f, hopSec / (claimMs * 0.001f));
    const float relDecay = juce::jlimit(0.01f, 1.0f, hopSec / juce::jmax(1.0e-4f, releaseMs * 0.001f));
    const float slewCents = juce::jmax(1.0f, slewCps * hopSec);
    const float matchTolCents = 60.0f;
    const float claimHi = 0.6f;
    const float hyst = 0.1f;
    const float claimLo = 0.45f;
    const float heldValidTolMidi = 0.5f;
    const float salHoldDecay = 0.80f;
    const int outLimit = juce::jmin((int) baseFreq.size(), (int) maxBases);
    std::array<bool, maxTracked> trkMatched {};
    std::array<bool, maxBases> candMatched {};
    std::array<float, maxBases> candFreq {};
    std::array<float, maxBases> candDispConf {};
    const int nCand = juce::jmin(numBases, (int) maxBases);
    const float candRef = (nCand > 0) ? baseSal[0] : 0.0f;
    for(int c = 0; c < nCand; ++c)
    {
        candFreq[(size_t) c] = baseFreq[(size_t) c];
        const float rel = (candRef > 0.0f) ? juce::jlimit(0.0f, 1.0f, baseSal[(size_t) c] / candRef) : 0.0f;
        candDispConf[(size_t) c] = rel * frameTonal;
    }
    for(int c = 0; c < nCand; ++c)
    {
        const float fc = candFreq[(size_t) c];
        if(fc <= 0.0f) { candMatched[(size_t) c] = true; continue; }
        int best = -1; float bestDev = matchTolCents;
        for(int k = 0; k < maxTracked; ++k)
        {
            if(! T[(size_t) k].active || trkMatched[(size_t) k]) continue;
            const float dev = std::abs(1200.0f * std::log2 (fc / juce::jmax(1.0e-6f, T[(size_t) k].freq)));
            if(dev < bestDev) { bestDev = dev; best = k; }
        }
        if(best >= 0)
        {
            auto& t = T[(size_t) best];
            trkMatched[(size_t) best] = true; candMatched[(size_t) c] = true;
            t.missing = 0; ++t.age;
            t.sal = candDispConf[(size_t) c];
            const float devCents = 1200.0f * std::log2 (fc / juce::jmax(1.0e-6f, t.freq));
            const float step = juce::jlimit(-slewCents, slewCents, devCents);
            t.freq *= std::pow(2.0f, step / 1200.0f);
            const float wander = juce::jlimit(0.0f, 1.0f, std::abs(devCents) / 100.0f);
            t.conf = juce::jlimit(0.0f, 1.0f, t.conf + claimRise * (1.0f - stab * wander));
            if(t.conf >= claimHi) t.claimed = true;
            const float tgtNow = nearestTargetMidi(t.freq);
            if(tgtNow < -900.0f) t.heldTargetMidi = -1000.0f;
            else if(t.heldTargetMidi < -900.0f) t.heldTargetMidi = tgtNow;
            else
            {
                const float heldHz = 440.0f * std::pow(2.0f, (t.heldTargetMidi - 69.0f) / 12.0f);
                const float heldNearest = nearestTargetMidi(heldHz);
                const bool heldGone = (heldNearest < -900.0f)
                                       || (std::abs(heldNearest - t.heldTargetMidi) > heldValidTolMidi);
                if(heldGone)
                {
                    t.heldTargetMidi = tgtNow;
                }
                else
                {
                    t.heldTargetMidi = heldNearest;
                    if(std::abs(tgtNow - t.heldTargetMidi) > 0.01f)
                    {
                        const float baseMidi = 69.0f + 12.0f * std::log2 (juce::jmax(1.0e-6f, t.freq) / 440.0f);
                        const float span = tgtNow - t.heldTargetMidi;
                        if(std::abs(span) > 1.0e-4f)
                        {
                            const float x = (baseMidi - t.heldTargetMidi) / span;
                            if(x > 0.5f + hyst) t.heldTargetMidi = tgtNow;
                        }
                        else t.heldTargetMidi = tgtNow;
                    }
                }
            }
        }
    }
    for(int c = 0; c < nCand; ++c)
    {
        if(candMatched[(size_t) c]) continue;
        const float fc = candFreq[(size_t) c];
        if(fc <= 0.0f) continue;
        int slot = -1;
        for(int k = 0; k < maxTracked; ++k) if(! T[(size_t) k].active) { slot = k; break; }
        if(slot < 0) continue;
        auto& t = T[(size_t) slot]; t = TrackedBase{};
        t.active = true; t.freq = fc; t.conf = claimRise; t.age = 1;
        t.sal = candDispConf[(size_t) c];
        trkMatched[(size_t) slot] = true;
    }
    for(int k = 0; k < maxTracked; ++k)
    {
        auto& t = T[(size_t) k];
        if(! t.active || trkMatched[(size_t) k]) continue;
        ++t.missing; t.conf -= relDecay;
        t.sal *= salHoldDecay;
        if(t.conf <= claimLo) t.claimed = false;
        if(t.conf <= 0.0f) t = TrackedBase{};
    }
    if(channel == 1)
    {
        const float ambigBand = juce::jlimit(0.02f, 0.2f, hyst + 0.02f);
        auto& T0 = tracked[0];
        for(int k = 0; k < maxTracked; ++k)
        {
            auto& t = T[(size_t) k];
            if(! t.active || ! t.claimed || t.heldTargetMidi < -900.0f) continue;
            const float tgtNow = nearestTargetMidi(t.freq);
            bool ambiguous = false;
            if(tgtNow > -900.0f && std::abs(tgtNow - t.heldTargetMidi) > 0.01f)
            {
                const float baseMidi = 69.0f + 12.0f * std::log2 (juce::jmax(1.0e-6f, t.freq) / 440.0f);
                const float span = tgtNow - t.heldTargetMidi;
                if(std::abs(span) > 1.0e-4f)
                {
                    const float x = (baseMidi - t.heldTargetMidi) / span;
                    ambiguous = (std::abs(x - 0.5f) < ambigBand);
                }
            }
            if(! ambiguous) continue;
            for(int k0 = 0; k0 < maxTracked; ++k0)
            {
                const auto& t0 = T0[(size_t) k0];
                if(! t0.active || ! t0.claimed || t0.heldTargetMidi < -900.0f) continue;
                if(std::abs(1200.0f * std::log2 (t.freq / juce::jmax(1.0e-6f, t0.freq))) < 100.0f)
                { t.heldTargetMidi = t0.heldTargetMidi; break; }
            }
        }
    }
    std::array<int, maxTracked> order {}; int nOrder = 0;
    for(int k = 0; k < maxTracked; ++k)
        if(T[(size_t) k].active && T[(size_t) k].claimed) order[(size_t) nOrder++] = k;
    for(int a = 1; a < nOrder; ++a)
    {
        const int key = order[(size_t) a]; int b = a - 1;
        while(b >= 0 && T[(size_t) order[(size_t) b]].conf < T[(size_t) key].conf)
        { order[(size_t)(b + 1)] = order[(size_t) b]; --b; }
        order[(size_t)(b + 1)] = key;
    }
    int out = 0;
    for(int oi = 0; oi < nOrder && out < outLimit; ++oi)
    {
        const int k = order[(size_t) oi];
        baseFreq[(size_t) out] = T[(size_t) k].freq;
        baseSal [(size_t) out] = juce::jmax(0.0f, T[(size_t) k].sal);
        baseTargetMidi[(size_t) out] = T[(size_t) k].heldTargetMidi;
        ++out;
    }
    for(int c = 0; c < nCand && out < outLimit; ++c)
    {
        const float fc = candFreq[(size_t) c];
        if(fc <= 0.0f) continue;
        bool near = false;
        for(int oi = 0; oi < out; ++oi)
            if(std::abs(1200.0f * std::log2 (fc / juce::jmax(1.0e-6f, baseFreq[(size_t) oi]))) < matchTolCents)
            { near = true; break; }
        if(! near) { baseFreq[(size_t) out] = fc; baseSal[(size_t) out] = candDispConf[(size_t) c];
                      baseTargetMidi[(size_t) out] = -1000.0f; ++out; }
    }
    numBases = out;
}
void NewProjectAudioProcessor::detect(int channel)
{
    const float t = juce::jlimit(0.0f, 1.0f, emphasisParam->load());
    const float s = juce::jlimit(0.0f, 1.0f, attractionParam->load());
    const float tolWarp = t * t;
    const float centsTolerance = centsTol + tolWarp * (centsTolMax - centsTol);
    const float tolLoR = std::exp2(-centsTolerance / 1200.0f);
    const float tolHiR = std::exp2( centsTolerance / 1200.0f);
    const float density = juce::jlimit(0.0f, 1.0f, densityParam->load());
    const float thr = densityToSalienceThreshold(density);
    const float fineCents = juce::jlimit(-100.0f, 100.0f, fineTuneParam->load());
    pitchRatio = std::pow(2.0f, pitchParam->load() / 12.0f);
    const float nyq = (float) (sampleRate * 0.5);
    const float center = centerParam->load();
    const float spread = spreadParam->load();
    const float fMin = juce::jlimit(20.0f, nyq, center * std::pow(2.0f, -spread));
    const float fMax = juce::jlimit(20.0f, nyq, center * std::pow(2.0f, spread));
    for(int k = 0; k < numBins; ++k)
    {
        binDestFreq[(size_t) k] = pvFreq[(size_t) k] * pitchRatio;
        binTargetAffinity[(size_t) k] = 0.0f;
    }
    const bool remap = (t > 0.0f || s > 0.0f);
    const bool moreBasesOn = (moreBasesParam != nullptr && moreBasesParam->load() >= 0.5f);
    const int peakLimit = moreBasesOn ? extendedMaxPeaks : defaultMaxPeaks;
    const int baseLimit = moreBasesOn ? extendedMaxBases : defaultMaxBases;
    const float salienceGateScale = moreBasesOn ? 0.85f : 1.0f;
    const float gatedThr = juce::jmax(0.0005f, thr * salienceGateScale);
    numBases = 0;
    subNumBases = 0;
    const int mode = (targetModeParam != nullptr) ? juce::roundToInt(targetModeParam->load()) : 0;
    const float heldFineSemis = (mode != 2) ? fineCents / 100.0f : 0.0f;
    const bool midiOn = (mode == 1);
    const int midiMask = midiScaleMask.load();
    for(int i = 0; i < 12; ++i)
        scaleOn[i] = midiOn ? ((midiMask & (1 << i)) != 0)
                            : (noteParam[i]->load() >= 0.5f);
    float maxMag = 0.0f;
    for(int k = 0; k < numBins; ++k) maxMag = std::max(maxMag, pvMag[(size_t) k]);
    if(maxMag < silenceMagFloor) { numPeaks = 0; return; }
    const float floorMag = maxMag * peakFloorRel;
    const int inBandLimit = peakLimit;
    const int subLimit = juce::jmin(peakLimit / 2, maxPeaks / 4);
    const int superLimit = juce::jmin(peakLimit / 2, maxPeaks / 4);
    int inBandCount = 0, subCount = 0, superCount = 0;
    numPeaks = 0;
    const float sepCentsPick = 50.0f;
    for(int k = 2; k + 2 < numBins && numPeaks < maxPeaks; ++k)
    {
        const float peakHz = pvFreq[(size_t) k] * pitchRatio;
        if(peakHz < 20.0f)
            continue;
        if(peakHz < fMin) { if(subCount >= subLimit) continue; }
        else if(peakHz > fMax) { if(superCount >= superLimit) continue; }
        else { if(inBandCount >= inBandLimit) continue; }
        const float m = pvMag[(size_t) k];
        if(! (m > floorMag && m > pvMag[(size_t) (k - 1)] && m >= pvMag[(size_t) (k + 1)]))
            continue;
        if(numPeaks > 0)
        {
            const float fPrev = peakFreq[(size_t)(numPeaks - 1)];
            const float dc = std::abs(1200.0f * std::log2 (peakHz / juce::jmax(1.0e-6f, fPrev)));
            if(dc < sepCentsPick)
            {
                if(m > peakAmp[(size_t)(numPeaks - 1)])
                { peakBin[(size_t)(numPeaks-1)] = k; peakFreq[(size_t)(numPeaks-1)] = peakHz; peakAmp[(size_t)(numPeaks-1)] = m; }
                continue;
            }
        }
        peakBin[(size_t) numPeaks] = k;
        peakFreq[(size_t) numPeaks] = peakHz;
        peakAmp[(size_t) numPeaks] = m;
        peakBaseIdx[(size_t) numPeaks] = -1;
        peakHarmonic[(size_t) numPeaks] = 0;
        peakRatio[(size_t) numPeaks] = 1.0f;
        ++numPeaks;
        if(peakHz < fMin) ++subCount;
        else if(peakHz > fMax) ++superCount;
        else ++inBandCount;
    }
    if(numPeaks == 0) return;
    buildHarmonicMap(centsTolerance);
    float totalPeak = 0.0f;
    for(int i = 0; i < numPeaks; ++i)
    {
        peakAmpWork[(size_t) i] = peakAmp[(size_t) i];
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
            if(peakAmpWork[(size_t) c] <= 0.0f) continue;
            if(Fc < fMin || Fc > fMax) continue;
            const float sal = salienceIdx(c);
            float score = sal;
            if(numBases == 0 && prevPrimary[(size_t) channel] > 0.0f
                && std::abs(1200.0f * std::log2 (Fc / prevPrimary[(size_t) channel])) < centsTolerance)
                score *= (1.0f + continuityBias);
            if(score > bestScore) { bestScore = score; bestSal = sal; bestF = Fc; bestIdx = c; }
        }
        if(bestIdx < 0) break;
        bool improved = true;
        while(improved)
        {
            improved = false;
            const float halfF = bestF * 0.5f;
            if(halfF >= fMin)
                for(int c = 0; c < numPeaks; ++c)
                {
                    if(peakAmpWork[(size_t) c] <= 0.0f) continue;
                    const float fc = peakFreq[(size_t) c];
                    if(std::abs(1200.0f * std::log2 (fc / halfF)) < centsTolerance)
                    {
                        const float subSal = salienceIdx(c);
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
        const float depth = juce::jlimit(0.0f, 1.0f,
                                          (firstSal > 0.0f) ? (bestSal / firstSal) : 1.0f);
        const float invBestF = 1.0f / bestF;
        for(int i = 0; i < numPeaks; ++i)
        {
            if(peakAmpWork[(size_t) i] <= 0.0f) continue;
            const float pf = peakFreq[(size_t) i];
            const float q = pf * invBestF;
            const int n = (int) (q + 0.5f);
            if(n < 1 || n > harmonicsTagMax) continue;
            const float ratio = q / (float) n;
            if(ratio < 0.94f || ratio > 1.06f) continue;
            const float cents = 1200.0f * std::log2 (ratio);
            const float sig = membershipSigma(n, pf);
            const float z = cents / sig;
            const float membership = std::exp(-z * z);
            peakAmpWork[(size_t) i] *= (1.0f - depth * membership);
        }
    }
    const float tonalNow = juce::jlimit(0.0f, 1.0f,
                                        firstSal / (tonalRefScale * (totalPeak + 1.0e-12f)));
    updateBaseTracker(channel, tonalNow);
    for(int b = 0; b < numBases; ++b)
        baseLog2Hz[(size_t) b] = (baseFreq[(size_t) b] > 0.0f)
                                    ? std::log2 (baseFreq[(size_t) b]) : -1.0e30f;
    if(numBases > 0)
    {
        prevPrimary[(size_t) channel] = baseFreq[0];
        const bool hystOn = (hysteresisParam != nullptr) && (hysteresisParam->load() >= 0.5f);
        if(hystOn)
        {
            for(int b = 0; b < numBases; ++b)
            {
                const float heldTgt = baseTargetMidi[(size_t) b];
                baseDisplayHz[(size_t) b] = (heldTgt > -900.0f)
                    ? snapToTargetMidi(baseFreq[(size_t) b], heldTgt + heldFineSemis, s)
                    : snapBaseToScale(baseFreq[(size_t) b], s, fineCents);
                baseConf[(size_t) b] = juce::jlimit(0.0f, 1.0f, baseSal[(size_t) b]);
            }
        }
        else
        {
            const float tonal = juce::jlimit(0.0f, 1.0f,
                                              firstSal / (tonalRefScale * (totalPeak + 1.0e-12f)));
            for(int b = 0; b < numBases; ++b)
            {
                baseDisplayHz[(size_t) b] = snapBaseToScale(baseFreq[(size_t) b], s, fineCents);
                const float rel = (firstSal > 0.0f)
                                    ? juce::jlimit(0.0f, 1.0f, baseSal[(size_t) b] / firstSal) : 0.0f;
                baseConf[(size_t) b] = rel * tonal;
            }
        }
    }
    {
        for(int i = 0; i < numPeaks; ++i)
            peakAmpWork[(size_t) i] = (peakFreq[(size_t) i] < fMin) ? peakAmp[(size_t) i] : 0.0f;
        subNumBases = 0;
        float subFirstSal = 0.0f;
        while(subNumBases < baseLimit)
        {
            float bestScore = 0.0f, bestSal = 0.0f, bestF = 0.0f;
            int bestIdx = -1;
            for(int c = 0; c < numPeaks; ++c)
            {
                const float Fc = peakFreq[(size_t) c];
                if(peakAmpWork[(size_t) c] <= 0.0f) continue;
                if(Fc >= fMin) continue;
                const float sal = salienceIdx(c, nonMidHarmonics);
                float score = sal;
                if(subNumBases == 0 && subPrevPrimary[(size_t) channel] > 0.0f
                    && std::abs(1200.0f * std::log2 (Fc / subPrevPrimary[(size_t) channel])) < centsTolerance)
                    score *= (1.0f + continuityBias);
                if(score > bestScore) { bestScore = score; bestSal = sal; bestF = Fc; bestIdx = c; }
            }
            if(bestIdx < 0) break;
            if(subNumBases == 0) subFirstSal = bestSal;
            else if(bestSal < gatedThr * subFirstSal) break;
            subBaseSal [(size_t) subNumBases] = bestSal;
            subBaseFreq[(size_t) subNumBases] = bestF;
            ++subNumBases;
            for(int i = 0; i < numPeaks; ++i)
            {
                if(peakAmpWork[(size_t) i] <= 0.0f) continue;
                const float q = peakFreq[(size_t) i] / bestF;
                const int n = (int) (q + 0.5f);
                if(n < 1 || n > harmonicsTagMax) continue;
                const float ratio = q / (float) n;
                if(ratio > tolLoR && ratio < tolHiR) peakAmpWork[(size_t) i] = 0.0f;
            }
        }
        if(subNumBases > 0) subPrevPrimary[(size_t) channel] = subBaseFreq[0];
        for(int i = 0; i < numPeaks; ++i)
        {
            const float fk = peakFreq[(size_t) i];
            if(fk >= fMin || subNumBases == 0) continue;
            int bBase = 0, bN = 1; float bDev = 1.0e9f;
            for(int b = 0; b < subNumBases; ++b)
            {
                const float Fb = subBaseFreq[(size_t) b];
                const int n = juce::jmax(1, (int) std::lround(fk / Fb));
                const float dev = std::abs(1200.0f * std::log2 (fk / ((float) n * Fb)));
                if(dev < bDev) { bDev = dev; bBase = b; bN = n; }
            }
            if(bDev < 50.0f)
            {
                peakBaseIdx[(size_t) i] = subBaseBase + bBase;
                peakHarmonic[(size_t) i] = bN;
            }
        }
    }
    {
        for(int i = 0; i < numPeaks; ++i)
        {
            const float fk = peakFreq[(size_t) i];
            peakAmpWork[(size_t) i] = 0.0f;
            if(fk <= fMax) continue;
            const float log2fk = std::log2 (fk);
            int mBase = -1, mN = 1; float mDev = 1.0e9f;
            for(int b = 0; b < numBases; ++b)
            {
                const float Fb = baseFreq[(size_t) b];
                if(Fb <= 0.0f) continue;
                const int n = juce::jmax(1, (int) std::lround(fk / Fb));
                const float ln = (n <= harmonicsTagMax) ? log2IntTable[(size_t) n] : std::log2 ((float) n);
                const float dev = 1200.0f * std::abs(log2fk - ln - baseLog2Hz[(size_t) b]);
                if(dev < mDev) { mDev = dev; mBase = b; mN = n; }
            }
            if(mBase >= 0 && mDev < membershipSigma(mN, fk))
            {
                peakBaseIdx[(size_t) i] = mBase;
                peakHarmonic[(size_t) i] = mN;
            }
            else
            {
                peakAmpWork[(size_t) i] = peakAmp[(size_t) i];
            }
        }
        int nHiWork = 0;
        for(int i = 0; i < numPeaks; ++i)
            if(peakAmpWork[(size_t) i] > 0.0f && peakFreq[(size_t) i] > fMax) ++nHiWork;
        hiNumBases = 0;
        float hiFirstSal = 0.0f;
        while(nHiWork > 0 && hiNumBases < baseLimit)
        {
            float bestScore = 0.0f, bestSal = 0.0f, bestF = 0.0f; int bestIdx = -1;
            for(int c = 0; c < numPeaks; ++c)
            {
                const float Fc = peakFreq[(size_t) c];
                if(peakAmpWork[(size_t) c] <= 0.0f) continue;
                if(Fc <= fMax) continue;
                const float sal = salienceIdx(c, nonMidHarmonics);
                float score = sal;
                if(hiNumBases == 0 && hiPrevPrimary[(size_t) channel] > 0.0f
                    && std::abs(1200.0f * std::log2 (Fc / hiPrevPrimary[(size_t) channel])) < centsTolerance)
                    score *= (1.0f + continuityBias);
                if(score > bestScore) { bestScore = score; bestSal = sal; bestF = Fc; bestIdx = c; }
            }
            if(bestIdx < 0) break;
            if(hiNumBases == 0) hiFirstSal = bestSal;
            else if(bestSal < gatedThr * hiFirstSal) break;
            hiBaseSal [(size_t) hiNumBases] = bestSal;
            hiBaseFreq[(size_t) hiNumBases] = bestF;
            ++hiNumBases;
            for(int i = 0; i < numPeaks; ++i)
            {
                if(peakAmpWork[(size_t) i] <= 0.0f) continue;
                const float q = peakFreq[(size_t) i] / bestF;
                const int n = (int) (q + 0.5f);
                if(n < 1 || n > harmonicsTagMax) continue;
                const float ratio = q / (float) n;
                if(ratio > tolLoR && ratio < tolHiR) peakAmpWork[(size_t) i] = 0.0f;
            }
        }
        if(hiNumBases > 0) hiPrevPrimary[(size_t) channel] = hiBaseFreq[0];
        for(int i = 0; i < numPeaks; ++i)
        {
            const float fk = peakFreq[(size_t) i];
            if(fk <= fMax || hiNumBases == 0) continue;
            if(peakBaseIdx[(size_t) i] >= 0 && peakBaseIdx[(size_t) i] < subBaseBase) continue;
            int bBase = 0, bN = 1; float bDev = 1.0e9f;
            for(int b = 0; b < hiNumBases; ++b)
            {
                const float Fb = hiBaseFreq[(size_t) b];
                const int n = juce::jmax(1, (int) std::lround(fk / Fb));
                const float dev = std::abs(1200.0f * std::log2 (fk / ((float) n * Fb)));
                if(dev < bDev) { bDev = dev; bBase = b; bN = n; }
            }
            if(bDev < 50.0f)
            {
                peakBaseIdx[(size_t) i] = hiBaseBase + bBase;
                peakHarmonic[(size_t) i] = bN;
            }
        }
    }
    if(! remap)
        return;
    if(numBases == 0)
        return;
    const float sigmaWarp = t * t;
    const float sigmaCents = proximitySigmaCents + sigmaWarp * (420.0f - proximitySigmaCents);
    const float tHi = juce::jlimit(0.0f, 1.0f, (t - 0.85f) / 0.15f);
    const float tHiSmooth = tHi * tHi * (3.0f - 2.0f * tHi);
    const float minPull = 0.65f * tHiSmooth;
    for(int i = 0; i < numPeaks; ++i)
    {
        const float fk = peakFreq[(size_t) i];
        if(fk < fMin)
            continue;
        const float log2fk = std::log2 (fk);
        int bestBase = 0, bestN = 1;
        float bestDev = 1.0e9f;
        for(int b = 0; b < numBases; ++b)
        {
            const float Fb = baseFreq[(size_t) b];
            if(Fb <= 0.0f) continue;
            const int n = juce::jmax(1, (int) std::lround(fk / Fb));
            const float ln = (n <= harmonicsTagMax) ? log2IntTable[(size_t) n] : std::log2 ((float) n);
            const float dev = 1200.0f * std::abs(log2fk - ln - baseLog2Hz[(size_t) b]);
            if(dev < bestDev) { bestDev = dev; bestBase = b; bestN = n; }
        }
        const bool isSuper = (fk > fMax);
        const float Fb = baseFreq[(size_t) bestBase];
        const int n = bestN;
        const float heldTgt = baseTargetMidi[(size_t) bestBase];
        const float FbS = (heldTgt > -900.0f) ? snapToTargetMidi(Fb, heldTgt + heldFineSemis, s)
                                              : snapBaseToScale(Fb, s, fineCents);
        const float fk0 = fk;
        const float fkFollow = fk * (FbS / Fb);
        const float ideal = (float) n * FbS;
        const float sigmaScale = sigmaCents / juce::jmax(1.0f, proximitySigmaCents);
        const float gapCentsN = (n <= harmonicsTagMax) ? gapCentsTable[(size_t) n] : 1200.0f * std::log2 ((float)(n + 1) / (float) n);
        const float sigN = shiftGapFrac * gapCentsN * sigmaScale;
        const float z = bestDev / juce::jmax(1.0e-3f, sigN);
        const float wProximity = std::exp(-z * z);
        const float sigPull = pullGapFrac * gapCentsN;
        const float zPull = bestDev / juce::jmax(1.0e-3f, sigPull);
        const float pullFloor = minPull * std::exp(-(zPull * zPull));
        const float w = isSuper ? wProximity : juce::jmax(wProximity, pullFloor);
        if(w < 1.0e-3f)
        {
            peakBaseIdx[(size_t) i] = bestBase;
            peakHarmonic[(size_t) i] = n;
            continue;
        }
        const float corrected = fkFollow * std::pow(ideal / fkFollow, t);
        const float baseDevCents = std::abs(1200.0f * std::log2 (FbS / Fb));
        const float bdc = baseDevCents / 90.0f;
        const float baseAffinity = std::exp(-(bdc * bdc));
        const float dest = fk0 * std::pow(corrected / fk0, w);
        peakRatio[(size_t) i] = dest / fk;
        peakTargetAffinity[(size_t) i] = baseAffinity;
        peakBaseIdx[(size_t) i] = bestBase;
        peakHarmonic[(size_t) i] = n;
    }
    const float sigmaScaleB = sigmaCents / juce::jmax(1.0f, proximitySigmaCents);
    for(int b = 0; b < numBases; ++b)
    {
        const float Fbb = baseFreq[(size_t) b];
        if(Fbb <= 0.0f) { baseInvFreq[(size_t) b] = 0.0f; continue; }
        const float heldTgtB = baseTargetMidi[(size_t) b];
        const float FbSb = (heldTgtB > -900.0f) ? snapToTargetMidi(Fbb, heldTgtB + heldFineSemis, s)
                                                : snapBaseToScale(Fbb, s, fineCents);
        baseInvFreq [(size_t) b] = 1.0f / Fbb;
        baseSnapRatio[(size_t) b] = FbSb / Fbb;
        baseSnapHz [(size_t) b] = FbSb;
        baseSnapLog2Hz [(size_t) b] = std::log2 (FbSb);
        baseSnapLog2Ratio[(size_t) b] = baseSnapLog2Hz[(size_t) b] - baseLog2Hz[(size_t) b];
        const float baseDevCb = std::abs(1200.0f * baseSnapLog2Ratio[(size_t) b]);
        const float bd = baseDevCb / 90.0f;
        baseAffinityV[(size_t) b] = std::exp(-(bd * bd));
    }
    int lastBase = -1;
    for(int k = 0; k < numBins; ++k)
    {
        const float fkb = pvFreq[(size_t) k] * pitchRatio;
        if(fkb < fMin) continue;
        int bBase = -1, bN = 1; float bestRatioDist = 1.0e9f;
        if(lastBase >= 0)
        {
            const float inv = baseInvFreq[(size_t) lastBase];
            if(inv > 0.0f)
            {
                const float q = fkb * inv;
                int nn = (int) (q + 0.5f); if(nn < 1) nn = 1;
                const float invNN = (nn <= harmonicsTagMax) ? invIntTable[(size_t) nn] : 1.0f / (float) nn;
                const float d = std::abs(q * invNN - 1.0f);
                if(d < 0.0015f) { bBase = lastBase; bN = nn; bestRatioDist = d; }
            }
        }
        if(bBase < 0)
            for(int b = 0; b < numBases; ++b)
            {
                const float inv = baseInvFreq[(size_t) b];
                if(inv <= 0.0f) continue;
                const float q = fkb * inv;
                int nn = (int) (q + 0.5f); if(nn < 1) nn = 1;
                const float invNN = (nn <= harmonicsTagMax) ? invIntTable[(size_t) nn] : 1.0f / (float) nn;
                const float d = std::abs(q * invNN - 1.0f);
                if(d < bestRatioDist) { bestRatioDist = d; bBase = b; bN = nn; }
                if(d < 0.0015f) break;
            }
        if(bBase < 0) continue;
        lastBase = bBase;
        const float Lfkb = std::log2 (fkb);
        const float LbN = (bN <= harmonicsTagMax) ? log2IntTable[(size_t) bN] : std::log2 ((float) bN);
        const float bDev = std::abs(1200.0f * (Lfkb - baseLog2Hz[(size_t) bBase] - LbN));
        const float gapCentsB = (bN <= harmonicsTagMax) ? gapCentsTable[(size_t) bN]
                                                        : 1200.0f * std::log2 ((float)(bN + 1) / (float) bN);
        const float sigNb = shiftGapFrac * gapCentsB * sigmaScaleB;
        const float zb = bDev / juce::jmax(1.0e-3f, sigNb);
        const float wProxb = std::exp(-(zb * zb));
        const float sigPullb = pullGapFrac * gapCentsB;
        const float zPullb = bDev / juce::jmax(1.0e-3f, sigPullb);
        const float pullFloorb = minPull * std::exp(-(zPullb * zPullb));
        const bool binSuper = (fkb > fMax);
        const float wb = binSuper ? wProxb : juce::jmax(wProxb, pullFloorb);
        if(wb < 1.0e-3f) continue;
        const float LfkFollow = Lfkb + baseSnapLog2Ratio[(size_t) bBase];
        const float Lideal = LbN + baseSnapLog2Hz[(size_t) bBase];
        const float Lcorrected = LfkFollow + t * (Lideal - LfkFollow);
        const float Ldest = Lfkb + wb * (Lcorrected - Lfkb);
        binDestFreq[(size_t) k] = std::exp2 (Ldest);
        binTargetAffinity[(size_t) k] = baseAffinityV[(size_t) bBase];
    }
  }
void NewProjectAudioProcessor::updatePartials(int channel)
{
    auto& parts = partials[(size_t) channel];
    const float hopSec = (float) hopSize / (float) sampleRate;
    const float twoPi = juce::MathConstants<float>::twoPi;
    const int K = numPeaks;
    for(auto& p : parts) p.matched = false;
    for(int i = 0; i < K; ++i) peakMatchedPartial[(size_t) i] = -1;
    matchCands.clear();
    const float pmLoR = std::exp2(-partialMatchTolCents / 1200.0f);
    const float pmHiR = std::exp2( partialMatchTolCents / 1200.0f);
    for(size_t pi = 0; pi < parts.size(); ++pi)
    {
        const float pf = parts[pi].freqNat;
        const float invpf = 1.0f / juce::jmax(1.0e-6f, pf);
        for(int i = 0; i < K; ++i)
        {
            const float fNat = peakFreq[(size_t) i];
            if(fNat <= 0.0f) continue;
            const float ratio = fNat * invpf;
            const float dHz = std::abs(fNat - pf);
            const bool nearCents = (ratio > pmLoR && ratio < pmHiR);
            if(! (nearCents || dHz <= partialMatchTolHz)) continue;
            const float dk = ratio >= 1.0f ? ratio : 1.0f / ratio;
            matchCands.push_back({ dk, (int) pi, i });
        }
    }
    std::sort(matchCands.begin(), matchCands.end(),
               [] (const MatchCand& a, const MatchCand& b) { return a.dist < b.dist; });
    for(const auto& c : matchCands)
    {
        if(parts[(size_t) c.part].matched) continue;
        if(peakMatchedPartial[(size_t) c.peak] >= 0) continue;
        auto& p = parts[(size_t) c.part];
        p.matched = true;
        peakMatchedPartial[(size_t) c.peak] = c.part;
        const int pb = peakBin[(size_t) c.peak];
        const float fAna = pvFreq[(size_t) pb];
        p.freqAna = fAna;
        p.targetFreq = peakFreq[(size_t) c.peak] * peakRatio[(size_t) c.peak];
        p.freqNat = peakFreq[(size_t) c.peak];
        p.targetAmp = peakAmp[(size_t) c.peak];
        float kre, kim;
        const float dAna = (float) pb - fAna / binWidth;
        p.anaPhase = kernelAt(dAna, kre, kim)
                        ? wrapPhase(pvPhase[(size_t) pb] - fastAtan2(kim, kre))
                        : pvPhase[(size_t) pb];
        if(peakBaseIdx[(size_t) c.peak] >= subBaseBase)
        { p.baseIdx = peakBaseIdx[(size_t) c.peak]; p.harmonic = peakHarmonic[(size_t) c.peak]; }
        else
            identifyHarmonic(peakFreq[(size_t) c.peak], p.baseIdx, p.harmonic);
        p.dying = 0;
    }
    for(auto& p : parts)
    {
        if(! p.matched) continue;
        const float fMean = 0.5f * (p.freqOut + p.targetFreq);
        p.phase = wrapPhase(p.phase + twoPi * fMean * hopSec);
        p.freqOut = p.targetFreq;
        p.amp = p.targetAmp;
        ++p.age;
    }
    for(int i = 0; i < K; ++i)
    {
        if(peakMatchedPartial[(size_t) i] >= 0) continue;
        const float fOut = peakFreq[(size_t) i] * peakRatio[(size_t) i];
        if(fOut <= 0.0f) continue;
        Partial p;
        p.id = nextPartialId[(size_t) channel]++;
        p.freqOut = fOut;
        p.freqNat = peakFreq[(size_t) i];
        p.amp = peakAmp[(size_t) i];
        const int pb = peakBin[(size_t) i];
        const float fAna = pvFreq[(size_t) pb];
        p.freqAna = fAna;
        float kre, kim;
        const float dAna = (float) pb - fAna / binWidth;
        p.anaPhase = kernelAt(dAna, kre, kim)
                        ? wrapPhase(pvPhase[(size_t) pb] - fastAtan2(kim, kre))
                        : pvPhase[(size_t) pb];
        p.phase = p.anaPhase;
        p.targetFreq = fOut;
        p.targetAmp = p.amp;
        if(peakBaseIdx[(size_t) i] >= subBaseBase)
        { p.baseIdx = peakBaseIdx[(size_t) i]; p.harmonic = peakHarmonic[(size_t) i]; }
        else
            identifyHarmonic(peakFreq[(size_t) i], p.baseIdx, p.harmonic);
        p.matched = true;
        parts.push_back(p);
    }
    for(auto& p : parts)
    {
        if(p.matched) continue;
        if(p.dying == 0) p.dying = partialDeathFrames;
        --p.dying;
        p.phase = wrapPhase(p.phase + twoPi * p.freqOut * hopSec);
        p.amp *= (float) juce::jmax(0, p.dying) / (float) partialDeathFrames;
    }
    parts.erase(std::remove_if(parts.begin(), parts.end(),
                    [] (const Partial& p) { return ! p.matched && p.dying <= 0; }),
                 parts.end());
    const float hopTwoPi = twoPi * hopSec;
    auto& refs = basePhase[(size_t) channel];
    for(auto& r : refs) r.matched = false;
    for(size_t ai = 0; ai < parts.size(); ++ai)
    {
        const int b = parts[ai].baseIdx;
        if(b < 0) continue;
        bool first = true;
        for(size_t q = 0; q < ai; ++q)
            if(parts[q].baseIdx == b) { first = false; break; }
        if(!first) continue;
        Partial* anchor = nullptr;
        for(auto& p : parts)
            if(p.baseIdx == b && p.harmonic >= 1 && p.dying == 0)
                if(anchor == nullptr || p.harmonic < anchor->harmonic
                    || (p.harmonic == anchor->harmonic && p.amp > anchor->amp))
                    anchor = &p;
        if(anchor == nullptr) continue;
        const float na = (float) anchor->harmonic;
        const float f0Ana = anchor->freqAna / na;
        const float f0Out = anchor->freqOut / na;
        if(f0Ana <= 0.0f || f0Out <= 0.0f) continue;
        BasePhaseRef* ref = nullptr;
        float bestDev = basePhaseMatchCents;
        for(auto& r : refs)
        {
            if(r.matched || r.f0Ana <= 0.0f) continue;
            const float dev = std::abs(1200.0f * std::log2(f0Ana / r.f0Ana));
            if(dev < bestDev) { bestDev = dev; ref = &r; }
        }
        if(ref == nullptr)
        {
            refs.push_back({});
            ref = &refs.back();
            ref->delta = 0.0f;
            ref->f0Ana = f0Ana;
            ref->f0Out = f0Out;
        }
        ref->matched = true;
        ref->missing = 0;
        const float dF = 0.5f * ((f0Out - f0Ana) + (ref->f0Out - ref->f0Ana));
        ref->delta = wrapPhase(ref->delta + hopTwoPi * dF);
        ref->f0Ana = f0Ana;
        ref->f0Out = f0Out;
        const float d = ref->delta;
        for(auto& p : parts)
        {
            if(p.baseIdx != b || p.harmonic < 1 || p.dying != 0) continue;
            p.phase = wrapPhase(p.anaPhase + (float) p.harmonic * d);
        }
    }
    for(auto& r : refs) if(!r.matched) ++r.missing;
    refs.erase(std::remove_if(refs.begin(), refs.end(),
        [] (const BasePhaseRef& r) { return r.missing > basePhaseDeathFrames; }), refs.end());
    if(channel == 0) ++frameCounter;
}
void NewProjectAudioProcessor::identifyHarmonic(float freqAnalysis,
                                                 int& baseIdxOut, int& harmonicOut) const
{
    const float nyq = (float) (sampleRate * 0.5);
    const float center = (centerParam != nullptr) ? centerParam->load() : 1000.0f;
    const float spread = (spreadParam != nullptr) ? spreadParam->load() : 2.0f;
    const float fMin = juce::jlimit(20.0f, nyq, center * std::pow(2.0f, -spread));
    if(freqAnalysis < fMin) { baseIdxOut = -1; harmonicOut = 0; return; }
    const float log2fa = std::log2 (freqAnalysis);
    int bBase = -1, bN = 1;
    float bDev = 1.0e9f;
    for(int b = 0; b < numBases; ++b)
    {
        const float Fb = baseFreq[(size_t) b];
        if(Fb <= 0.0f) continue;
        const int n = juce::jmax(1, (int) std::lround(freqAnalysis / Fb));
        const float ln = (n <= harmonicsTagMax) ? log2IntTable[(size_t) n] : std::log2 ((float) n);
        const float dev = 1200.0f * std::abs(log2fa - ln - baseLog2Hz[(size_t) b]);
        if(dev < bDev) { bDev = dev; bBase = b; bN = n; }
    }
    const float gapN = (bN <= harmonicsTagMax) ? gapCentsTable[(size_t) bN]
                                               : 1200.0f * std::log2((float)(bN + 1) / (float) bN);
    const float tol = juce::jmin(50.0f, identifyGapFrac * gapN);
    if(bDev < tol) { baseIdxOut = bBase; harmonicOut = bN; }
    else { baseIdxOut = -1; harmonicOut = 0; }
}
void NewProjectAudioProcessor::processPV(int channel)
{
    detect(channel);
}
float NewProjectAudioProcessor::destinationFrequency(float /*sourceFreq*/, int bin) const
{
    return binDestFreq[(size_t) bin];
}
void NewProjectAudioProcessor::mapToDestination(int channel, bool computePhase)
{
    std::fill(dstMag.begin(), dstMag.begin() + numBins, 0.0f);
    std::fill(dstFreqNum.begin(), dstFreqNum.begin() + numBins, 0.0f);
    std::fill(dstHitCount.begin(), dstHitCount.begin() + numBins, 0.0f);
    std::fill(dstRetuneWeight.begin(), dstRetuneWeight.begin() + numBins, 0.0f);
    std::fill(dstRetunePeak.begin(), dstRetunePeak.begin() + numBins, 0.0f);
    const bool feedbackActive = (feedbackParam != nullptr)
        && (feedbackParam->load() > 0.0f)
        && (attractionParam != nullptr && attractionParam->load() > 0.0f);
    if(feedbackActive)
        std::fill(feedbackAddedMag.begin(), feedbackAddedMag.begin() + numBins, 0.0f);
    if(computePhase)
        std::fill(dstBestMag.begin(), dstBestMag.begin() + numBins, 0.0f);
    const float nyq = (float) (sampleRate * 0.5);
    const float center = centerParam->load();
    const float spread = spreadParam->load();
    const float detectMinHz = juce::jlimit(20.0f, nyq, center * std::pow(2.0f, -spread));
    const auto& bypassMask = transientBypassMask[(size_t) channel];
    for(int k = 0; k < numBins; ++k)
    {
        if(bypassMask[(size_t) k] != 0)
            continue;
        const float g = destinationFrequency(pvFreq[(size_t) k], k);
        const int j = (int) std::lround(g / binWidth);
        if(j >= 0 && j < numBins)
        {
            const float m = pvMag[(size_t) k];
            const float srcHz = pvFreq[(size_t) k] * pitchRatio;
            const float retuneAmt = (srcHz >= detectMinHz)
                ? std::sqrt(juce::jlimit(0.0f, 1.0f, binTargetAffinity[(size_t) k]))
                : 0.0f;
            dstMag[(size_t) j] += m;
            dstFreqNum[(size_t) j] += m * g;
            dstHitCount[(size_t) j] += 1.0f;
            dstRetuneWeight[(size_t) j] += m * retuneAmt;
            dstRetunePeak[(size_t) j] = std::max(dstRetunePeak[(size_t) j], retuneAmt);
            if(computePhase && m > dstBestMag[(size_t) j])
            {
                dstBestMag[(size_t) j] = m;
                dstPhase[(size_t) j] = pvPhase[(size_t) k];
            }
        }
    }
    applyEnvelopeCompensation();
    std::copy(dstMag.begin(), dstMag.begin() + numBins, preFeedbackMag.begin());
    if(feedbackActive)
    {
        const float fbForDecay = juce::jlimit(0.0f, 1.0f, feedbackParam->load());
        const float holdDecay = 0.60f + 0.395f * fbForDecay;
        auto& capHold = capHoldMag[(size_t) channel];
        for(int j = 0; j < numBins; ++j)
            capHold[(size_t) j] = juce::jmax(preFeedbackMag[(size_t) j], capHold[(size_t) j] * holdDecay);
        {
            const float fbKnob = juce::jlimit(0.0f, 1.0f, feedbackParam->load());
            const float attract = juce::jlimit(0.0f, 1.0f, attractionParam->load()) ;
            {
                const float baseHopRatio = 0.25f;
                const float hopRatio = (fftSize > 0) ? ((float) hopSize / (float) fftSize) : baseHopRatio;
                const float norm = std::pow(juce::jmax(1.0e-6f, hopRatio / baseHopRatio), 0.25f);
                const float fbDrive = 2.3f;
                const float fb = juce::jlimit(0.0f, 0.996f, fbDrive * fbKnob * attract * norm);
                const float capMul = 1.0f + 0.22f * fbKnob;
                auto& prevMag = prevDstMag[(size_t) channel];
                auto& prevFreq = prevDstFreq[(size_t) channel];
                auto& prevPre = prevPreFbMag[(size_t) channel];
                for(int j = 0; j < numBins; ++j)
                {
                    const float dm = dstMag[(size_t) j];
                    if(dm <= 0.0f)
                        continue;
                    const float binHz = (float) j * binWidth;
                    if(binHz < detectMinHz)
                        continue;
                    if(dstRetuneWeight[(size_t) j] <= 1.0e-9f)
                        continue;
                    const float peakAff = juce::jlimit(0.0f, 1.0f, dstRetunePeak[(size_t) j]);
                    const float retunedRatio = dstRetuneWeight[(size_t) j] / (dm + 1.0e-9f);
                    const float minRetuned = attract * (0.10f + 0.35f * peakAff);
                    const float retunedNow = juce::jlimit(0.0f, 1.0f,
                                                           juce::jmax(4.5f * retunedRatio, minRetuned));
                    if(retunedNow <= 0.0f)
                        continue;
                    const int jm1 = juce::jmax(0, j - 1);
                    const int jp1 = juce::jmin(numBins - 1, j + 1);
                    const float localNow = juce::jmax(preFeedbackMag[(size_t) jm1],
                                        juce::jmax(preFeedbackMag[(size_t) j], preFeedbackMag[(size_t) jp1]));
                    const float localHist = juce::jmax(prevPre[(size_t) jm1],
                                         juce::jmax(prevPre[(size_t) j], prevPre[(size_t) jp1]));
                    const float localHold = juce::jmax(capHold[(size_t) jm1],
                                         juce::jmax(capHold[(size_t) j], capHold[(size_t) jp1]));
                    const float refMax = juce::jmax(localNow, juce::jmax(localHist, localHold));
                    const float cap = refMax * capMul;
                    const float addMagRaw = (fb * retunedNow) * prevMag[(size_t) j];
                    const float addMag = juce::jlimit(0.0f, juce::jmax(0.0f, cap - dstMag[(size_t) j]), addMagRaw);
                    if(addMag <= 0.0f)
                        continue;
                    dstMag[(size_t) j] += addMag;
                    dstFreqNum[(size_t) j] += addMag * prevFreq[(size_t) j];
                    feedbackAddedMag[(size_t) j] += addMag;
                }
            }
        }
    }
    for(int j = 0; j < numBins; ++j)
        dstFreq[(size_t) j] = (dstMag[(size_t) j] > 0.0f)
                                ? dstFreqNum[(size_t) j] / dstMag[(size_t) j]
                                : 0.0f;
    if(feedbackActive)
    {
        auto& prevMag = prevDstMag[(size_t) channel];
        auto& prevFreq = prevDstFreq[(size_t) channel];
        auto& prevPre = prevPreFbMag[(size_t) channel];
        std::copy(dstMag.begin(), dstMag.begin() + numBins, prevMag.begin());
        std::copy(dstFreq.begin(), dstFreq.begin() + numBins, prevFreq.begin());
        std::copy(preFeedbackMag.begin(), preFeedbackMag.begin() + numBins, prevPre.begin());
    }
    if(feedbackActive)
    {
        const float compAmount = 0.28f;
        for(int j = 0; j < numBins; ++j)
        {
            const float added = feedbackAddedMag[(size_t) j];
            if(added <= 0.0f)
                continue;
        const float postMag = dstMag[(size_t) j];
        const float preMag = juce::jmax(1.0e-6f, postMag - added);
        const float gainIncrease = added / preMag;
        const float addGain = 1.0f / (1.0f + compAmount * gainIncrease);
        const float compensatedAdded = added * addGain;
        dstMag[(size_t) j] = preMag + compensatedAdded;
        const float postNum = dstFreqNum[(size_t) j];
        const float preNum = postNum - added * prevDstFreq[(size_t) channel][(size_t) j];
        dstFreqNum[(size_t) j] = preNum + compensatedAdded * prevDstFreq[(size_t) channel][(size_t) j];
        }
    }
}
void NewProjectAudioProcessor::applyEnvelopeCompensation()
{
    const float amount = (envCompParam != nullptr) ? juce::jlimit(0.0f, 1.0f, envCompParam->load()) : 0.0f;
    if(amount <= 0.0f)
        return;
    std::fill(envRefMag.begin(), envRefMag.begin() + numBins, 0.0f);
    const float invPitch = 1.0f / juce::jmax(1.0e-6f, pitchRatio);
    for(int j = 0; j < numBins; ++j)
    {
        const float src = (float) j * invPitch;
        if(src <= 0.0f)
        {
            envRefMag[(size_t) j] = pvMag[0];
            continue;
        }
        const int i0 = (int) std::floor(src);
        if(i0 >= numBins - 1)
        {
            envRefMag[(size_t) j] = 0.0f;
            continue;
        }
        const float frac = src - (float) i0;
        const float m0 = pvMag[(size_t) juce::jmax(0, i0)];
        const float m1 = pvMag[(size_t) juce::jmax(0, i0 + 1)];
        envRefMag[(size_t) j] = m0 + (m1 - m0) * frac;
    }
    envRefPrefix[0] = 0.0f;
    envOutPrefix[0] = 0.0f;
    for(int j = 0; j < numBins; ++j)
    {
        envRefPrefix[(size_t) (j + 1)] = envRefPrefix[(size_t) j] + envRefMag[(size_t) j];
        envOutPrefix[(size_t) (j + 1)] = envOutPrefix[(size_t) j] + dstMag[(size_t) j];
    }
    const float maxOctSpan = 3.0f;
    const float minOctSpan = 0.5f;
    const float octSpan = maxOctSpan - (maxOctSpan - minOctSpan) * amount;
    const float bwFactor = std::pow(2.0f, octSpan * 0.5f) - 1.0f;
    const float alpha = 0.15f + 0.85f * amount;
    const float collisionStrength = 0.9f * amount;
    const float eps = 1.0e-8f;
    const float sizeRef = (float) (1 << 12);
    const float sizeNorm = (fftSize > 0) ? (sizeRef / (float) fftSize) : 1.0f;
    for(int j = 1; j < numBins; ++j)
    {
        const int radius = juce::jlimit(1, 512, (int) std::lround((float) j * bwFactor));
        const int a = juce::jmax(0, j - radius);
        const int b = juce::jmin(numBins - 1, j + radius);
        const float lenInv = 1.0f / (float) (b - a + 1);
        const float refAvg = (envRefPrefix[(size_t) (b + 1)] - envRefPrefix[(size_t) a]) * lenInv;
        const float outAvg = (envOutPrefix[(size_t) (b + 1)] - envOutPrefix[(size_t) a]) * lenInv;
        const float ratio = (refAvg + eps) / (outAvg + eps);
        const float envGain = juce::jlimit(0.25f, 4.0f, fastPow(ratio, alpha));
        const float hits = 1.0f + (dstHitCount[(size_t) j] - 1.0f) * sizeNorm;
        const float collisionGain = (hits > 1.0f)
            ? fastPow(1.0f / hits, collisionStrength)
            : 1.0f;
        const float gain = juce::jlimit(0.2f, 4.0f, envGain * collisionGain);
        dstMag[(size_t) j] *= gain;
        dstFreqNum[(size_t) j] *= gain;
    }
}
void NewProjectAudioProcessor::synthesizeAdditive(int channel, bool reseed)
{
    float* fd = fftData.data();
    float* outPh = outPhase[(size_t) channel].data();
    const int N = fftSize;
    const auto& bypassRe = transientBypassRe[(size_t) channel];
    const auto& bypassIm = transientBypassIm[(size_t) channel];
    const auto& bypassMask = transientBypassMask[(size_t) channel];
    const float twoPi = juce::MathConstants<float>::twoPi;
    const float hopDuration = (float) hopSize / (float) sampleRate;
    if(reseed)
    {
        for(int j = 0; j < numBins; ++j)
            outPh[j] = (dstMag[(size_t) j] > 0.0f) ? dstPhase[(size_t) j] : 0.0f;
        synthSeed[(size_t) channel] = false;
    }
    std::fill(accRe.begin(), accRe.begin() + numBins, 0.0f);
    std::fill(accIm.begin(), accIm.begin() + numBins, 0.0f);
    std::fill(accCov.begin(), accCov.begin() + numBins, 0.0f);
    const float morph = juce::jlimit(0.0f, 1.0f, morphParam->load());
    const int L = (int) synKernelRe.size();
    const int center = kernelHalf * kernelOS;
    for(const auto& p : partials[(size_t) channel])
    {
        if(p.freqOut <= 0.0f) continue;
        float birth = 1.0f;
        if(p.age < partialBirthFrames)
            birth *= (float) (p.age + 1) / (float) partialBirthFrames;
        if(birth <= 0.0f) continue;
        const float amp = juce::jmax(0.0f, p.amp) * birth;
        const float b = p.freqOut / binWidth;
        float sphi, cphi;
        fastSinCos(p.phase, sphi, cphi);
        const int j0 = juce::jmax(0, (int) std::ceil(b - kernelHalf));
        const int j1 = juce::jmin(numBins - 1, (int) std::floor(b + kernelHalf));
        for(int j = j0; j <= j1; ++j)
        {
            const float tf = ((float) j - b) * (float) kernelOS + (float) center;
            const int ti = (int) tf;
            if(ti < 0 || ti + 1 >= L) continue;
            const float fr = tf - (float) ti;
            const float kre = synKernelRe[(size_t) ti]
                            + (synKernelRe[(size_t)(ti+1)] - synKernelRe[(size_t) ti]) * fr;
            const float kim = synKernelIm[(size_t) ti]
                            + (synKernelIm[(size_t)(ti+1)] - synKernelIm[(size_t) ti]) * fr;
            const float kmag = synKernelMag[(size_t) ti]
                             + (synKernelMag[(size_t)(ti+1)] - synKernelMag[(size_t) ti]) * fr;
            accRe[(size_t) j] += amp * (cphi * kre - sphi * kim);
            accIm[(size_t) j] += amp * (cphi * kim + sphi * kre);
            accCov[(size_t) j] = juce::jmax(accCov[(size_t) j], birth * kmag);
        }
    }
    for(int j = 0; j < numBins; ++j)
    {
        const float mag = dstMag[(size_t) j];
        if(mag <= 0.0f)
        {
            fd[2 * j] = 0.0f;
            fd[2 * j + 1] = 0.0f;
        }
        else
        {
            if(! reseed)
                outPh[j] = wrapPhase(outPh[j] + dstFreq[(size_t) j] * twoPi * hopDuration);
            if(bypassMask[(size_t) j] == 0)
            {
                const float cov = juce::jmin(1.0f, accCov[(size_t) j]);
                float phase = outPh[j];
                if(cov > 1.0e-4f && (accRe[(size_t) j] != 0.0f || accIm[(size_t) j] != 0.0f))
                {
                    const float targetPhase = fastAtan2 (accIm[(size_t) j], accRe[(size_t) j]);
                    const float t = morph * cov;
                    phase = wrapPhase(outPh[j] + t * wrapPhase(targetPhase - outPh[j]));
                }
                float sp, cp;
                fastSinCos(phase, sp, cp);
                fd[2 * j] = mag * cp;
                fd[2 * j + 1] = mag * sp;
            }
        }
        if(bypassMask[(size_t) j] != 0)
        {
            fd[2 * j] = bypassRe[(size_t) j];
            fd[2 * j + 1] = bypassIm[(size_t) j];
        }
    }
    fd[1] = 0.0f;
    fd[2 * (N / 2) + 1] = 0.0f;
}
void NewProjectAudioProcessor::publishSpectrum()
{
    auto& s = spectrumBridge.startWrite();
    s.numBins = numBins;
    s.binWidth = binWidth;
    s.sampleRate = sampleRate;
    s.pvBypassed = (bypassParam == nullptr || bypassParam->load() < 0.5f);
    const int nb = juce::jmin(numBins, (int) s.mag.size());
    if(s.pvBypassed)
    {
        for(int j = 0; j < nb; ++j)
        {
            s.mag [(size_t) j] = pvMag [(size_t) j] * spectrumNorm;
            s.freq[(size_t) j] = pvFreq[(size_t) j];
        }
    }
    else
    {
        const auto& bMask = transientBypassMask[0];
        const auto& bRe = transientBypassRe[0];
        const auto& bIm = transientBypassIm[0];
        for(int j = 0; j < nb; ++j)
        {
            if(! tsActive && bMask[(size_t) j] != 0)
            {
                const float re = bRe[(size_t) j], im = bIm[(size_t) j];
                s.mag [(size_t) j] = std::sqrt(re * re + im * im) * spectrumNorm;
                s.freq[(size_t) j] = pvFreq[(size_t) j];
            }
            else
            {
                const float trMag = tsActive ? tsDispMag[(size_t) j] : 0.0f;
                if(trMag > dstMag[(size_t) j])
                {
                    s.mag [(size_t) j] = trMag * spectrumNorm;
                    s.freq[(size_t) j] = pvFreq[(size_t) j];
                }
                else
                {
                    s.mag [(size_t) j] = dstMag [(size_t) j] * spectrumNorm;
                    s.freq[(size_t) j] = dstFreq[(size_t) j];
                }
            }
        }
    }
    const int nbases = juce::jmin(numBases, (int) s.baseHz.size());
    for(int i = 0; i < nbases; ++i)
    {
        s.baseHz [(size_t) i] = baseDisplayHz[(size_t) i];
        s.baseConf[(size_t) i] = baseConf [(size_t) i];
    }
    s.numBases = nbases;
    spectrumBridge.publish();
}
void NewProjectAudioProcessor::processFrame(int channel)
{
    if(currentOrder < minOrder || numBins <= 0)
        return;
    float* fd = fftData.data();
    const float* in = inputFifo [(size_t) channel].data();
    float* out = outputFifo[(size_t) channel].data();
    const int wrapSplit = fftSize - pos;
    for(int i = 0; i < wrapSplit; ++i)
        fd[i] = in[(size_t)(pos + i)] * window[(size_t) i];
    for(int i = wrapSplit; i < fftSize; ++i)
        fd[i] = in[(size_t)(i - wrapSplit)] * window[(size_t) i];
    fftEngines[(size_t) (currentOrder - minOrder)]->performRealOnlyForwardTransform(fd, false);
    analyze(channel);
    const bool pvBypassed = (bypassParam == nullptr || bypassParam->load() < 0.5f);
    if(pvBypassed)
    {
        fluxBaseline[(size_t) channel] = 0.0f;
        heldStrength[(size_t) channel] = 0.0f;
        holdRemaining[(size_t) channel] = 0;
        transientInit[(size_t) channel] = false;
        synthSeed[(size_t) channel] = true;
        std::fill(prevDstMag[(size_t) channel].begin(), prevDstMag[(size_t) channel].begin() + numBins, 0.0f);
        std::fill(prevDstFreq[(size_t) channel].begin(), prevDstFreq[(size_t) channel].begin() + numBins, 0.0f);
        std::fill(prevPreFbMag[(size_t) channel].begin(), prevPreFbMag[(size_t) channel].begin() + numBins, 0.0f);
        std::fill(capHoldMag[(size_t) channel].begin(), capHoldMag[(size_t) channel].begin() + numBins, 0.0f);
        if(channel == 0)
        {
            numBases = 0;
            publishSpectrum();
        }
        fftEngines[(size_t) (currentOrder - minOrder)]->performRealOnlyInverseTransform(fd);
        for(int i = 0; i < wrapSplit; ++i)
            out[(size_t)(pos + i)] += fd[i] * window[(size_t) i] * windowCorrection;
        for(int i = wrapSplit; i < fftSize; ++i)
            out[(size_t)(i - wrapSplit)] += fd[i] * window[(size_t) i] * windowCorrection;
        return;
    }
    const float ts = detectTransient(channel);
    updateTransientBypass(channel);
    if(tsActive)
    {
        tsMask[(size_t) channel].compute(pvMag.data(), tsMaskBuf.data());
        float* sc = tsScratch.data();
        std::copy(fd, fd + 2 * fftSize, sc);
        for(int k = 0; k < numBins; ++k)
        {
            const float mk = tsMaskBuf[(size_t) k];
            const float full = pvMag[(size_t) k];
            sc[2 * k] *= mk;
            sc[2 * k + 1] *= mk;
            pvMag[(size_t) k] = full * (1.0f - mk);
            if(channel == 0) tsDispMag[(size_t) k] = full * mk;
        }
        fftEngines[(size_t) (currentOrder - minOrder)]->performRealOnlyInverseTransform(sc);
        for(int i = 0; i < wrapSplit; ++i)
            out[(size_t)(pos + i)] += sc[i] * window[(size_t) i] * windowCorrection;
        for(int i = wrapSplit; i < fftSize; ++i)
            out[(size_t)(i - wrapSplit)] += sc[i] * window[(size_t) i] * windowCorrection;
    }
    if(ts >= transientReseedThreshold) formantHopCounter[(size_t) channel] = 0;
    applyFormantShift(channel);
    processPV(channel);
    updatePartials(channel);
    const bool reseed = synthSeed[(size_t) channel] || (ts >= transientReseedThreshold);
    mapToDestination(channel, true);
    synthesizeAdditive(channel, reseed);
    if(channel == 0)
        publishSpectrum();
    fftEngines[(size_t) (currentOrder - minOrder)]->performRealOnlyInverseTransform(fd);
    for(int i = 0; i < wrapSplit; ++i)
        out[(size_t)(pos + i)] += fd[i] * window[(size_t) i] * windowCorrection;
    for(int i = wrapSplit; i < fftSize; ++i)
        out[(size_t)(i - wrapSplit)] += fd[i] * window[(size_t) i] * windowCorrection;
}
void NewProjectAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    if(fftEngines.empty() || window.empty())
        return;
    const int targetMode = (targetModeParam != nullptr) ? juce::roundToInt(targetModeParam->load()) : 0;
    if(targetMode == 1)
    {
        for(const auto meta : midiMessages)
        {
            const auto m = meta.getMessage();
            if(m.isNoteOn())
            {
                const int nn = juce::jlimit(0, 127, m.getNoteNumber());
                const int pc = nn % 12;
                midiHeldNote[nn] = juce::jmin(127, midiHeldNote[nn] + 1);
                midiHeld[pc] = juce::jmin(127, midiHeld[pc] + 1);
            }
            else if(m.isNoteOff())
            {
                const int nn = juce::jlimit(0, 127, m.getNoteNumber());
                const int pc = nn % 12;
                midiHeldNote[nn] = juce::jmax(0, midiHeldNote[nn] - 1);
                midiHeld[pc] = juce::jmax(0, midiHeld[pc] - 1);
            }
            else if(m.isAllNotesOff() || m.isAllSoundOff())
            {
                for(int n = 0; n < 128; ++n) midiHeldNote[n] = 0;
                for(int i = 0; i < 12; ++i) midiHeld[i] = 0;
            }
        }
        int mask = 0;
        for(int i = 0; i < 12; ++i)
            if(midiHeld[i] > 0) mask |= (1 << i);
        if(mask != lastMidiMask)
        {
            lastMidiMask = mask;
            midiScaleMask.store(mask);
        }
    }
    else if(lastMidiMask != -1)
    {
        for(int n = 0; n < 128; ++n) midiHeldNote[n] = 0;
        for(int i = 0; i < 12; ++i) midiHeld[i] = 0;
        lastMidiMask = -1;
        midiScaleMask.store(0);
    }
    reconfigure(minOrder + (int) sizeParam->load(),
                 overlapFromIndex((int) overlapParam->load()));
    const int totalIn = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    for(int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());
    if(auto* scBus = getBus(true, 1); scBus != nullptr && scBus->isEnabled())
    {
        auto scBuffer = getBusBuffer(buffer, true, 1);
        float pk = 0.0f;
        for(int ch = 0; ch < scBuffer.getNumChannels(); ++ch)
            pk = juce::jmax(pk, scBuffer.getMagnitude(ch, 0, scBuffer.getNumSamples()));
        scInputPeak.store(pk, std::memory_order_relaxed);
    }
    else
        scInputPeak.store(-1.0f, std::memory_order_relaxed);
    const bool pvBypassed = (bypassParam == nullptr || bypassParam->load() < 0.5f);
    const bool scBusOn = [this] { auto* b = getBus(true, 1); return b != nullptr && b->isEnabled(); }();
    const bool scMode = (targetMode == 2);
    const bool scVisualRun = scBusOn && scMode;
    const bool scRun = scVisualRun && ! pvBypassed;
    SidechainDetector<maxBins, maxBases>::Params scP;
    const float* scIn[2] = { nullptr, nullptr };
    int scNumCh = 0;
    float scNorm = 1.0f;
    if(scVisualRun)
    {
        auto scBuffer = getBusBuffer(buffer, true, 1);
        scNumCh = juce::jmin(2, scBuffer.getNumChannels());
        for(int ch = 0; ch < scNumCh; ++ch) scIn[ch] = scBuffer.getReadPointer(ch);
        scNorm = (scNumCh > 0) ? 1.0f / (float) scNumCh : 1.0f;
    }
    if(scRun)
    {
        const float nyq = (float) (sampleRate * 0.5);
        const float center = centerParam->load();
        const float spread = spreadParam->load();
        const float density = juce::jlimit(0.0f, 1.0f, densityParam->load());
        scP.fMin = juce::jlimit(20.0f, nyq, center * std::pow(2.0f, -spread));
        scP.fMax = juce::jlimit(20.0f, nyq, center * std::pow(2.0f, spread));
        scP.thr = densityToSalienceThreshold(density);
        scP.moreBases = (moreBasesParam != nullptr && moreBasesParam->load() >= 0.5f);
        scP.pitchRatio = std::pow(2.0f, scPitchParam->load() / 12.0f);
    }
    const int numCh = juce::jmin(buffer.getNumChannels(), maxChannels);
    const int numSamples = buffer.getNumSamples();
    const bool ms = (msParam->load() >= 0.5f) && numCh == 2;
    float* chPtr[maxChannels] = { nullptr, nullptr };
    for(int ch = 0; ch < numCh; ++ch)
        chPtr[ch] = buffer.getWritePointer(ch);
    const bool enhanceTs = (enhanceTransientParam != nullptr && enhanceTransientParam->load() >= 0.5f);
    const float knob = juce::jlimit(0.0f, 1.0f, transientParam != nullptr ? transientParam->load() : 0.0f);
    tsMask[0].setThreshold(knob);
    tsMask[1].setThreshold(knob);
    tsActive = enhanceTs && (knob > 1.0e-4f);
    for(int s = 0; s < numSamples; ++s)
    {
        if(ms)
        {
            const float L = chPtr[0][s];
            const float R = chPtr[1][s];
            const float mo = outputFifo[0][(size_t) pos];
            const float so = outputFifo[1][(size_t) pos];
            outputFifo[0][(size_t) pos] = 0.0f;
            outputFifo[1][(size_t) pos] = 0.0f;
            inputFifo[0][(size_t) pos] = 0.5f * (L + R);
            inputFifo[1][(size_t) pos] = 0.5f * (L - R);
            chPtr[0][s] = mo + so;
            chPtr[1][s] = mo - so;
        }
        else
        {
            for(int ch = 0; ch < numCh; ++ch)
            {
                const float input = chPtr[ch][s];
                chPtr[ch][s] = outputFifo[(size_t) ch][(size_t) pos];
                outputFifo[(size_t) ch][(size_t) pos] = 0.0f;
                inputFifo [(size_t) ch][(size_t) pos] = input;
            }
        }
        if(scVisualRun)
        {
            float scs = 0.0f;
            for(int ch = 0; ch < scNumCh; ++ch) scs += scIn[ch][s];
            scDetector.pushSample(pos, scs * scNorm);
        }
        pos = (pos + 1) & fftMask;
        if(++count == hopSize)
        {
            count = 0;
            if(scRun)
            {
                scDetector.processHop(pos, scP);
                scNumTargets = scDetector.copyTargets(scTargetHz.data(), maxBases);
            }
            else
            {
                scNumTargets = 0;
                if(pvBypassed && scVisualRun)
                    scDetector.processBypassHop(pos);
                else
                    scDetector.publishGatedFrame(pvBypassed);
            }
            for(int ch = 0; ch < numCh; ++ch)
                processFrame(ch);
        }
    }
}
bool NewProjectAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* NewProjectAudioProcessor::createEditor()
{
    return new NewProjectAudioProcessorEditor(*this);
}
void NewProjectAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if(auto xml = apvts.copyState().createXml())
    {
        xml->setAttribute("stateVersion", stateVersion);
        copyXmlToBinary(*xml, destData);
    }
}
void NewProjectAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if(auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        auto loadedVersion = xml->getStringAttribute("stateVersion", "");
        bool needsFFTSizeMigration = loadedVersion.isEmpty() || loadedVersion < "1.1.2";
        auto tree = juce::ValueTree::fromXml(*xml);
        if(needsFFTSizeMigration)
        {
            for(int i = 0; i < tree.getNumChildren(); ++i)
            {
                auto child = tree.getChild(i);
                auto id = child.getProperty("id").toString();
                if(id == "fftSize")
                {
                    float oldVal = (float) child.getProperty("value", 0.0f);
                    int oldIdx = (int) std::round(oldVal * 3.0f);
                    int newIdx = oldIdx + 1;
                    float newVal = (float) newIdx / 4.0f;
                    child.setProperty("value", newVal, nullptr);
                }
            }
        }
        bool hasTargetMode = false;
        bool midiWasOn = false;
        for(int i = 0; i < tree.getNumChildren(); ++i)
        {
            const auto child = tree.getChild(i);
            const auto id = child.getProperty("id").toString();
            if(id == "targetMode") hasTargetMode = true;
            else if(id == "midiMode") midiWasOn = ((float) child.getProperty("value") >= 0.5f);
        }
        apvts.replaceState(tree);
        if(! hasTargetMode && midiWasOn)
            if(auto* p = apvts.getParameter("targetMode"))
                p->setValueNotifyingHost(p->convertTo0to1 (1.0f));
    }
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NewProjectAudioProcessor();
}