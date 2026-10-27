#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "KnobLookAndFeel.h"
#include "SmoothColour.h"
#include "Oklch.h"
#include <cmath>
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
    void setStatusIndicatorEnabled(bool enabled)
    {
        if(statusIndicatorEnabled == enabled) return;
        statusIndicatorEnabled = enabled;
        if(enabled)
        {
            statusNow = statusFrom = statusTo = wantedMotion();
            statusStartMs = juce::Time::getMillisecondCounterHiRes();
            statusPrimed = true;
        }
        else
        {
            statusPrimed = false;
        }
        repaint();
    }
    std::function<juce::String()> textProvider;
    void paintButton(juce::Graphics& g, bool, bool) override
    {
        auto b = getLocalBounds().toFloat().reduced(1.0f, 1.0f);
        const float corner = 3.0f;
        if(statusIndicatorEnabled)
        {
            paintStatusButton(g, b, corner);
            return;
        }
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
    struct StatusMotion
    {
        float active = 0.0f, hovered = 0.0f, pressed = 0.0f, enabled = 1.0f;
    };
    static juce::Colour tuneAccent(juce::Colour colour, float chromaFactor,
                                   float lightnessDelta)
    {
        auto lab = oklch::toOKLab(colour);
        lab.L = juce::jlimit(0.0f, 1.0f, lab.L + lightnessDelta);
        lab.a *= chromaFactor;
        lab.b *= chromaFactor;
        return oklch::fromOKLab(lab);
    }
    static StatusMotion mixMotion(StatusMotion a, StatusMotion b, float t) noexcept
    {
        return { a.active + (b.active-a.active)*t,
                 a.hovered + (b.hovered-a.hovered)*t,
                 a.pressed + (b.pressed-a.pressed)*t,
                 a.enabled + (b.enabled-a.enabled)*t };
    }
    static bool different(StatusMotion a, StatusMotion b) noexcept
    {
        return a.active != b.active || a.hovered != b.hovered
            || a.pressed != b.pressed || a.enabled != b.enabled;
    }
    StatusMotion wantedMotion() const noexcept
    {
        const bool en = isEnabled();
        return { getToggleState() ? 1.0f : 0.0f,
                 en && isOver() ? 1.0f : 0.0f,
                 en && isDown() ? 1.0f : 0.0f,
                 en ? 1.0f : 0.0f };
    }
    void animateStatus()
    {
        const auto wanted = wantedMotion();
        const double now = juce::Time::getMillisecondCounterHiRes();
        if(! statusPrimed)
        {
            statusNow = statusFrom = statusTo = wanted;
            statusStartMs = now;
            statusPrimed = true;
            repaint();
            return;
        }
        if(different(wanted, statusTo))
        {
            statusFrom = statusNow;
            const bool toggleChanged = wanted.active != statusTo.active;
            const bool pressStarted = wanted.pressed > statusTo.pressed;
            statusTo = wanted;
            statusStartMs = now;
            statusDurationMs = toggleChanged ? 220.0
                             : pressStarted ? 70.0
                             : 140.0;
        }
        const float t = juce::jlimit(0.0f, 1.0f,
            (float)((now - statusStartMs) / statusDurationMs));
        const float eased = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::pi * t);
        const auto next = mixMotion(statusFrom, statusTo, eased);
        if(different(statusNow, next))
        {
            statusNow = next;
            repaint();
        }
    }
    void paintStatusButton(juce::Graphics& g, juce::Rectangle<float> b, float corner)
    {
        const auto v = statusNow;
        auto fill = juce::Colour(0xff141418)
            .interpolatedWith(juce::Colour(0xff1b1b21), v.hovered);
        fill = fill.interpolatedWith(juce::Colour(0xff111116), v.pressed);
        fill = juce::Colour(0xff121216).interpolatedWith(fill, v.enabled);
        auto textColour = juce::Colour(0xffc4c4cc)
            .interpolatedWith(juce::Colour(0xffdddde4), v.hovered);
        textColour = textColour.interpolatedWith(juce::Colour(0xffb8b8c0), v.pressed);
        textColour = juce::Colour(0xff55555c).interpolatedWith(textColour, v.enabled);
        g.setColour(fill);
        g.fillRoundedRectangle(b, corner);
        const auto font = KnobLookAndFeel::courier(
            juce::jmin(14.0f, b.getHeight() * 0.50f));
        const float satFactor = 1.0f + 0.18f*v.hovered - 0.25f*v.pressed;
        constexpr float pressedLightnessDrop = 0.24f;
        const float lightDelta = 0.018f*v.hovered - pressedLightnessDrop*v.pressed;
        const auto teal = tuneAccent(juce::Colour(0xff45aeb1), satFactor, lightDelta);
        const auto magenta = tuneAccent(juce::Colour(0xffeb8fff), satFactor, lightDelta);
        const auto offIdle = juce::Colour(0xff4a4a54);
        const auto offHover = juce::Colour(0xff65656e);
        const auto offPressed = juce::Colour(0xff35353e);
        const auto off = oklch::lerp(
            oklch::lerp(offIdle, offHover, v.hovered),
            offPressed, v.pressed);
        const auto disabledGrey = juce::Colour(0xff2c2c32);
        const auto borderStop = [&](float fraction) -> juce::Colour
        {
            const auto lerpColour = oklch::lerp(teal, magenta, fraction);
            const auto activeColour = lerpColour.withSaturation(0.47f).withBrightness(0.95f * lerpColour.getBrightness());
            const auto stateColour = oklch::lerp(off, activeColour, v.active);
            return oklch::lerp(disabledGrey, stateColour, v.enabled);
        };
        juce::ColourGradient gradient(
            borderStop(0.0f), b.getX(), b.getY(),
            borderStop(1.0f), b.getRight(), b.getBottom(), false);
        for(int i = 1; i < 8; ++i)
        {
            const float fraction = (float) i / 8.0f;
            gradient.addColour(fraction, borderStop(fraction));
        }
        g.setGradientFill(gradient);
        constexpr float borderWidth = 1.2f;
        g.drawRoundedRectangle(b, corner, borderWidth);
        g.setColour(textColour);
        g.setFont(font);
        g.drawFittedText(getButtonText(), b.toNearestInt(),
                         juce::Justification::centred, 1);
    }
    void timerCallback() override
    {
        if(textProvider)
        {
            auto t = textProvider();
            if(t != getButtonText())
                setButtonText(t);
        }
        if(statusIndicatorEnabled)
        {
            animateStatus();
            return;
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
    bool statusIndicatorEnabled = false;
    bool statusPrimed = false;
    double statusStartMs = 0.0;
    double statusDurationMs = 140.0;
    StatusMotion statusNow, statusFrom, statusTo;
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