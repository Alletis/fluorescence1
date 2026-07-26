#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "KnobLookAndFeel.h"
#include "SmoothColour.h"
class SmoothComboBox : public juce::ComboBox,
                       private juce::Timer
{
public:
    SmoothComboBox()
    {
        setColour(juce::ComboBox::textColourId, juce::Colours::transparentBlack);
        startTimerHz(60);
    }
    ~SmoothComboBox() override { stopTimer(); }
    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(1.0f, 1.0f);
        const float corner = 3.0f;
        g.setColour(fillS.get());
        g.fillRoundedRectangle(b, corner);
        g.setColour(lineS.get());
        g.drawRoundedRectangle(b, corner, 1.2f);
        const float aw = 9.0f, ah = 4.5f;
        const float ax = b.getRight() - 13.0f, ay = b.getCentreY();
        juce::Path p;
        p.startNewSubPath(ax - aw * 0.5f, ay - ah * 0.5f);
        p.lineTo(ax, ay + ah * 0.5f);
        p.lineTo(ax + aw * 0.5f, ay - ah * 0.5f);
        g.setColour(arrowS.get());
        g.strokePath(p, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        const auto textBounds = getLocalBounds().reduced(6, 2).withTrimmedRight(18);
        const auto textCol = isEnabled() ? juce::Colour(0xffe8e8ee) : juce::Colour(0xff55555c);
        g.setColour(textCol);
        g.setFont(KnobLookAndFeel::courier(juce::jmin(14.0f, b.getHeight() * 0.50f)));
        g.drawFittedText(getText(), textBounds, juce::Justification::centredLeft, 1);
    }
private:
    void timerCallback() override
    {
        const bool en = isEnabled();
        const bool hov = isMouseOver(true);
        const bool prs = isMouseButtonDown();
        juce::Colour fillT, lineT, arrowT;
        if(! en)
        {
            fillT = juce::Colour(0xff121216);
            lineT = juce::Colour(0xff2c2c32);
            arrowT = juce::Colour(0xff44444c);
        }
        else
        {
            fillT = prs ? juce::Colour(0xff0c0c10) : hov ? juce::Colour(0xff20202a) : juce::Colour(0xff15151a);
            lineT = prs ? juce::Colour(0xff307a7c) : hov ? juce::Colour(0xff6ecdd0) : juce::Colour(0xff4a4a54);
            arrowT = prs ? juce::Colour(0xff307a7c) : hov ? juce::Colour(0xff9fe3e5) : juce::Colour(0xff8a8a92);
        }
        const float rate = 1.0f / 3.0f;
        if(! primed)
        {
            fillS.set(fillT); lineS.set(lineT); arrowS.set(arrowT);
            primed = true; repaint(); return;
        }
        bool moving = false;
        if(fillS.approach(fillT, rate)) moving = true;
        if(lineS.approach(lineT, rate)) moving = true;
        if(arrowS.approach(arrowT, rate)) moving = true;
        if(moving) repaint();
    }
    SmoothColour fillS, lineS, arrowS;
    bool primed = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SmoothComboBox)
};