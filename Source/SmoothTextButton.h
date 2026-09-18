#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "KnobLookAndFeel.h"
#include "SmoothColour.h"
class SmoothTextButton : public juce::TextButton,
                         private juce::Timer
{
public:
    SmoothTextButton() { startTimerHz(60); }
    explicit SmoothTextButton(const juce::String& t) : juce::TextButton(t) { startTimerHz(60); }
    ~SmoothTextButton() override { stopTimer(); }
    void setActive(bool a) { active = a; }
    bool isActive() const noexcept { return active; }
    void setMagentaAccent(bool enabled) { magentaAccent = enabled; }
    void setNeutralStyle(bool enabled) { neutralStyle = enabled; }
    std::function<juce::String()> textProvider;
    void paintButton(juce::Graphics& g, bool, bool) override
    {
        auto b = getLocalBounds().toFloat().reduced(1.0f, 1.0f);
        const float corner = 3.0f;
        g.setColour(fillS.get());
        g.fillRoundedRectangle(b, corner);
        g.setColour(lineS.get());
        g.drawRoundedRectangle(b, corner, 1.2f);
        g.setColour(textS.get());
        g.setFont(KnobLookAndFeel::courier(juce::jmin(14.0f, b.getHeight() * 0.50f)));
        g.drawFittedText(getButtonText(), getLocalBounds().reduced(6, 2),
                          juce::Justification::centred, 2);
    }
private:
    void timerCallback() override
    {
        if(textProvider)
        {
            auto t = textProvider();
            if(t != getButtonText())
                setButtonText(t);
        }
        const bool en = isEnabled();
        const bool hov = isOver();
        const bool prs = isDown();
        juce::Colour fillT, lineT, textT;
        if(! en)
        {
            fillT = juce::Colour(0xff121216);
            lineT = juce::Colour(0xff2c2c32);
            textT = juce::Colour(0xff55555c);
        }
        else if(neutralStyle)
        {
            fillT = prs ? juce::Colour(0xff111116)
                    : hov ? juce::Colour(0xff1b1b21)
                        : juce::Colour(0xff141418);
            lineT = prs ? juce::Colour(0xff35353e)
                    : hov ? juce::Colour(0xff65656e)
                        : juce::Colour(0xff3f3f48);
            textT = prs ? juce::Colour(0xffb8b8c0)
                    : hov ? juce::Colour(0xffddddE4)
                        : juce::Colour(0xffc4c4cc);
        }
        else if(active)
        {
            fillT = prs ? activePressedFill() : hov ? activeHoverFill() : activeIdleFill();
            lineT = prs ? pressedLine() : hov ? hoverLine() : idleLine();
            textT = hov ? juce::Colour(0xfff2f2f6) : juce::Colour(0xffe8e8ee);
        }
        else
        {
            fillT = prs ? idlePressedFill() : hov ? idleHoverFill() : idleFill();
            lineT = prs ? pressedLine() : hov ? hoverLine() : idleOutline();
            textT = hov ? juce::Colour(0xfff2f2f6) : juce::Colour(0xffd0d0d8);
        }
        const float rate = 1.0f / 3.0f;
        if(! primed)
        {
            fillS.set(fillT); lineS.set(lineT); textS.set(textT);
            primed = true; repaint(); return;
        }
        bool moving = false;
        if(fillS.approach(fillT, rate)) moving = true;
        if(lineS.approach(lineT, rate)) moving = true;
        if(textS.approach(textT, rate)) moving = true;
        if(moving) repaint();
    }
    SmoothColour fillS, lineS, textS;
    bool primed = false;
    bool active = false;
    bool magentaAccent = false;
    bool neutralStyle = false;
    juce::Colour idleFill() const { return juce::Colour(0xff15151a); }
    juce::Colour idleHoverFill() const { return magentaAccent ? juce::Colour(0xff281d30) : juce::Colour(0xff20202a); }
    juce::Colour idlePressedFill() const { return magentaAccent ? juce::Colour(0xff160f18) : juce::Colour(0xff0c0c10); }
    juce::Colour activeIdleFill() const { return magentaAccent ? juce::Colour(0xff3b1f3c) : juce::Colour(0xff14323a); }
    juce::Colour activeHoverFill() const { return magentaAccent ? juce::Colour(0xff562b58) : juce::Colour(0xff1c4a52); }
    juce::Colour activePressedFill() const
    {
        return magentaAccent ? juce::Colour(0xff2e1630) : juce::Colour(0xff0c2429);
    }
    juce::Colour idleOutline() const { return juce::Colour(0xff4a4a54); }
    juce::Colour idleLine() const { return magentaAccent ? juce::Colour(0xffeb8fff) : juce::Colour(0xff45aeb1); }
    juce::Colour hoverLine() const { return magentaAccent ? juce::Colour(0xffffbbff) : juce::Colour(0xff6ecdd0); }
    juce::Colour pressedLine() const { return magentaAccent ? juce::Colour(0xffb85cc7) : juce::Colour(0xff307a7c); }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SmoothTextButton)
};