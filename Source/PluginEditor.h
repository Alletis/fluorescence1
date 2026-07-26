#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "KnobLookAndFeel.h"
#include "ValueKnob.h"
#include "SmoothToggle.h"
#include "SmoothComboBox.h"
#include "PianoRoll.h"
#include "ScalePanel.h"
#include "SpectrumDisplay.h"
#include "SecondaryMenu.h"
#include "SmoothColour.h"
#include <cmath>
#include <functional>
class ValueReadout : public juce::Component
                  , private juce::Timer
{
public:
    ValueReadout() { setInterceptsMouseClicks(false, false); setVisible(false); }
    void setText(const juce::String& t) { text = t; repaint(); }
    const juce::String& getText() const noexcept { return text; }
    void popIn()
    {
        alpha = 1.0f;
        targetAlpha = 1.0f;
        setVisible(true);
        stopTimer();
        repaint();
    }
    void fadeIn()
    {
        targetAlpha = 1.0f;
        setVisible(true);
        startTimerHz(60);
    }
    void fadeOut()
    {
        targetAlpha = 0.0f;
        startTimerHz(60);
    }
    void paint(juce::Graphics& g) override
    {
        if(alpha <= 0.001f)
            return;
        auto b = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(juce::Colour(0xff15151a).withAlpha(alpha));
        g.fillRoundedRectangle(b, 3.0f);
        g.setColour(juce::Colour(0xff5a5a64).withAlpha(0.65f * alpha));
        g.drawRoundedRectangle(b, 3.0f, 1.0f);
        g.setColour(juce::Colour(0xffe8e8ee).withAlpha(alpha));
        g.setFont(KnobLookAndFeel::courier(KnobLookAndFeel::uiFont));
        g.drawText(text, getLocalBounds(), juce::Justification::centred);
    }
private:
    void timerCallback() override
    {
        alpha += (targetAlpha - alpha) * (1.0f / 3.0f);
        alpha = juce::jlimit(0.0f, 1.0f, alpha);
        if(std::abs(targetAlpha - alpha) < 0.01f)
        {
            alpha = targetAlpha;
            if(alpha <= 0.001f)
                setVisible(false);
            stopTimer();
        }
        repaint();
    }
    juce::String text;
    float alpha = 0.0f;
    float targetAlpha = 0.0f;
};
class HoverHintBox : public juce::Component,
                     private juce::Timer
{
public:
    HoverHintBox()
    {
        startTimerHz(60);
    }
    ~HoverHintBox() override { stopTimer(); }
    void setHintText(juce::String t)
    {
        auto next = t.trim();
        next = next.replace("\\n", "\n");
        const auto fallback = juce::String("-\n\nHover your mouse over any interface element for a description.");
        const auto wanted = next.isNotEmpty() ? next : fallback;
        if(wanted == hintText)
            return;
        hintText = wanted;
        repaint();
    }
    void setOnHeightChanged(std::function<void()> cb) { onHeightChanged = std::move(cb); }
    int getPreferredHeight() const noexcept { return juce::roundToInt(currentHeight); }
    std::function<void()> onCollapsedChanged;
    bool isCollapsed() const noexcept { return collapsed; }
    void setCollapsed(bool c) { collapsed = c; }
    void toggleCollapsed()
    {
        collapsed = ! collapsed;
        if(onCollapsedChanged) onCollapsedChanged();
    }
    void paint(juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(bg.get());
        g.fillRoundedRectangle(r, 6.0f);
        g.setColour(border.get());
        g.drawRoundedRectangle(r, 6.0f, 1.0f);
        const float handleH = 16.0f;
        const auto body = r.withTrimmedBottom(handleH + 2.0f).reduced(8.0f, 6.0f);
        if(body.getHeight() > 2.0f)
        {
            g.setColour(textCol.get().withAlpha(bodyAlpha));
            g.setFont(KnobLookAndFeel::courier(KnobLookAndFeel::uiFont * 0.7f));
            g.drawFittedText(hintText, body.toNearestInt().reduced(0, 1), juce::Justification::topLeft, 3);
        }
        const auto handle = getHandleRect(r);
        g.setColour(handleCol.get());
        g.fillRoundedRectangle(handle, 3.0f);
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        const bool wasPressedInZone = toggleDown;
        toggleDown = false;
        const auto r = getLocalBounds().toFloat().reduced(0.5f);
        const bool inZone = getToggleHitRect(r).contains(e.position);
        toggleHot = inZone;
        if(wasPressedInZone && inZone)
            toggleCollapsed();
        repaint();
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        const auto r = getLocalBounds().toFloat().reduced(0.5f);
        const bool inZone = getToggleHitRect(r).contains(e.position);
        toggleHot = inZone;
        toggleDown = inZone;
        repaint();
    }
    void mouseMove(const juce::MouseEvent& e) override
    {
        const auto r = getLocalBounds().toFloat().reduced(0.5f);
        const bool inZone = getToggleHitRect(r).contains(e.position);
        if(inZone != toggleHot)
        {
            toggleHot = inZone;
            repaint();
        }
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        const auto r = getLocalBounds().toFloat().reduced(0.5f);
        const bool inZone = getToggleHitRect(r).contains(e.position);
        const bool newDown = e.mods.isLeftButtonDown() && inZone;
        bool changed = false;
        if(inZone != toggleHot) { toggleHot = inZone; changed = true; }
        if(newDown != toggleDown) { toggleDown = newDown; changed = true; }
        if(changed) repaint();
    }
    void mouseExit(const juce::MouseEvent&) override
    {
        if(toggleHot || toggleDown)
        {
            toggleHot = false;
            toggleDown = false;
            repaint();
        }
    }
private:
    juce::Rectangle<float> getHandleRect(juce::Rectangle<float> r) const
    {
        const float handleW = juce::jlimit(24.0f, 42.0f, r.getWidth() * 0.24f);
        const float collapsedBottomInset = collapsedHeight * 0.5f;
        const float expandAmt = juce::jlimit(0.0f, 1.0f,
                                              (r.getHeight() - collapsedHeight)
                                                / juce::jmax(1.0f, expandedHeight - collapsedHeight));
        const float expandedNudge = 1.0f * expandAmt;
        const float cy = r.getBottom() - juce::jmin(collapsedBottomInset, r.getHeight() * 0.5f) + expandedNudge;
        return juce::Rectangle<float> (handleW, 6.0f).withCentre({ r.getCentreX(), cy });
    }
    juce::Rectangle<float> getToggleHitRect(juce::Rectangle<float> r) const
    {
        const float toggleH = juce::jmin(collapsedHeight, r.getHeight());
        return r.withTrimmedTop(r.getHeight() - toggleH);
    }
    void timerCallback() override
    {
        const float targetPhase = collapsed ? 0.0f : 2.0f;
        const float phaseStep = 0.11f;
        const float heightPhaseSpan = 1.5f;
        const float textPhaseStart = heightPhaseSpan;
        const float textPhaseSpan = 2.0f - textPhaseStart;
        const auto bgT = juce::Colour(0xff17171c);
        const auto borderT = juce::Colour(0xff34343e);
        const auto textT = juce::Colour(0xffb0b0b8);
        auto handleT = collapsed ? juce::Colour(0xff8a8a92) : juce::Colour(0xff45aeb1);
        if(toggleHot)
            handleT = handleT.brighter(0.28f);
        if(toggleDown)
            handleT = handleT.darker(0.35f);
        if(! primed)
        {
            bg.set(bgT);
            border.set(borderT);
            textCol.set(textT);
            handleCol.set(handleT);
            animPhase = targetPhase;
            bodyAlpha = juce::jlimit(0.0f, 1.0f, (animPhase - textPhaseStart) / textPhaseSpan);
            const float initHeightParam = juce::jlimit(0.0f, 1.0f, animPhase / heightPhaseSpan);
            const float initEasedHeight = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::pi * initHeightParam);
            currentHeight = juce::jmap(initEasedHeight, collapsedHeight, expandedHeight);
            lastHeightPx = juce::roundToInt(currentHeight);
            primed = true;
            if(onHeightChanged)
                onHeightChanged();
            repaint();
            return;
        }
        bool moving = false;
        moving |= bg.approach(bgT, 1.0f / 3.0f);
        moving |= border.approach(borderT, 1.0f / 3.0f);
        moving |= textCol.approach(textT, 1.0f / 3.0f);
        moving |= handleCol.approach(handleT, 1.0f / 3.0f);
        const float prevPhase = animPhase;
        if(animPhase < targetPhase)
            animPhase = juce::jmin(targetPhase, animPhase + phaseStep);
        else if(animPhase > targetPhase)
            animPhase = juce::jmax(targetPhase, animPhase - phaseStep);
        if(std::abs(animPhase - prevPhase) > 0.0001f)
            moving = true;
        animPhase = juce::jlimit(0.0f, 2.0f, animPhase);
        const float prevHeight = currentHeight;
        const float heightParam = juce::jlimit(0.0f, 1.0f, animPhase / heightPhaseSpan);
        const float easedHeight = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::pi * heightParam);
        currentHeight = juce::jmap(easedHeight, collapsedHeight, expandedHeight);
        const float dh = currentHeight - prevHeight;
        if(std::abs(dh) > 0.05f)
            moving = true;
        const float textNorm = juce::jlimit(0.0f, 1.0f, (animPhase - textPhaseStart) / textPhaseSpan);
        if(std::abs(textNorm - bodyAlpha) > 0.003f)
            moving = true;
        bodyAlpha = textNorm;
        const int heightPx = juce::roundToInt(currentHeight);
        if(heightPx != lastHeightPx)
        {
            lastHeightPx = heightPx;
            if(onHeightChanged)
                onHeightChanged();
        }
        if(moving)
            repaint();
    }
    juce::String hintText { "-\n\nHover your mouse over any interface element for a description." };
    SmoothColour bg, border, textCol, handleCol;
    bool primed = false;
    bool collapsed = true;
    float animPhase = 0.0f;
    float bodyAlpha = 0.0f;
    float currentHeight = 22.0f;
    int lastHeightPx = 22;
    bool toggleHot = false;
    bool toggleDown = false;
    const float expandedHeight = 132.0f;
    const float collapsedHeight = 22.0f;
    std::function<void()> onHeightChanged;
};
class SpectrumCoordOverlay : public juce::Component,
                             private juce::Timer
{
public:
    SpectrumCoordOverlay() { setInterceptsMouseClicks(false, false); startTimerHz(60); }
    ~SpectrumCoordOverlay() override { stopTimer(); }
    void setSource(SpectrumDisplay* src) { source = src; }
    void paint(juce::Graphics& g) override
    {
        if(source == nullptr) return;
        const float a = source->coordOverlayGetAlpha();
        if(a <= 0.001f) return;
        const juce::String text = source->coordOverlayGetText();
        const juce::String maxText = "(20000.0 Hz, 5.00 spread)";
        const auto font = KnobLookAndFeel::courier(12.0f);
        g.setFont(font);
        const int padX = 8;
        const int boxH = getHeight();
        const int textW = juce::roundToInt(juce::GlyphArrangement::getStringWidth(font, text));
        const int maxTW = juce::roundToInt(juce::GlyphArrangement::getStringWidth(font, maxText));
        const int boxW = juce::jmax(maxTW, textW) + padX * 2;
        const int x = getWidth() - boxW - 6;
        auto b = juce::Rectangle<float> ((float) x, 0.0f, (float) boxW, (float) boxH);
        g.setColour(juce::Colour(0xff111116).withAlpha(0.82f * a));
        g.fillRoundedRectangle(b, 4.0f);
        g.setColour(juce::Colour(0xff5a5a64).withAlpha(0.65f * a));
        g.drawRoundedRectangle(b, 4.0f, 1.0f);
        g.setColour(juce::Colour(0xffe8e8ee).withAlpha(a));
        g.drawText(text, x + padX, 0, boxW - padX * 2, boxH, juce::Justification::centred, false);
    }
private:
    void timerCallback() override { repaint(); }
    SpectrumDisplay* source = nullptr;
};
class ImageLink : public juce::Component
{
public:
    ImageLink() { setMouseCursor(juce::MouseCursor::PointingHandCursor); }
    void setImage(juce::Image i) { img = std::move(i); repaint(); }
    void setURL(juce::String u) { url = std::move(u); }
    void paint(juce::Graphics& g) override
    {
        if(img.isValid())
            g.drawImage(img, getLocalBounds().toFloat());
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        if(url.isNotEmpty() && ! e.mouseWasDraggedSinceMouseDown())
            juce::URL(url).launchInDefaultBrowser();
    }
private:
    juce::Image img;
    juce::String url;
};
class NewProjectAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit NewProjectAudioProcessorEditor(NewProjectAudioProcessor&);
    ~NewProjectAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;
private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    void styleLabel(juce::Label&, const juce::String&);
    void setupKnob(ValueKnob&, const juce::String& paramId, std::unique_ptr<SA>&);
    void wireReadout(ValueKnob&, juce::Label&);
    enum class ReadoutMode { none, hover, drag };
    void showReadout(ValueKnob*, juce::Label*, ReadoutMode mode);
    void scheduleHoverReadoutShow(ValueKnob* knob);
    void updateReadout();
    void hideReadout();
    void rebuildLogo();
    void handleGlobalMouseDown(const juce::MouseEvent&);
    void handleGlobalMouseMove(const juce::MouseEvent&);
    juce::Label* findReadoutLabelForKnob(ValueKnob* knob);
    juce::String findHoverHintTextForComponent(juce::Component* eventComponent) const;
    NewProjectAudioProcessor& proc;
    KnobLookAndFeel knobLnf;
    struct ContentComponent : juce::Component
    {
        NewProjectAudioProcessorEditor& owner;
        explicit ContentComponent(NewProjectAudioProcessorEditor& o) : owner(o) {}
        void paint(juce::Graphics&) override;
    };
    ContentComponent content { *this };
    SmoothTextButton bypassModeBtn { "ACTIVE" };
    SmoothComboBox fftSizeBox, overlapBox;
    SmoothToggleButton fullMidiBtn { "FULL MIDI" };
    SmoothToggleButton moreBasesBtn { "MORE BASES" };
    SmoothToggleButton hysteresisBtn { "HYSTERESIS" };
    SmoothTextButton midSideBtn { "LEFT - RIGHT" };
    SmoothComboBox modeBox;
    ValueKnob envComp;
    ValueKnob formant;
    ValueKnob transient;
    ValueKnob transpose, attraction, emphasis, morph, density, feedback, fineTune, scTranspose;
    SpectrumDisplay spectrum;
    SpectrumDisplay scSpectrum;
    SpectrumCoordOverlay spectrumCoordOverlay;
    SpectrumCoordOverlay scSpectrumCoordOverlay;
    PianoRoll pianoRoll;
    ScalePanel scalePanel;
    MenuDotsButton menuButtonA;
    RoundToggle enhanceTransientBtn;
    FlyoutPanel flyoutA;
    struct MouseSpy : juce::MouseListener
    {
        NewProjectAudioProcessorEditor& owner;
        explicit MouseSpy(NewProjectAudioProcessorEditor& o) : owner(o) {}
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseMove(const juce::MouseEvent& e) override;
    };
    MouseSpy mouseSpy { *this };
    juce::Label fftLbl, overlapLbl, stereoLbl, formantLbl, transientLbl, transposeLbl,
                attractionLbl, emphasisLbl, morphLbl, densityLbl, feedbackLbl, fineTuneLbl, envCompLbl,
                scTransposeLbl, targetLbl;
    std::unique_ptr<CA> fftAtt, overlapAtt, modeAtt;
    std::unique_ptr<BA> bypassAtt, fullMidiAtt, moreBasesAtt, midSideAtt, hysteresisAtt;
    std::unique_ptr<BA> enhanceTransientAtt;
    std::unique_ptr<SA> transientAtt, transposeAtt, attractionAtt, emphasisAtt,
                        morphAtt, densityAtt, feedbackAtt, fineTuneAtt, envCompAtt, scTransposeAtt;
    std::unique_ptr<SA> formantAtt;
    ValueReadout valueReadout;
    ValueKnob* readoutKnob = nullptr;
    juce::Label* readoutLabel = nullptr;
    ValueKnob* hoveredKnob = nullptr;
    ReadoutMode readoutMode = ReadoutMode::none;
    int readoutTimerToken = 0;
    int dividerX = 0;
    static constexpr int baseW = 760;
    static constexpr int baseH = 540;
    static constexpr float minZoom = 0.75f;
    static constexpr float maxZoom = 2.5f;
    float uiZoom = 1.0f;
    float lastSavedZoom = -1.0f;
    bool sizeRestoreDone = false;
    void timerCallback() override;
    void applyModeAlphas();
    float modePhase = 0.0f;
    float modeTarget = 0.0f;
    juce::Image logoSource, logoBlended;
    juce::Rectangle<int> logoArea;
    juce::Image alletisSource;
    ImageLink alletisLink;
    juce::Image brand2Source;
    juce::Image eyeSource;
    struct DimMask : juce::Component,
                     private juce::Timer
    {
        DimMask() { setInterceptsMouseClicks(false, false); startTimerHz(60); }
        ~DimMask() override { stopTimer(); }
        void setBypassed(bool b) { target = b ? 1.0f : 0.0f; }
        void primeTo(bool b) { target = amount = b ? 1.0f : 0.0f; setVisible(amount > 0.001f); }
        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colours::black.withAlpha(0.55f * amount));
        }
    private:
        void timerCallback() override
        {
            if(std::abs(target - amount) < 0.0004f)
            {
                if(amount != target) { amount = target; setVisible(amount > 0.001f); repaint(); }
                if(isVisible()) toFront(false);
                return;
            }
            amount += (target - amount) / 4.0f;
            setVisible(true);
            toFront(false);
            repaint();
        }
        float amount = 0.0f, target = 0.0f;
    };
    DimMask bypassMask;
    struct LogoFlyout : juce::Component,
                        private juce::Timer
    {
        std::function<void()> onClose;
        LogoFlyout()
        {
            setVisible(false);
            setAlpha(0.0f);
            setInterceptsMouseClicks(false, false);
            startTimerHz(60);
        }
        ~LogoFlyout() override { stopTimer(); }
        bool isOpen() const noexcept { return open; }
        void show()
        {
            if(open) return;
            open = true;
            page = pageWanted = 0; pageAlpha = 1.0f; pagePhase = 1.0f;
            setAlpha(anim);
            setInterceptsMouseClicks(true, true);
            setVisible(true);
            toFront(false);
        }
        void close() { if(! open) return; open = false; setInterceptsMouseClicks(false, false); if(onClose) onClose(); }
        void toggle() { open ? close() : show(); }
        void setPanelRect(juce::Rectangle<int> r) { panelRect = r; repaint(); }
        void setLogo(juce::Image img, juce::Rectangle<int> r) { logoImg = std::move(img); logoRect = r; repaint(); }
        void setPageLogos(juce::Image eye, juce::Image brand2) { eyeImg = std::move(eye); brand2Img = std::move(brand2); repaint(); }
        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colours::black.withAlpha(0.6f));
            auto p = panelRect.toFloat();
            g.setColour(juce::Colour(0xff17171c));
            g.fillRoundedRectangle(p, 12.0f);
            g.setColour(juce::Colour(0xff45aeb1));
            g.drawRoundedRectangle(p, 12.0f, 1.2f);
            if(logoImg.isValid())
                g.drawImage(logoImg, logoRect.toFloat());
            {
                auto row = panelRect.reduced(24, 0).withTop(logoRect.getBottom() + 4).withHeight(18);
                g.setColour(juce::Colour(0xffc4c4cc));
                g.setFont(KnobLookAndFeel::courier(KnobLookAndFeel::uiFont));
                g.drawText("           Version 1.1.0", row, juce::Justification::centredLeft, false);
                g.drawText("By Project Alletis           ", row, juce::Justification::centredRight, false);
            }
            const float arrowA = 0.35f + 0.65f * pageAlpha;
            drawArrow(g, leftArrowRect(), true, page > 0, arrowA);
            drawArrow(g, rightArrowRect(), false, page < 1, arrowA);
            const int cx = panelRect.getCentreX();
            int y = logoRect.getBottom();
            if(page == 0)
            {
                y += 22 + 16;
                drawSection(g, "DSP & DESIGN", y, pageAlpha); y += 26;
                drawLogoName(g, eyeImg, "Alletis", cx, y + 12, pageAlpha); y += 40;
                drawSection(g, "DEDICATED TO", y, pageAlpha); y += 26;
                drawLogoName(g, brand2Img, "XuAlex5", cx, y + 12, pageAlpha);
                {
                    const auto f = KnobLookAndFeel::courier(KnobLookAndFeel::uiFont);
                    const int tw = textW(f, githubText);
                    const auto lr = githubLinkRect();
                    g.setColour(juce::Colour(0xff6ecdd0).withMultipliedAlpha(pageAlpha));
                    g.setFont(f);
                    g.drawText(githubText, lr, juce::Justification::centred, false);
                    g.fillRect((float) lr.getCentreX() - tw * 0.5f, (float) lr.getBottom() - 2.0f, (float) tw, 1.0f);
                }
            }
            else
            {
                y += 22 + 8;
                drawSection(g, "SPECIAL THANKS", y, pageAlpha); y += 20;
                for(auto* n : { "ZL Audio, Cure Audio, saundix, YuanYuy,",
                                "sukabing, A.C.F., IAMMRGODIE, Meowtronix,",
                                "sout, Killy, KPa_11, farah, Hck,"})
                {
                    drawLine(g, n, y, pageAlpha);
                    y += 14;
                }
                y += 4;
                drawLine(g, "and you, for testing this plugin rn.", y, pageAlpha);
            }
        }
        void mouseDown(const juce::MouseEvent& e) override
        {
            const auto pos = e.getPosition();
            if(leftArrowRect().contains(pos)) { pageWanted = juce::jmax(0, page - 1); return; }
            if(rightArrowRect().contains(pos)) { pageWanted = juce::jmin(1, page + 1); return; }
            if(page == 0 && pagePhase >= 1.0f && githubLinkRect().contains(pos))
            {
                juce::URL(githubUrl).launchInDefaultBrowser();
                return;
            }
            if(! panelRect.contains(pos)) close();
        }
    private:
        juce::Rectangle<int> leftArrowRect() const { return { panelRect.getX() + 6, panelRect.getCentreY() - 22, 30, 44 }; }
        juce::Rectangle<int> rightArrowRect() const { return { panelRect.getRight() - 36, panelRect.getCentreY() - 22, 30, 44 }; }
        juce::Rectangle<int> githubLinkRect() const
        {
            const int tw = textW(KnobLookAndFeel::courier(KnobLookAndFeel::uiFont), githubText) + 10;
            return juce::Rectangle<int> (tw, 18).withCentre({ panelRect.getCentreX(), panelRect.getBottom() - 24 });
        }
        static int textW(const juce::Font& f, const juce::String& s)
        {
            juce::GlyphArrangement ga;
            ga.addLineOfText(f, s, 0.0f, 0.0f);
            return juce::roundToInt(ga.getBoundingBox(0, -1, true).getWidth());
        }
        void drawArrow(juce::Graphics& g, juce::Rectangle<int> r, bool left, bool enabled, float a)
        {
            const float cx = (float) r.getCentreX(), cy = (float) r.getCentreY();
            const float w = 9.0f, h = 17.0f;
            juce::Path pth;
            if(left) { pth.startNewSubPath(cx + w * 0.5f, cy - h * 0.5f); pth.lineTo(cx - w * 0.5f, cy); pth.lineTo(cx + w * 0.5f, cy + h * 0.5f); }
            else { pth.startNewSubPath(cx - w * 0.5f, cy - h * 0.5f); pth.lineTo(cx + w * 0.5f, cy); pth.lineTo(cx - w * 0.5f, cy + h * 0.5f); }
            g.setColour((enabled ? juce::Colour(0xff6ecdd0) : juce::Colour(0xff34343e)).withMultipliedAlpha(a));
            g.strokePath(pth, juce::PathStrokeType(2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        void drawSection(juce::Graphics& g, const juce::String& t, int y, float a)
        {
            g.setColour(juce::Colour(0xff6ecdd0).withMultipliedAlpha(a));
            g.setFont(KnobLookAndFeel::courier(KnobLookAndFeel::uiFont));
            g.drawText(t, panelRect.getX(), y, panelRect.getWidth(), 18, juce::Justification::centred, false);
        }
        void drawLine(juce::Graphics& g, const juce::String& t, int y, float a)
        {
            g.setColour(juce::Colour(0xffd0d0d8).withMultipliedAlpha(a));
            g.setFont(KnobLookAndFeel::courier(KnobLookAndFeel::uiFont));
            g.drawText(t, panelRect.getX(), y, panelRect.getWidth(), 18, juce::Justification::centred, false);
        }
        void drawLogoName(juce::Graphics& g, const juce::Image& img, const juce::String& name,
                           int cx, int cy, float a)
        {
            const int ih = 22;
            const auto f = KnobLookAndFeel::courier(15.0f);
            const int lw = img.isValid() ? juce::roundToInt(ih * (float) img.getWidth() / (float) img.getHeight()) : 0;
            const int gap = (lw > 0 ? 8 : 0);
            const int tw = textW(f, name);
            int x = cx - (lw + gap + tw) / 2;
            if(img.isValid())
            {
                g.setOpacity(a);
                g.drawImage(img, juce::Rectangle<int> (x, cy - ih / 2, lw, ih).toFloat());
                x += lw + gap;
            }
            g.setColour(juce::Colour(0xffe8e8ee).withMultipliedAlpha(a));
            g.setFont(f);
            g.drawText(name, x, cy - ih / 2, tw + 4, ih, juce::Justification::centredLeft, false);
        }
        void timerCallback() override
        {
            const float t = open ? 1.0f : 0.0f;
            if(std::abs(t - anim) >= 0.0004f) { anim += (t - anim) / 3.0f; setAlpha(anim); }
            else if(anim != t) { anim = t; setAlpha(anim); if(! open) setVisible(false); }
            bool moving = false;
            if(pageWanted != page)
            {
                pagePhase = juce::jmax(0.0f, pagePhase - pageStep);
                if(pagePhase <= 0.0f) page = pageWanted;
                moving = true;
            }
            else if(pagePhase < 1.0f)
            {
                pagePhase = juce::jmin(1.0f, pagePhase + pageStep);
                moving = true;
            }
            if(moving)
            {
                pageAlpha = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::pi * pagePhase);
                repaint();
            }
        }
        bool open = false;
        float anim = 0.0f;
        int page = 0, pageWanted = 0;
        float pageAlpha = 1.0f;
        float pagePhase = 1.0f;
        static constexpr float pageStep = 1.0f / 9.0f;
        juce::Rectangle<int> panelRect;
        juce::Image logoImg;
        juce::Rectangle<int> logoRect;
        juce::Image eyeImg, brand2Img;
        const juce::String githubText { "GITHUB REPOSITORY" };
        const juce::String githubUrl { "https://github.com/Alletis/fluorescence1" };
    };
    LogoFlyout logoFlyout;
    HoverHintBox hoverHintBox;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NewProjectAudioProcessorEditor)
};