#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "KnobLookAndFeel.h"
#include <array>
#include <atomic>
#include <cmath>
class PianoRoll : public juce::Component,
                  private juce::Timer
{
public:
    explicit PianoRoll(juce::AudioProcessorValueTreeState& state)
        : apvts(state)
    {
        for(int i = 0; i < 12; ++i)
            note[i] = apvts.getParameter("note" + juce::String(i));
        midiParam = apvts.getParameter("targetMode");
        startTimerHz(60);
    }
    ~PianoRoll() override { stopTimer(); }
    void setMidiMask(const std::atomic<int>* m) { midiMask = m; }
    void paint(juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const float ww = b.getWidth() / 7.0f;
        const float border = juce::jmax(2.0f, ww * 0.05f);
        const juce::Colour bg(0xff121216);
        for(int pass = 0; pass < 2; ++pass)
            for(int i = 0; i < 12; ++i)
            {
                if(isBlack(i) != (pass == 1)) continue;
                const auto r = keyRect(i, b);
                const float corner = juce::jmin(isBlack(i) ? 5.0f : 7.0f, r.getWidth() * 0.22f);
                g.setColour(fillS[(size_t) i].get());
                g.fillRoundedRectangle(r, corner);
                g.setColour(bg);
                g.drawRoundedRectangle(r, corner, border);
                const bool on = onAmt[(size_t) i] > 0.5f;
                const auto area = (isBlack(i) ? r.withTrimmedTop(r.getHeight() * 0.35f)
                                               : r.withTrimmedTop(r.getHeight() * 0.70f)).toNearestInt();
                g.setColour(isBlack(i) ? (on ? juce::Colour(0xff241526) : juce::Colour(0xff8a8a92))
                                         : (on ? juce::Colour(0xff2a1830) : juce::Colour(0xffc8c8d0)));
                g.setFont(KnobLookAndFeel::courier(KnobLookAndFeel::uiFont));
                g.drawFittedText(names[i], area, juce::Justification::centred, 1);
            }
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        if(midiOn()) return;
        armed = hitTest(e.position);
        pressed = armed;
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if(midiOn()) return;
        pressed = (armed >= 0 && hitTest(e.position) == armed) ? armed : -1;
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        if(! midiOn() && armed >= 0 && hitTest(e.position) == armed && note[armed] != nullptr)
            note[armed]->setValueNotifyingHost(isOn(armed) ? 0.0f : 1.0f);
        armed = -1;
        pressed = -1;
    }
    void mouseMove(const juce::MouseEvent& e) override
    {
        hovered = midiOn() ? -1 : hitTest(e.position);
    }
    void mouseExit(const juce::MouseEvent&) override { hovered = -1; }
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
        const float rate = 1.0f / 3.0f;
        bool moving = false;
        for(int i = 0; i < 12; ++i)
        {
            const juce::Colour target = fillTarget(i);
            const float onT = isOn(i) ? 1.0f : 0.0f;
            if(! primed)
            {
                fillS[(size_t) i].set(target);
                onAmt[(size_t) i] = onT;
            }
            else
            {
                if(fillS[(size_t) i].approach(target, rate)) moving = true;
                const float d = onT - onAmt[(size_t) i];
                onAmt[(size_t) i] += d * rate;
                if(std::abs(d) > 0.002f) moving = true;
            }
        }
        if(! primed) { primed = true; repaint(); return; }
        if(moving) repaint();
    }
    juce::Colour fillTarget(int i) const
    {
        const bool on = isOn(i);
        const bool hov = (i == hovered);
        const bool prs = (i == pressed);
        const bool black = isBlack(i);
        if(on)
        {
            const auto base = juce::Colour(0xffeb8fff)
                                  .withMultipliedSaturation(0.62f)
                                  .withMultipliedBrightness(0.80f);
            if(prs) return base.withMultipliedBrightness(0.85f);
            if(hov) return base.withMultipliedSaturation(1.15f).withMultipliedBrightness(1.08f);
            return base;
        }
        if(midiOn())
            return juce::Colour(0xff45aeb1).withSaturation(0.30f).withBrightness(black ? 0.34f : 0.52f);
        if(black) return prs ? juce::Colour(0xff141418) : hov ? juce::Colour(0xff2a2a32) : juce::Colour(0xff1d1d23);
        return prs ? juce::Colour(0xff3a3a42) : hov ? juce::Colour(0xff5a5a64) : juce::Colour(0xff4a4a54);
    }
    bool isOn(int i) const
    {
        if(midiOn() && midiMask != nullptr)
            return(midiMask->load() & (1 << i)) != 0;
        return note[i] != nullptr && note[i]->getValue() >= 0.5f;
    }
    bool midiOn() const { return midiParam != nullptr
                              && juce::roundToInt(midiParam->getValue() * 2.0f) == 1; }
    static constexpr int whiteIndex[12] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
    static bool isBlack(int i) { return whiteIndex[i] < 0; }
    static constexpr int whitePc[7] = { 0, 2, 4, 5, 7, 9, 11 };
    static constexpr float blackWidthFactor = 0.66f;
    static constexpr float blackHeightFactor = 0.62f;
    juce::Rectangle<float> keyRect(int i, juce::Rectangle<float> b) const
    {
        const float ww = b.getWidth() / 7.0f;
        if(! isBlack(i))
            return { b.getX() + (float) whiteIndex[i] * ww, b.getY(), ww, b.getHeight() };
        const float seam = b.getX() + (float) whiteIndex[i + 1] * ww;
        const float bw = ww * blackWidthFactor;
        return { seam - bw * 0.5f, b.getY(), bw, b.getHeight() * blackHeightFactor };
    }
    int hitTest(juce::Point<float> p) const
    {
        const auto b = getLocalBounds().toFloat();
        if(! b.contains(p)) return -1;
        for(int i = 0; i < 12; ++i)
            if(isBlack(i) && keyRect(i, b).contains(p))
                return i;
        const float ww = b.getWidth() / 7.0f;
        const int wi = juce::jlimit(0, 6, (int) ((p.x - b.getX()) / ww));
        return whitePc[wi];
    }
    juce::AudioProcessorValueTreeState& apvts;
    juce::RangedAudioParameter* note[12] { };
    juce::RangedAudioParameter* midiParam = nullptr;
    const std::atomic<int>* midiMask = nullptr;
    int hovered = -1, pressed = -1, armed = -1;
    bool primed = false;
    std::array<SmoothColour, 12> fillS;
    std::array<float, 12> onAmt {};
    const juce::StringArray names { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRoll)
};