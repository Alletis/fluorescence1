#include "PluginEditor.h"
#if __has_include("../build/vs2026/juce_binarydata_NewProjectData/JuceLibraryCode/BinaryData.h")
 #include "../build/vs2026/juce_binarydata_NewProjectData/JuceLibraryCode/BinaryData.h"
#elif __has_include("BinaryData.h")
 #include "BinaryData.h"
#else
 #error "BinaryData.h not found. Run CMake configure/build to generate NewProjectData."
#endif
#include "SpectrumDisplay.h"
NewProjectAudioProcessorEditor::NewProjectAudioProcessorEditor(NewProjectAudioProcessor& p)
    : AudioProcessorEditor(p),
      proc(p),
      spectrum(p.apvts, p.getSpectrumBridge()),
      scSpectrum(p.apvts, p.getSidechainSpectrumBridge()),
      pianoRoll(p.apvts),
      scalePanel(p.apvts)
{
    setLookAndFeel(&knobLnf);
    addAndMakeVisible(content);
    content.setInterceptsMouseClicks(false, true);
    addChildComponent(logoFlyout);
    logoSource = juce::ImageCache::getFromMemory(BinaryData::logo_png, BinaryData::logo_pngSize);
    alletisSource = juce::ImageCache::getFromMemory(BinaryData::brand_png, BinaryData::brand_pngSize);
    alletisLink.setURL("https://alletis.com");
    brand2Source = juce::ImageCache::getFromMemory(BinaryData::brand2_png, BinaryData::brand2_pngSize);
    eyeSource = juce::ImageCache::getFromMemory(BinaryData::eye_png, BinaryData::eye_pngSize);
    hoverHintBox.setOnHeightChanged([safe = juce::Component::SafePointer<NewProjectAudioProcessorEditor> (this)]
    {
        if(safe != nullptr)
            safe->resized();
    });
    content.addAndMakeVisible(alletisLink);
    content.addAndMakeVisible(hoverHintBox);
    content.addAndMakeVisible(spectrum);
    content.addAndMakeVisible(scSpectrum);
    content.addAndMakeVisible(spectrumCoordOverlay);
    spectrumCoordOverlay.setSource(&spectrum);
    spectrumCoordOverlay.toFront(false);
    content.addAndMakeVisible(scSpectrumCoordOverlay);
    scSpectrumCoordOverlay.setSource(&scSpectrum);
    scSpectrumCoordOverlay.toFront(false);
    content.addAndMakeVisible(pianoRoll);
    content.addAndMakeVisible(scalePanel);
    pianoRoll.setMidiMask(&p.getMidiScaleMask());
    spectrum.setMidiMask(&p.getMidiScaleMask());
    scSpectrum.setForceTargetColour(true);
    spectrum.setSidechainTargetCount(&p.getSidechainTargetCount());
    content.addAndMakeVisible(bypassModeBtn);
    bypassModeBtn.setClickingTogglesState(true);
    bypassModeBtn.setNeutralStyle(true);
    bypassAtt = std::make_unique<BA> (p.apvts, "pvBypass", bypassModeBtn);
    const auto syncBypassBtn = [this]
    {
        const bool activeOn = bypassModeBtn.getToggleState();
        bypassModeBtn.setButtonText(activeOn ? "ACTIVE" : "BYPASSED");
        bypassModeBtn.setActive(false);
        bypassMask.setBypassed(! activeOn);
    };
    bypassModeBtn.onClick = syncBypassBtn;
    bypassModeBtn.onStateChange = syncBypassBtn;
    syncBypassBtn();
    fftSizeBox.addItemList({ "512", "1024", "2048", "4096", "8192" }, 1);
    overlapBox.addItemList({ "4x", "8x", "16x" }, 1);
    content.addAndMakeVisible(fftSizeBox);
    content.addAndMakeVisible(overlapBox);
    fftAtt = std::make_unique<CA> (p.apvts, "fftSize", fftSizeBox);
    overlapAtt = std::make_unique<CA> (p.apvts, "overlap", overlapBox);
    content.addAndMakeVisible(fullMidiBtn);
    content.addAndMakeVisible(moreBasesBtn);
    content.addAndMakeVisible(hysteresisBtn);
    content.addAndMakeVisible(midSideBtn);
    modeBox.addItemList({ "Scale", "MIDI", "Sidechain" }, 1);
    content.addAndMakeVisible(modeBox);
    fullMidiAtt = std::make_unique<BA> (p.apvts, "fullMidi", fullMidiBtn);
    moreBasesAtt = std::make_unique<BA> (p.apvts, "moreBases", moreBasesBtn);
    hysteresisAtt = std::make_unique<BA> (p.apvts, "hysteresis", hysteresisBtn);
    midSideAtt = std::make_unique<BA> (p.apvts, "midSide", midSideBtn);
    modeAtt = std::make_unique<CA> (p.apvts, "targetMode", modeBox);
    midSideBtn.setClickingTogglesState(true);
    const auto syncStereoBtn = [this]
    {
        const bool msOn = midSideBtn.getToggleState();
        midSideBtn.setButtonText(msOn ? "MID - SIDE" : "LEFT - RIGHT");
        midSideBtn.setActive(false);
    };
    midSideBtn.onClick = syncStereoBtn;
    midSideBtn.onStateChange = syncStereoBtn;
    syncStereoBtn();
    modeTarget = (modeBox.getSelectedId() == 3) ? 2.0f : 0.0f;
    modePhase = modeTarget;
    applyModeAlphas();
    modeBox.onChange = [this]
    {
        modeTarget = (modeBox.getSelectedId() == 3) ? 2.0f : 0.0f;
        if(! isTimerRunning()) startTimerHz(60);
    };
    setupKnob(transient, "transient", transientAtt);
    setupKnob(transpose, "pitch", transposeAtt);
    setupKnob(attraction, "attraction", attractionAtt);
    setupKnob(emphasis, "emphasis", emphasisAtt);
    setupKnob(morph, "morph", morphAtt);
    setupKnob(density, "density", densityAtt);
    setupKnob(feedback, "feedback", feedbackAtt);
    setupKnob(fineTune, "fineTune", fineTuneAtt);
    setupKnob(envComp, "envComp", envCompAtt);
    setupKnob(formant, "formant", formantAtt);
    transpose.setSliderStyle(juce::Slider::LinearHorizontal);
    transpose.setSliderSnapsToMousePosition(false);
    transpose.setWheelStep(1.0);
    setupKnob(scTranspose, "scPitch", scTransposeAtt);
    scTranspose.setSliderStyle(juce::Slider::LinearHorizontal);
    scTranspose.setSliderSnapsToMousePosition(false);
    scTranspose.setWheelStep(1.0);
    scTranspose.setMagentaAccent(true);
    fineTune.setSliderStyle(juce::Slider::LinearVertical);
    fineTune.setSliderSnapsToMousePosition(false);
    fineTune.setMagentaAccent(true);
    fineTune.setWheelStep(25.0);
    envComp.setSliderStyle(juce::Slider::LinearHorizontal);
    envComp.setSliderSnapsToMousePosition(false);
    envComp.setWheelStep(0.05);
    envComp.setLinearTrackEndCap(ValueKnob::LinearEndCap::BothRounded);
    envComp.setLinearFillEndCap(ValueKnob::LinearEndCap::LeftRounded);
    formant.setSliderStyle(juce::Slider::LinearHorizontal);
    formant.setSliderSnapsToMousePosition(false);
    formant.setWheelStep(1.0);
    attraction.setGradientArc(true);
    attraction.setWheelStep(0.05);
    styleLabel(fftLbl, "FFT SIZE");
    styleLabel(overlapLbl, "FFT OVERLAP");
    styleLabel(stereoLbl, "STEREO");
    styleLabel(transientLbl, "TRANSIENT");
    styleLabel(transposeLbl, "TRANSPOSE");
    styleLabel(scTransposeLbl, "SC TRANSPOSE");
    styleLabel(attractionLbl, "ATTRACTION");
    styleLabel(emphasisLbl, "EMPHASIS");
    styleLabel(morphLbl, "MORPH");
    styleLabel(densityLbl, "DENSITY");
    styleLabel(feedbackLbl, "FEEDBACK");
    styleLabel(fineTuneLbl, "FINE TUNE");
    styleLabel(envCompLbl, "COMPENSATION");
    styleLabel(formantLbl, "FORMANT");
    styleLabel(targetLbl, "TARGETS");
    content.addChildComponent(valueReadout);
    wireReadout(transient, transientLbl);
    wireReadout(transpose, transposeLbl);
    wireReadout(scTranspose, scTransposeLbl);
    wireReadout(attraction, attractionLbl);
    wireReadout(emphasis, emphasisLbl);
    wireReadout(morph, morphLbl);
    wireReadout(density, densityLbl);
    wireReadout(feedback, feedbackLbl);
    wireReadout(fineTune, fineTuneLbl);
    wireReadout(envComp, envCompLbl);
    wireReadout(formant, formantLbl);
    content.addChildComponent(flyoutA);
    flyoutA.addAndMakeVisible(transient);
    flyoutA.addAndMakeVisible(morph);
    flyoutA.addAndMakeVisible(feedback);
    flyoutA.addAndMakeVisible(transientLbl);
    flyoutA.addAndMakeVisible(morphLbl);
    flyoutA.addAndMakeVisible(feedbackLbl);
    flyoutA.addAndMakeVisible(overlapLbl);
    flyoutA.addAndMakeVisible(overlapBox);
    flyoutA.addAndMakeVisible(fullMidiBtn);
    flyoutA.addAndMakeVisible(moreBasesBtn);
    flyoutA.addAndMakeVisible(hysteresisBtn);
    flyoutA.addAndMakeVisible(formantLbl);
    flyoutA.addAndMakeVisible(formant);
    flyoutA.addAndMakeVisible(envCompLbl);
    flyoutA.addAndMakeVisible(envComp);
    flyoutA.addAndMakeVisible(enhanceTransientBtn);
    enhanceTransientAtt = std::make_unique<BA> (p.apvts, "enhanceTransient", enhanceTransientBtn);
    content.addAndMakeVisible(menuButtonA);
    menuButtonA.onClick = [this]
    {
        flyoutA.toggle();
        const bool open = flyoutA.isOpen();
        menuButtonA.setActive(open);
        proc.apvts.state.setProperty("uiSecondaryOpen", open, nullptr);
    };
    flyoutA.onClose = [this]
    {
        menuButtonA.setActive(false);
        proc.apvts.state.setProperty("uiSecondaryOpen", false, nullptr);
    };
    flyoutA.onPinnedChanged = [this] (bool pinned)
    {
        proc.apvts.state.setProperty("uiSecondaryPinned", pinned, nullptr);
    };
    flyoutA.setPinned((bool) proc.apvts.state.getProperty("uiSecondaryPinned", false));
    hoverHintBox.setCollapsed(! (bool) proc.apvts.state.getProperty("uiHintOpen", false));
    hoverHintBox.onCollapsedChanged = [this]
    {
        proc.apvts.state.setProperty("uiHintOpen", ! hoverHintBox.isCollapsed(), nullptr);
    };
    setWantsKeyboardFocus(true);
    addMouseListener(&mouseSpy, true);
    setResizable(true, true);
    content.addChildComponent(bypassMask);
    bypassMask.primeTo(! bypassModeBtn.getToggleState());
    if(auto* c = getConstrainer())
        c->setFixedAspectRatio((double) baseW / (double) baseH);
    const float z = juce::jlimit(minZoom, maxZoom,
                                  (float) (double) proc.apvts.state.getProperty("uiZoom", 1.0));
    uiZoom = z;
    lastSavedZoom = z;
    setSize(juce::roundToInt(baseW * z), juce::roundToInt(baseH * z));
    setResizeLimits(juce::roundToInt(baseW * minZoom), juce::roundToInt(baseH * minZoom),
                     juce::roundToInt(baseW * maxZoom), juce::roundToInt(baseH * maxZoom));
    if((bool) proc.apvts.state.getProperty("uiSecondaryOpen", false))
    {
        flyoutA.show();
        menuButtonA.setActive(true);
    }
    sizeRestoreDone = true;
}
NewProjectAudioProcessorEditor::~NewProjectAudioProcessorEditor()
{
    stopTimer();
    removeMouseListener(&mouseSpy);
    setLookAndFeel(nullptr);
}
void NewProjectAudioProcessorEditor::applyModeAlphas()
{
    const float pianoA = juce::jlimit(0.0f, 1.0f, 1.0f - modePhase);
    const float scA = juce::jlimit(0.0f, 1.0f, modePhase - 1.0f);
    const bool scActive = (modeTarget >= 1.0f);
    const bool pianoActive = ! scActive;
    auto applyGroup = [] (juce::Component* c, float a, bool active)
    {
        c->setAlpha(a);
        c->setVisible(a > 0.001f);
        c->setInterceptsMouseClicks(active, active);
        c->setWantsKeyboardFocus(active);
        if(! active && c->hasKeyboardFocus(true))
            c->giveAwayKeyboardFocus();
    };
    for(juce::Component* c : { (juce::Component*) &pianoRoll,
                                (juce::Component*) &fineTune,
                                (juce::Component*) &fineTuneLbl,
                                (juce::Component*) &scalePanel })
        applyGroup(c, pianoA, pianoActive);
    for(juce::Component* c : { (juce::Component*) &scSpectrum,
                                (juce::Component*) &scTranspose,
                                (juce::Component*) &scTransposeLbl })
        applyGroup(c, scA, scActive);
}
void NewProjectAudioProcessorEditor::timerCallback()
{
    const float speed = 2.0f / 10.0f;
    if(modePhase < modeTarget) modePhase = juce::jmin(modeTarget, modePhase + speed);
    else modePhase = juce::jmax(modeTarget, modePhase - speed);
    applyModeAlphas();
    if(std::abs(modePhase - modeTarget) < 1.0e-4f)
    {
        modePhase = modeTarget;
        applyModeAlphas();
        stopTimer();
    }
}
void NewProjectAudioProcessorEditor::setupKnob(ValueKnob& k, const juce::String& id,
                                                std::unique_ptr<SA>& att)
{
    content.addAndMakeVisible(k);
    att = std::make_unique<SA> (proc.apvts, id, k);
    if(auto* prm = proc.apvts.getParameter(id))
        k.setDoubleClickReturnValue(true, prm->convertFrom0to1 (prm->getDefaultValue()));
}
void NewProjectAudioProcessorEditor::wireReadout(ValueKnob& k, juce::Label& lbl)
{
    k.onDragStart = [this, &k, &lbl] { showReadout(&k, &lbl, ReadoutMode::drag); };
    k.onValueChange = [this, &k]
    {
        if(readoutKnob == &k)
        {
            updateReadout();
            return;
        }
        if(hoveredKnob == &k)
            if(auto* lbl = findReadoutLabelForKnob(&k))
                showReadout(&k, lbl, ReadoutMode::hover);
    };
    k.onDragEnd = [this, &k]
    {
        if(hoveredKnob == &k)
            if(auto* lbl = findReadoutLabelForKnob(&k))
            {
                showReadout(&k, lbl, ReadoutMode::hover);
                return;
            }
        hideReadout();
    };
}
void NewProjectAudioProcessorEditor::showReadout(ValueKnob* k, juce::Label* lbl, ReadoutMode mode)
{
    readoutKnob = k;
    readoutLabel = lbl;
    readoutMode = mode;
    ++readoutTimerToken;
    if(auto* par = lbl->getParentComponent())
        par->addChildComponent(valueReadout);
    updateReadout();
    if(mode == ReadoutMode::hover)
        valueReadout.fadeIn();
    else
        valueReadout.popIn();
    valueReadout.toFront(false);
}
void NewProjectAudioProcessorEditor::scheduleHoverReadoutShow(ValueKnob* knob)
{
    const int token = ++readoutTimerToken;
    juce::Timer::callAfterDelay(250, [safe = juce::Component::SafePointer<NewProjectAudioProcessorEditor> (this), token, knob]
    {
        if(safe == nullptr)
            return;
        if(safe->readoutTimerToken != token)
            return;
        if(safe->hoveredKnob != knob)
            return;
        if(auto* lbl = safe->findReadoutLabelForKnob(knob))
            safe->showReadout(knob, lbl, ReadoutMode::hover);
    });
}
void NewProjectAudioProcessorEditor::updateReadout()
{
    if(readoutKnob == nullptr || readoutLabel == nullptr) return;
    valueReadout.setText(readoutKnob->getTextFromValue(readoutKnob->getValue()));
    const auto font = readoutLabel->getFont();
    const float nameW = juce::GlyphArrangement::getStringWidth(font, readoutLabel->getText());
    const int textR = readoutLabel->getX()
                       + juce::roundToInt((readoutLabel->getWidth() + nameW) * 0.5f);
    const auto chipFont = KnobLookAndFeel::courier(KnobLookAndFeel::uiFont);
    const int w = juce::roundToInt(juce::GlyphArrangement::getStringWidth(chipFont, valueReadout.getText())) + 14;
    const int h = readoutLabel->getHeight();
    int x = textR + 6;
    const int y = readoutLabel->getY();
    const int parentW = readoutLabel->getParentComponent() != nullptr
                          ? readoutLabel->getParentComponent()->getWidth() : baseW;
    if(x + w > parentW - 2) x = parentW - 2 - w;
    x = juce::jmax(2, x);
    valueReadout.setBounds(x, y, w, h);
}
void NewProjectAudioProcessorEditor::hideReadout()
{
    readoutKnob = nullptr;
    readoutLabel = nullptr;
    readoutMode = ReadoutMode::none;
    ++readoutTimerToken;
    valueReadout.fadeOut();
}
static juce::Image maxBlendToBg(const juce::Image& src, int w, int h, juce::Colour bg)
{
    if(! src.isValid() || w <= 0 || h <= 0)
        return {};
    const float srcAspect = (float) src.getWidth() / (float) src.getHeight();
    const float boxAspect = (float) w / (float) h;
    int fw = w, fh = h;
    if(srcAspect > boxAspect) { fw = w; fh = juce::jmax(1, juce::roundToInt((float) w / srcAspect)); }
    else { fh = h; fw = juce::jmax(1, juce::roundToInt((float) h * srcAspect)); }
    const int ox = (w - fw) / 2, oy = (h - fh) / 2;
    auto scaled = src.rescaled(fw, fh, juce::Graphics::highResamplingQuality);
    juce::Image out(juce::Image::RGB, w, h, false);
    const int br = bg.getRed(), bgc = bg.getGreen(), bb = bg.getBlue();
    juce::Image::BitmapData s(scaled, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData d(out, juce::Image::BitmapData::writeOnly);
    for(int y = 0; y < h; ++y)
        for(int x = 0; x < w; ++x)
        {
            const int sx = x - ox, sy = y - oy;
            if(sx >= 0 && sy >= 0 && sx < fw && sy < fh)
            {
                const auto p = s.getPixelColour(sx, sy);
                d.setPixelColour(x, y, juce::Colour(
                    (juce::uint8) juce::jmax(br, (int) p.getRed()),
                    (juce::uint8) juce::jmax(bgc, (int) p.getGreen()),
                    (juce::uint8) juce::jmax(bb, (int) p.getBlue())));
            }
            else
            {
                d.setPixelColour(x, y, bg);
            }
        }
    return out;
}
void NewProjectAudioProcessorEditor::rebuildLogo()
{
    const int s = juce::jmax(1, juce::roundToInt(std::ceil(maxZoom)));
    logoBlended = maxBlendToBg(logoSource, logoArea.getWidth() * s, logoArea.getHeight() * s,
                                juce::Colour(0xff121216));
}
void NewProjectAudioProcessorEditor::styleLabel(juce::Label& l, const juce::String& text)
{
    l.setText(text, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    l.setColour(juce::Label::textColourId, juce::Colour(0xffb0b0b8));
    l.setFont(KnobLookAndFeel::courier(KnobLookAndFeel::uiFont));
    l.setInterceptsMouseClicks(false, false);
    content.addAndMakeVisible(l);
}
void NewProjectAudioProcessorEditor::MouseSpy::mouseDown(const juce::MouseEvent& e)
{
    owner.handleGlobalMouseDown(e);
}
void NewProjectAudioProcessorEditor::MouseSpy::mouseMove(const juce::MouseEvent& e)
{
    owner.handleGlobalMouseMove(e);
}
juce::Label* NewProjectAudioProcessorEditor::findReadoutLabelForKnob(ValueKnob* knob)
{
    if(knob == nullptr) return nullptr;
    if(knob == &transient) return &transientLbl;
    if(knob == &transpose) return &transposeLbl;
    if(knob == &scTranspose) return &scTransposeLbl;
    if(knob == &attraction) return &attractionLbl;
    if(knob == &emphasis) return &emphasisLbl;
    if(knob == &morph) return &morphLbl;
    if(knob == &density) return &densityLbl;
    if(knob == &feedback) return &feedbackLbl;
    if(knob == &fineTune) return &fineTuneLbl;
    if(knob == &envComp) return &envCompLbl;
    if(knob == &formant) return &formantLbl;
    return nullptr;
}
void NewProjectAudioProcessorEditor::handleGlobalMouseMove(const juce::MouseEvent& e)
{
    juce::String hintText;
    if(flyoutA.isOpen() && flyoutA.hitTestPin(e.getEventRelativeTo(&flyoutA).position))
        hintText = "Pin\n\nPrevents additional options from hiding when clicking outside.";
    if(hintText.isEmpty())
        hintText = findHoverHintTextForComponent(e.eventComponent);
    if(hintText.isEmpty())
    {
        const auto p = e.getEventRelativeTo(&content).position.toInt();
        if(logoArea.contains(p))
            hintText = "-\n\nVersion: 1.1.2";
    }
    hoverHintBox.setHintText(hintText);
    ValueKnob* knob = nullptr;
    for(auto* c = e.eventComponent; c != nullptr; c = c->getParentComponent())
    {
        if(auto* vk = dynamic_cast<ValueKnob*> (c))
        {
            knob = vk;
            break;
        }
        if(c == this)
            break;
    }
    if(knob == hoveredKnob)
        return;
    hoveredKnob = knob;
    hideReadout();
    if(knob != nullptr)
        scheduleHoverReadoutShow(knob);
}
juce::String NewProjectAudioProcessorEditor::findHoverHintTextForComponent(juce::Component* eventComponent) const
{
    auto isInsideScalePanel = [this] (juce::Component* c)
    {
        for(auto* p = c; p != nullptr; p = p->getParentComponent())
        {
            if(p == &scalePanel)
                return true;
            if(p == this)
                break;
        }
        return false;
    };
    for(auto* c = eventComponent; c != nullptr; c = c->getParentComponent())
    {
        if(isInsideScalePanel(c))
        {
            if(dynamic_cast<KeyButton*> (c) != nullptr)
                return "Scale Key\n\nSelect the root key for the scales.\nChanges automatically shift the scale while a preset is active.";
            if(auto* btn = dynamic_cast<juce::Button*> (c))
            {
                const auto t = btn->getButtonText().trim().toUpperCase();
                if(t == "FULL" || t == "CLEAR")
                    return "Scale Full/Clear\n\nQuickly selects or removes all notes.";
                return "Scale Selector\n\nSelect any scale or chord preset from the drop-down menu to the current set key.\nAny modification to the Piano Roll disables the current chosen preset.";
            }
            break;
        }
        if(c == &menuButtonA) return "More Options\n\nClick here to show or hide additional options.";
        if(c == &bypassModeBtn) return "Active/Bypass\n\nBypass disables all Phase-Vocoder processing, passing the dry signal through for comparison.";
        if(c == &fftSizeBox) return "FFT Size\n\nLarger sizes give higher frequency resolution and better low-end accuracy but lower time resolution and more latency.";
        if(c == &overlapBox) return "FFT Overlap\n\nHigher overlap improves time resolution and reduces artefacts at the cost of more CPU.";
        if(c == &modeBox) return "Targets\n\nSelect source type for attraction targets.\n\nScale: Selected notes.\nMIDI: MIDI input.\nSidechain: Sidechain input.";
        if(c == &fullMidiBtn) return "Full MIDI\n\nWhen enabled, each MIDI note targets only the exact frequency played instead of the same note in every octave.";
        if(c == &moreBasesBtn) return "More Bases\n\nDetects and accepts more base frequencies in the candidate range.";
        if(c == &hysteresisBtn) return "Hysteresis\n\nStabilises base detection over time. \nBases with high confidence persist through brief dropouts and resist flicker between nearby target pitches.";
        if(c == &midSideBtn) return "Stereo\n\nSelect the stereo processing domain.\n\nLeft - Right: \nprocess channels independently.\nMid - Side: \nprocess Mid and Side components.";
        if(c == &enhanceTransientBtn) return "Enhance Transient\n\nClick to switch between basic and enhanced transient detection.";
        if(c == &transient) return "Transient\n\nBypasses processing on detected attacks.\nHigher values preserve more transients.";
        if(c == &transpose) return "Transpose (Main)\n\nRepitches input audio before analysis and processing.";
        if(c == &formant) return "Formant\n\nShifts the spectral envelope up or down to the transposed audio.\nTends to make voices thinner or thicker.";
        if(c == &scTranspose) return "Transpose (Sidechain)\n\nRepitches sidechain audio before analysis.";
        if(c == &attraction) return "Attraction\n\nAmount each detected base frequency and its harmonics are shifted towards its nearest target pitch.";
        if(c == &emphasis) return "Emphasis\n\nHigher values move inharmonics closer to the nearest integer harmonic of a base frequency.";
        if(c == &morph) return "Morph\n\nBlends synthesised phases toward the detected partial structure.\nHigher values lock harmonics together for a more coherent tone.";
        if(c == &density) return "Density\n\nHigher values accept more weak candidates as detected base frequencies.";
        if(c == &feedback) return "Feedback\n\nBoosts retuned base frequencies and their harmonics shifted near targets.";
        if(c == &fineTune) return "Fine Tune\n\nAmount in cents to shift the target notes by.";
        if(c == &envComp) return "Compensation\n\nFlattens synthesised spectrum to correct energy build-up when nearby frequencies accumulate during Phase-Vocoder synthesis.";
        if(c == &pianoRoll) return "Piano Roll\n\nScale: \nClick notes here to modify the target scale.\n\nMIDI: \nDisplays the current MIDI input.";
        if(c == &spectrum || c == &spectrumCoordOverlay) return "Spectrum (Main)\n\nDisplays Phase-Vocoder frequencies and detected bases.\nDrag your mouse on the spectrum to change detection range.\n\nLeft-Right: Centre\nUp-Down: Spread";
        if(c == &scSpectrum || c == &scSpectrumCoordOverlay) return "Spectrum (Sidechain)\n\nDisplays Phase-Vocoder frequencies and detected bases for sidechain input.\n\nSee Main Spectrum for more detail.";
        if(c == &alletisLink) return "-\n\nSupport and receive newest updates on alletis.com";
        if(c == this)
            break;
    }
    return {};
}
void NewProjectAudioProcessorEditor::handleGlobalMouseDown(const juce::MouseEvent& e)
{
    bool onSlider = false, insideFlyout = false, onButton = false, inLogoFlyout = false;
    for(auto* c = e.eventComponent; c != nullptr; c = c->getParentComponent())
    {
        if(dynamic_cast<juce::Slider*> (c) != nullptr) onSlider = true;
        if(c == &flyoutA) insideFlyout = true;
        if(c == &menuButtonA) onButton = true;
        if(c == &logoFlyout) inLogoFlyout = true;
        if(c == this) break;
    }
    if(! logoFlyout.isOpen() && ! inLogoFlyout
        && logoArea.contains(e.getEventRelativeTo(&content).position.toInt()))
    {
        logoFlyout.show();
        return;
    }
    if(flyoutA.isOpen() && ! insideFlyout && ! onButton && ! flyoutA.isPinned())
        flyoutA.close();
    if(! onSlider)
        if(auto* f = juce::Component::getCurrentlyFocusedComponent())
            for(auto* c = f; c != nullptr; c = c->getParentComponent())
                if(dynamic_cast<juce::Slider*> (c) != nullptr) { grabKeyboardFocus(); break; }
}
void NewProjectAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff121216));
}
void NewProjectAudioProcessorEditor::ContentComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff121216));
    g.setColour(juce::Colour(0xff2a2a30));
    g.drawVerticalLine(owner.dividerX, 10.0f, (float) baseH - 10.0f);
    if(owner.logoBlended.isValid())
        g.drawImage(owner.logoBlended, owner.logoArea.toFloat());
}
void NewProjectAudioProcessorEditor::parentHierarchyChanged()
{
   #if JUCE_WINDOWS
    if(auto* peer = getPeer())
    {
        const auto engines = peer->getAvailableRenderingEngines();
        for(int i = 0; i < engines.size(); ++i)
            if(engines[i].containsIgnoreCase("software"))
            {
                if(peer->getCurrentRenderingEngine() != i)
                    peer->setCurrentRenderingEngine(i);
                break;
            }
    }
   #endif
}
void NewProjectAudioProcessorEditor::resized()
{
    const float sx = (float) getWidth() / (float) baseW;
    const float sy = (float) getHeight() / (float) baseH;
    uiZoom = juce::jlimit(minZoom, maxZoom, sx);
    content.setTransform(juce::AffineTransform::scale(sx, sy));
    content.setBounds(0, 0, baseW + 2, baseH + 2);
    logoFlyout.setTransform(juce::AffineTransform::scale(sx, sy));
    logoFlyout.setBounds(0, 0, baseW + 2, baseH + 2);
    const auto logoPanel = juce::Rectangle<int> (460, 300).withCentre({ baseW / 2, baseH / 2 });
    logoFlyout.setPanelRect(logoPanel);
    if(logoSource.isValid())
    {
        const float lAspect = (float) logoSource.getWidth() / (float) logoSource.getHeight();
        const int lw = juce::roundToInt((logoPanel.getWidth() - 56) * 0.7f);
        const int lh = juce::roundToInt((float) lw / lAspect);
        auto lr = juce::Rectangle<int> (lw, lh).withCentre({ logoPanel.getCentreX(),
                                                              logoPanel.getY() + 18 + lh / 2 });
        const int bs = juce::jmax(1, juce::roundToInt(std::ceil(maxZoom)));
        logoFlyout.setLogo(maxBlendToBg(logoSource, lw * bs, lh * bs, juce::Colour(0xff17171c)), lr);
        const int ih = 22;
        const auto pbg = juce::Colour(0xff17171c);
        juce::Image eyeB, b2B;
        if(eyeSource.isValid())
        {
            const int w = juce::roundToInt(ih * (float) eyeSource.getWidth() / (float) eyeSource.getHeight());
            eyeB = maxBlendToBg(eyeSource, w * bs, ih * bs, pbg);
        }
        if(brand2Source.isValid())
        {
            const int w = juce::roundToInt(ih * (float) brand2Source.getWidth() / (float) brand2Source.getHeight());
            b2B = maxBlendToBg(brand2Source, w * bs, ih * bs, pbg);
        }
        logoFlyout.setPageLogos(eyeB, b2B);
    }
    const int pad = 10;
    const int labelH = 16;
    auto setArcSensitivity = [] (ValueKnob& k, int d)
    {
        const auto rp = k.getRotaryParameters();
        const float total = juce::jmax(rp.startAngleRadians, rp.endAngleRadians)
                          - juce::jmin(rp.startAngleRadians, rp.endAngleRadians);
        k.setMouseDragSensitivity(juce::jmax(1, juce::roundToInt((float) d * 0.5f * total)));
    };
    auto area = juce::Rectangle<int> (0, 0, baseW, baseH).reduced(pad);
    const float tH = 32.0f;
    const float tBox = juce::jlimit(14.0f, tH - 2.0f, tH * 0.58f);
    const float refW = juce::GlyphArrangement::getStringWidth(KnobLookAndFeel::courier(KnobLookAndFeel::uiFont), "FFT SIZE");
    const int leftW = juce::roundToInt((tBox + 12.0f + refW) * 1.6f);
    auto left = area.removeFromLeft(leftW);
    const auto leftColumn = left;
    area.removeFromLeft(pad);
    auto right = area;
    dividerX = left.getRight() + pad / 2;
    bypassMask.setBounds(dividerX, 0, baseW - dividerX, baseH);
    const int leftButtonsCenterX = leftColumn.getX() + (leftColumn.getWidth() - pad) / 2;
    const int logoW = left.getWidth() - pad;
    const int logoH = juce::roundToInt((float) logoW * (101.0f / 320.0f));
    logoArea = left.removeFromTop(logoH).withWidth(logoW);
    left.removeFromTop(12);
    rebuildLogo();
    bypassModeBtn.setBounds(left.removeFromTop(26).withTrimmedRight(pad));
    left.removeFromTop(18);
    auto fftLabelRow = left.removeFromTop(labelH);
    auto fftRow = left.removeFromTop(26).withTrimmedRight(pad);
    fftSizeBox.setBounds(fftRow);
    fftLbl.setBounds(fftRow.withY(fftLabelRow.getY()).withHeight(labelH));
    left.removeFromTop(18);
    auto targetLabelRow = left.removeFromTop(labelH);
    auto targetRow = left.removeFromTop(26).withTrimmedRight(pad);
    modeBox.setBounds(targetRow);
    targetLbl.setBounds(targetRow.withY(targetLabelRow.getY()).withHeight(labelH));
    left.removeFromTop(18);
    auto stereoLabelRow = left.removeFromTop(labelH);
    auto stereoRow = left.removeFromTop(26).withTrimmedRight(pad);
    midSideBtn.setBounds(stereoRow);
    stereoLbl.setBounds(stereoRow.withY(stereoLabelRow.getY()).withHeight(labelH));
    if(alletisSource.isValid())
    {
        const int aw = leftColumn.getWidth() / 2;
        const float aspect = (float) alletisSource.getWidth() / (float) alletisSource.getHeight();
        const int ah = juce::roundToInt((float) aw / aspect);
        juce::Rectangle<int> ab(aw, ah);
        ab.setCentre(leftButtonsCenterX, 0);
        ab.setY(leftColumn.getBottom() - ah);
        alletisLink.setBounds(ab);
        const int bs = juce::jmax(1, juce::roundToInt(std::ceil(maxZoom)));
        alletisLink.setImage(maxBlendToBg(alletisSource, aw * bs, ah * bs, juce::Colour(0xff121216)));
        const int hintH = hoverHintBox.getPreferredHeight();
        juce::Rectangle<int> hb(leftColumn.getX(), 0, leftColumn.getWidth() - pad, hintH);
        hb.setY(ab.getY() - 8 - hintH);
        hoverHintBox.setBounds(hb);
    }
    const int H = right.getHeight();
    const int colW = right.getWidth() / 5;
    const int attrD = juce::jlimit(60, 156, colW);
    const int satD = juce::jlimit(36, 240, juce::roundToInt(attrD * 0.62f));
    const int transH = labelH + 28;
    const int rowH = labelH + attrD;
    const int botH = juce::jlimit(110, 200, juce::roundToInt(H * 0.20f));
    auto bottomBand = right.removeFromBottom(botH);
    right.removeFromBottom(pad);
    auto clusterBand = right.removeFromBottom(rowH);
    right.removeFromBottom(pad);
    auto transposeBand = right.removeFromBottom(transH);
    right.removeFromBottom(pad);
    auto spectrumBand = right;
    spectrum.setBounds(spectrumBand);
    {
        const int gap = 4;
        const int ovH = 22;
        spectrumCoordOverlay.setBounds(spectrumBand.getX(), spectrumBand.getBottom() + gap,
                                        spectrumBand.getWidth(), ovH);
        spectrumCoordOverlay.toFront(false);
    }
    transposeLbl.setBounds(transposeBand.removeFromBottom(labelH));
    transpose.setBounds(transposeBand.removeFromTop(juce::jmin(28, transposeBand.getHeight())));
    transpose.setMouseDragSensitivity(juce::jmax(1, transpose.getWidth()));
    const auto bottomFull = bottomBand;
    {
        auto bb = bottomFull;
        const int ftCellW = juce::jmax(satD + 2 * pad, bb.getWidth() / 6);
        auto ft = bb.removeFromLeft(ftCellW);
        fineTuneLbl.setBounds(ft.removeFromBottom(labelH));
        const int ftW = juce::jmin(28, ft.getWidth());
        fineTune.setBounds(ft.withSizeKeepingCentre(ftW, ft.getHeight()));
        fineTune.setMouseDragSensitivity(juce::jmax(1, fineTune.getHeight()));
        bb.removeFromLeft(pad);
        auto scaleArea = bb.removeFromRight(150);
        bb.removeFromRight(pad);
        scalePanel.setBounds(scaleArea);
        pianoRoll.setBounds(bb);
    }
    {
        auto sb = bottomFull;
        const int sliderRowH = labelH + 28;
        auto sliderRow = sb.removeFromBottom(sliderRowH);
        scSpectrum.setBounds(sb);
        const int ovGap = 4, ovH = 22;
        scSpectrumCoordOverlay.setBounds(sb.getX(), sb.getBottom() + ovGap, sb.getWidth(), ovH);
        scSpectrumCoordOverlay.toFront(false);
        scTransposeLbl.setBounds(sliderRow.removeFromBottom(labelH));
        scTranspose.setBounds(sliderRow.removeFromTop(juce::jmin(28, sliderRow.getHeight())));
        scTranspose.setMouseDragSensitivity(juce::jmax(1, scTranspose.getWidth()));
    }
    {
        const int bandBottom = clusterBand.getBottom();
        const int labelY = bandBottom - labelH;
        ValueKnob* kn[3] = { &emphasis, &attraction, &density };
        juce::Label* lb[3] = { &emphasisLbl, &attractionLbl, &densityLbl };
        const int cxMid = clusterBand.getX() + colW * 2 + colW / 2;
        const int spacing = juce::roundToInt((float) colW * 1.28f);
        const int cxs[3] = { cxMid - spacing, cxMid, cxMid + spacing };
        for(int j = 0; j < 3; ++j)
        {
            const int d = (j == 1) ? attrD : satD;
            const int cx = cxs[j];
            kn[j]->setBounds(cx - d / 2, labelY - d, d, d);
            setArcSensitivity(*kn[j], d);
            lb[j]->setBounds(cx - colW / 2, labelY, colW, labelH);
        }
        const int btnSide = juce::jlimit(32, 56, juce::roundToInt((float) satD * 0.58f));
        menuButtonA.setBounds(scalePanel.getRight() - btnSide, bandBottom - btnSide, btnSide, btnSide);
        const int dMenu = juce::jlimit(40, 110, satD);
        const int gMenu = juce::jmax(34, juce::roundToInt((float) dMenu * 0.42f));
        const int mPadX = 44;
        const int mPadY = 16;
        const int comboH = 26;
        const int togH = 30;
        const int rowGap = 6;
        const int topRowH = dMenu + 2 + labelH;
        const int compH = labelH + 24;
        const int ovH = labelH + comboH;
        const int flyW = mPadX * 2 + dMenu * 3 + gMenu * 2;
        const int flyH = mPadY * 2 + topRowH + 10 + compH + 10 + compH + rowGap + ovH + 4 + togH;
        flyoutA.setBounds(scalePanel.getRight() - flyW, (bandBottom - btnSide) - 8 - flyH, flyW, flyH);
        flyoutA.prewarmBackdrop();
        auto inner = juce::Rectangle<int> (0, 0, flyW, flyH).reduced(mPadX, mPadY);
        auto knobRow = inner.removeFromTop(topRowH);
        const int colMenu = (knobRow.getWidth() - gMenu * 2) / 3;
        auto placeMenuKnob = [&] (ValueKnob& k, juce::Label& lbl, juce::Rectangle<int> col)
        {
            lbl.setBounds(col.removeFromBottom(labelH));
            col.removeFromBottom(2);
            const int s = juce::jmin(col.getWidth(), col.getHeight());
            k.setBounds(col.getCentreX() - s / 2, col.getBottom() - s, s, s);
            setArcSensitivity(k, s);
        };
        auto colT = knobRow.removeFromLeft(colMenu);
        knobRow.removeFromLeft(gMenu);
        auto colM = knobRow.removeFromLeft(colMenu);
        knobRow.removeFromLeft(gMenu);
        auto colF = knobRow;
        placeMenuKnob(transient, transientLbl, colT);
        placeMenuKnob(morph, morphLbl, colM);
        placeMenuKnob(feedback, feedbackLbl, colF);
        {
            const auto tb = transient.getBounds();
            const int td = juce::jlimit(11, 18, juce::roundToInt((float) tb.getWidth() * 0.18f));
            enhanceTransientBtn.setBounds(tb.getX() + 2 - td, tb.getBottom() - td - 10, td, td);
        }
        const int halfPad = mPadX / 2;
        inner = inner.expanded(mPadX - halfPad, 0);
        inner.removeFromTop(5);
        formantLbl.setBounds(inner.removeFromTop(labelH));
        formant.setBounds(inner.removeFromTop(24));
        formant.setMouseDragSensitivity(juce::jmax(1, formant.getWidth()));
        inner.removeFromTop(5);
        envCompLbl.setBounds(inner.removeFromTop(labelH));
        envComp.setBounds(inner.removeFromTop(24));
        envComp.setMouseDragSensitivity(juce::jmax(1, envComp.getWidth()));
        inner.removeFromTop(rowGap);
        auto ov = inner.removeFromTop(ovH);
        overlapLbl.setBounds(ov.removeFromTop(labelH));
        overlapBox.setBounds(ov.reduced(4, 0));
        inner.removeFromTop(10);
        auto togglesRow = inner.removeFromTop(togH);
        const int rowX = togglesRow.getX();
        const int rowW = togglesRow.getWidth();
        const int toggleColW = rowW / 3;
        const int tGap = 6;
        const int tY = togglesRow.getY();
        const int toggleH = togglesRow.getHeight();
        fullMidiBtn.setBounds(rowX, tY, toggleColW - tGap, toggleH);
        moreBasesBtn.setBounds(rowX + toggleColW, tY, toggleColW - tGap, toggleH);
        hysteresisBtn.setBounds(rowX + toggleColW * 2, tY, rowW - toggleColW * 2, toggleH);
    }
    repaint();
    if(sizeRestoreDone && std::abs(uiZoom - lastSavedZoom) > 0.001f)
    {
        lastSavedZoom = uiZoom;
        proc.apvts.state.setProperty("uiZoom", (double) uiZoom, nullptr);
    }
}