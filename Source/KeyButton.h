#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "KnobLookAndFeel.h"
#include "SmoothColour.h"
#include <cmath>
#include <functional>
class KeyButton : public juce::Component,
                  private juce::Timer
{
public:
    std::function<void(int)> onValueChange;
    std::function<void()> onMenuRequest;
    KeyButton() { startTimerHz(60); }
    ~KeyButton() override { stopTimer(); }
    void setMagentaAccent(bool enabled) { magentaAccent = enabled; }
    void setIndex(int i, bool notify = false)
    {
        const int n = ((i % 12) + 12) % 12;
        if(n != index)
        {
            index = n;
            repaint();
            if(notify && onValueChange) onValueChange(index);
        }
    }
    int getIndex() const noexcept { return index; }
    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(0.5f);
        const float corner = 3.0f;
        g.setColour(fillS.get());
        g.fillRoundedRectangle(b, corner);
        g.setColour(lineS.get());
        g.drawRoundedRectangle(b, corner, 1.2f);
        g.setColour(textS.get());
        g.setFont(KnobLookAndFeel::courier(juce::jmin(15.0f, b.getHeight() * 0.55f)));
        g.drawFittedText(noteName(index), getLocalBounds().reduced(6, 0),
                          juce::Justification::centred, 1);
    }
    void mouseDown(const juce::MouseEvent&) override
    {
        if(! isEnabled()) return;
        appliedSteps = 0;
        changed = false;
        held = true;
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if(! isEnabled()) return;
        const auto off = e.getOffsetFromDragStart();
        const float along = (float) off.x - (float) off.y;
        const int steps = (int) (along / pixelsPerStep);
        if(steps != appliedSteps)
        {
            setIndex(index + (steps - appliedSteps), true);
            appliedSteps = steps;
            changed = true;
        }
    }
    void mouseUp(const juce::MouseEvent&) override
    {
        const bool wasHeld = held;
        held = false;
        if(isEnabled() && wasHeld && ! changed && onMenuRequest)
            onMenuRequest();
    }
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        if(! isEnabled()) return;
        const float primaryDelta = std::abs(wheel.deltaY) >= std::abs(wheel.deltaX)
                                     ? wheel.deltaY : wheel.deltaX;
        if(primaryDelta > 0.0f)
            setIndex(index + 1, true);
        else if(primaryDelta < 0.0f)
            setIndex(index - 1, true);
    }
    static juce::String noteName(int i)
    {
        static const char* n[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        return n[((i % 12) + 12) % 12];
    }
private:
    void timerCallback() override
    {
        const bool en = isEnabled();
        const bool hov = isMouseOver(true);
        const bool prs = held;
        juce::Colour fillT, lineT, textT;
        if(! en)
        {
            fillT = juce::Colour(0xff121216);
            lineT = juce::Colour(0xff2c2c32);
            textT = juce::Colour(0xff55555c);
        }
        else
        {
            fillT = prs ? pressedFill() : hov ? hoverFill() : idleFill();
            lineT = prs ? pressedLine() : hov ? hoverLine() : idleLine();
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
    static constexpr float pixelsPerStep = 16.0f;
    int index = 0;
    int appliedSteps = 0;
    bool changed = false;
    bool held = false;
    SmoothColour fillS, lineS, textS;
    bool primed = false;
    bool magentaAccent = false;
    juce::Colour idleFill() const { return juce::Colour(0xff15151a); }
    juce::Colour hoverFill() const { return magentaAccent ? juce::Colour(0xff281d30) : juce::Colour(0xff20202a); }
    juce::Colour pressedFill() const
    {
        return magentaAccent ? juce::Colour(0xff160f18) : juce::Colour(0xff0c0c10);
    }
    juce::Colour idleLine() const { return juce::Colour(0xff4a4a54); }
    juce::Colour hoverLine() const { return magentaAccent ? juce::Colour(0xffffbbff) : juce::Colour(0xff6ecdd0); }
    juce::Colour pressedLine() const
    {
        return magentaAccent ? juce::Colour(0xffb85cc7) : juce::Colour(0xff307a7c);
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeyButton)
};