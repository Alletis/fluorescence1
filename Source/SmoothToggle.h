#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "KnobLookAndFeel.h"
#include <cmath>
class SmoothToggleButton : public juce::ToggleButton,
                           private juce::Timer
{
public:
    explicit SmoothToggleButton(const juce::String& text)
        : juce::ToggleButton(text)
    {
        startTimerHz(60);
    }
    ~SmoothToggleButton() override { stopTimer(); }
private:
    struct SmoothColour
    {
        float r = 0, g = 0, b = 0, a = 1;
        void set(juce::Colour c) { r = c.getFloatRed(); g = c.getFloatGreen(); b = c.getFloatBlue(); a = c.getFloatAlpha(); }
        juce::Colour get() const { return juce::Colour::fromFloatRGBA(r, g, b, a); }
        bool approach(juce::Colour c, float rate)
        {
            const float dr = c.getFloatRed() - r, dg = c.getFloatGreen() - g,
                        db = c.getFloatBlue() - b, da = c.getFloatAlpha() - a;
            r += dr * rate; g += dg * rate; b += db * rate; a += da * rate;
            return std::abs(dr) + std::abs(dg) + std::abs(db) + std::abs(da) > 0.002f;
        }
    };
    void timerCallback() override
    {
        const bool on = getToggleState();
        const bool hov = isOver();
        const bool prs = isDown();
        const juce::Colour fillT = on ? (prs ? juce::Colour(0xff08222b) : hov ? juce::Colour(0xff123a42) : juce::Colour(0xff15151a))
                                       : (prs ? juce::Colour(0xff0c0c10) : juce::Colour(0xff15151a));
        const juce::Colour lineT = on ? (prs ? juce::Colour(0xff307a7c) : hov ? juce::Colour(0xff6ecdd0) : juce::Colour(0xff45aeb1))
                                       : (prs ? juce::Colour(0xff5a5a64) : hov ? juce::Colour(0xff6a6a74) : juce::Colour(0xff4a4a54));
        const juce::Colour markT = prs ? juce::Colour(0xff307a7c) : hov ? juce::Colour(0xff6ecdd0) : juce::Colour(0xff45aeb1);
        const float onT = on ? 1.0f : 0.0f;
        const float rate = 1.0f / 3.0f;
        if(! primed)
        {
            fillS.set(fillT); lineS.set(lineT); markS.set(markT); onAmt = onT;
            primed = true; repaint(); return;
        }
        bool moving = false;
        if(fillS.approach(fillT, rate)) moving = true;
        if(lineS.approach(lineT, rate)) moving = true;
        if(markS.approach(markT, rate)) moving = true;
        const float d = onT - onAmt; onAmt += d * rate;
        if(std::abs(d) > 0.002f) moving = true;
        if(moving) repaint();
    }
    void paintButton(juce::Graphics& g, bool, bool) override
    {
        const float h = (float) getHeight();
        const float box = juce::jlimit(14.0f, h - 2.0f, h * 0.58f);
        const float fontSize = KnobLookAndFeel::uiFont;
        const float top = (h - box) * 0.5f;
        juce::Rectangle<float> boxBounds(3.0f, top, box, box);
        const float corner = box * 0.22f;
        g.setColour(fillS.get());
        g.fillRoundedRectangle(boxBounds, corner);
        g.setColour(lineS.get());
        g.drawRoundedRectangle(boxBounds, corner, juce::jmax(1.5f, box * 0.08f));
        if(onAmt > 0.01f)
        {
            const float inset = juce::jmax(1.5f, box * 0.20f);
            auto inner = boxBounds.reduced(inset);
            const float innerCorner = juce::jmax(1.0f, corner - 0.5f * inset);
            g.setColour(markS.get().withMultipliedAlpha(onAmt));
            g.fillRoundedRectangle(inner, innerCorner);
        }
        g.setColour(findColour(juce::ToggleButton::textColourId));
        g.setFont(KnobLookAndFeel::courier(fontSize));
        if(! isEnabled())
            g.setOpacity(0.5f);
        g.drawFittedText(getButtonText(),
                          getLocalBounds().withTrimmedLeft((int) box + 12).withTrimmedRight(2),
                          juce::Justification::centredLeft, 10);
    }
    SmoothColour fillS, lineS, markS;
    float onAmt = 0.0f;
    bool primed = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SmoothToggleButton)
};