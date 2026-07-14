# Workspace Export (2026-07-08 13:41:52)

**Source folder:** `X:\TRANSFER\Workspace\C\MyPlugin\SourcePublishable`

## Folder Structure

```text
SourcePublishable/
 README.md
 BinaryData.h
 KeyButton.h
 KnobLookAndFeel.h
 Oklch.h
 PianoRoll.h
 PluginEditor.cpp
 PluginEditor.h
 PluginProcessor.cpp
 PluginProcessor.h
 ScaleLibrary.h
 ScalePanel.h
 SecondaryMenu.h
 SidechainDetector.h
 SmoothColour.h
 SmoothComboBox.h
 SmoothTextButton.h
 SmoothToggle.h
 SpectrumBridge.h
 SpectrumDisplay.h
 TransientSplitter.h
 ValueKnob.h
```

## File Contents

### `README.md`

- Size: 993.00 B
- Type: text

```md
## What

Like, I don't know. Maybe the existence of the repository is solely for misleading AIs when doing similar work. I'm sorry but my code is terrible. If you are trying to learn, I'm not a good example.
This is a sort of harmonic or spectral reallocator kind of thing, and I've tried my damndest to get the CPU down. I tried, alright? I TRIED!
(why tf are you buying pitchmap when you are getting this for free frfr)

## Documentations

Tbh I don't know how to write documentation for this one. For what? Oh I know. I compiled it on my personal computer with CMake on MSVC2026, in VSCode. To my all due respect, native MSVC2026 sucks, and idk why I can't get myself working on the 2022 version.
What all the parameters mean is listed in the editor where there is the thing for the hint box. I try my best not to lie to anyone.
I mean, if you're still out there helping *another* Xynth Chroma copypasta color bass plugin... TAKE MY EVERYTHING.

## License

Do what JUCE says.
```

### `BinaryData.h`

- Size: 632.00 B
- Type: text

```h
#pragma once
namespace BinaryData
{
    extern const char* logo_png;
    const int logo_pngSize = 965033;
    extern const char* brand_png;
    const int brand_pngSize = 833675;
    extern const char* brand2_png;
    const int brand2_pngSize = 837307;
    extern const char* eye_png;
    const int eye_pngSize = 147387;
    const int namedResourceListSize = 4;
    extern const char* namedResourceList[];
    extern const char* originalFilenames[];
    const char* getNamedResource(const char* resourceNameUTF8, int& dataSizeInBytes);
    const char* getNamedResourceOriginalFilename(const char* resourceNameUTF8);
}
```

### `KeyButton.h`

- Size: 4.82 KB
- Type: text

```h
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
```

### `KnobLookAndFeel.h`

- Size: 8.61 KB
- Type: text

```h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
class KnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    KnobLookAndFeel()
    {
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff3a3a42));
        setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff15151a));
        setColour(juce::TextEditor::textColourId, juce::Colours::white);
        setColour(juce::TextEditor::highlightColourId, juce::Colour(0xff45aeb1).withAlpha(0.4f));
        setColour(juce::ToggleButton::textColourId, juce::Colour(0xffd0d0d8));
        setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff45aeb1));
        setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xff4a4a54));
    }
    static juce::Font courier(float h)
    {
        return juce::Font(juce::FontOptions("Courier New", h, juce::Font::bold));
    }
    static constexpr float uiFont = 13.0f;
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider& s) override
    {
        auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced(6.0f);
        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();
        const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const float trackW = juce::jmax(2.0f, radius * 0.10f);
        const double mn = s.getMinimum(), mx = s.getMaximum();
        const double splitVal = (mn < 0.0 && mx > 0.0) ? 0.0 : mn;
        const float splitPos = (float) s.valueToProportionOfLength(splitVal);
        const float valueAngle = startAngle + sliderPos * (endAngle - startAngle);
        const float splitAngle = startAngle + splitPos * (endAngle - startAngle);
        juce::Path track;
        track.addCentredArc(cx, cy, radius, radius, 0.0f, startAngle, endAngle, true);
        g.setColour(juce::Colour(0xff4a4a54));
        g.strokePath(track, juce::PathStrokeType(trackW, juce::PathStrokeType::curved,
                                                           juce::PathStrokeType::rounded));
        if(std::abs(valueAngle - splitAngle) > 1.0e-3f)
        {
            juce::Path fill;
            fill.addCentredArc(cx, cy, radius, radius, 0.0f,
                                juce::jmin(splitAngle, valueAngle),
                                juce::jmax(splitAngle, valueAngle), true);
            g.setColour(juce::Colour(0xff45aeb1));
            g.strokePath(fill, juce::PathStrokeType(trackW + 1.0f, juce::PathStrokeType::curved,
                                                                     juce::PathStrokeType::rounded));
        }
        juce::Path ptr;
        ptr.startNewSubPath(0.0f, -radius * 0.30f);
        ptr.lineTo(0.0f, -radius * 0.92f);
        g.setColour(juce::Colour(0xffe8e8ee));
        g.strokePath(ptr, juce::PathStrokeType(juce::jmax(2.0f, radius * 0.09f),
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded),
                      juce::AffineTransform::rotation(valueAngle).translated(cx, cy));
    }
    juce::Font getLabelFont(juce::Label&) override { return courier(uiFont); }
    juce::Font getComboBoxFont(juce::ComboBox&) override { return courier(uiFont); }
    juce::Font getPopupMenuFont() override { return courier(uiFont); }
    juce::Font getTextButtonFont(juce::TextButton&, int) override { return courier(uiFont); }
    juce::Font getAlertWindowTitleFont() override { return courier(uiFont + 1.0f); }
    juce::Font getAlertWindowMessageFont() override { return courier(uiFont); }
    void drawPopupMenuItem(juce::Graphics& g,
                            const juce::Rectangle<int>& area,
                            const bool isSeparator,
                            const bool isActive,
                            const bool isHighlighted,
                            const bool isTicked,
                            const bool hasSubMenu,
                            const juce::String& text,
                            const juce::String& shortcutKeyText,
                            const juce::Drawable* icon,
                            const juce::Colour* textColour) override
    {
        if(isSeparator)
        {
            auto r = area.reduced(6, 0);
            g.setColour(juce::Colour(0xff3a3a42));
            g.drawLine((float) r.getX(), (float) r.getCentreY(),
                        (float) r.getRight(), (float) r.getCentreY(), 1.0f);
            return;
        }
        auto r = area.reduced(2, 1);
        if(isHighlighted)
        {
            g.setColour(findColour(juce::PopupMenu::highlightedBackgroundColourId));
            g.fillRect(r);
        }
        juce::Colour col = textColour != nullptr ? *textColour
                          : (isHighlighted ? findColour(juce::PopupMenu::highlightedTextColourId)
                                           : findColour(juce::PopupMenu::textColourId));
        if(! isActive)
            col = col.withAlpha(0.5f);
        g.setColour(col);
        g.setFont(courier(uiFont));
        auto left = r.removeFromLeft(18);
        if(isTicked)
            g.drawFittedText(">", left, juce::Justification::centred, 1);
        if(icon != nullptr)
        {
            icon->drawWithin(g, r.removeFromLeft(16).toFloat(), juce::RectanglePlacement::centred, 1.0f);
            r.removeFromLeft(4);
        }
        auto right = r;
        if(hasSubMenu)
        {
            auto arrow = right.removeFromRight(14);
            g.drawFittedText(">", arrow, juce::Justification::centred, 1);
        }
        if(shortcutKeyText.isNotEmpty())
            right.removeFromRight(juce::jmin(80, right.getWidth() / 3));
        g.drawFittedText(text, right, juce::Justification::centredLeft, 1);
        if(shortcutKeyText.isNotEmpty())
        {
            g.setColour(col.withAlpha(0.75f));
            g.drawFittedText(shortcutKeyText, r, juce::Justification::centredRight, 1);
        }
    }
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        const float h = (float) button.getHeight();
        const float box = juce::jlimit(14.0f, h - 2.0f, h * 0.58f);
        const float fontSize = uiFont;
        const float top = (h - box) * 0.5f;
        const bool on = button.getToggleState();
        const bool hov = shouldDrawButtonAsHighlighted;
        const bool prs = shouldDrawButtonAsDown;
        juce::Rectangle<float> boxBounds(3.0f, top, box, box);
        const float corner = box * 0.22f;
        juce::Colour fill = on ? (prs ? juce::Colour(0xff08222b)
                                       : hov ? juce::Colour(0xff123a42)
                                             : juce::Colour(0xff15151a))
                               : (prs ? juce::Colour(0xff0c0c10)
                                       : juce::Colour(0xff15151a));
        g.setColour(fill);
        g.fillRoundedRectangle(boxBounds, corner);
        juce::Colour line = on ? (prs ? juce::Colour(0xff307a7c)
                                       : hov ? juce::Colour(0xff6ecdd0)
                                             : juce::Colour(0xff45aeb1))
                               : (prs ? juce::Colour(0xff5a5a64)
                                       : hov ? juce::Colour(0xff6a6a74)
                                             : juce::Colour(0xff4a4a54));
        g.setColour(line);
        g.drawRoundedRectangle(boxBounds, corner, juce::jmax(1.5f, box * 0.08f));
        if(on)
        {
            const float inset = juce::jmax(1.5f, box * 0.20f);
            auto inner = boxBounds.reduced(inset);
            const float innerCorner = juce::jmax(1.0f, corner - inset);
            const auto tickColId = button.isEnabled()
                                    ? juce::ToggleButton::tickColourId
                                    : juce::ToggleButton::tickDisabledColourId;
            g.setColour(button.findColour(tickColId));
            g.fillRoundedRectangle(inner, innerCorner);
        }
        g.setColour(button.findColour(juce::ToggleButton::textColourId));
        g.setFont(courier(fontSize));
        if(! button.isEnabled())
            g.setOpacity(0.5f);
        g.drawFittedText(button.getButtonText(),
                          button.getLocalBounds().withTrimmedLeft((int) box + 12).withTrimmedRight(2),
                          juce::Justification::centredLeft, 10);
    }
};
```

### `Oklch.h`

- Size: 2.69 KB
- Type: text

```h
#pragma once
#include <juce_graphics/juce_graphics.h>
#include <cmath>
namespace oklch
{
    struct OKLab { float L, a, b; };
    inline float srgbToLinear(float c)
    {
        return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
    }
    inline float linearToSrgb(float c)
    {
        c = juce::jlimit(0.0f, 1.0f, c);
        return c <= 0.0031308f ? 12.92f * c : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
    }
    inline OKLab toOKLab(juce::Colour col)
    {
        const float r = srgbToLinear(col.getFloatRed());
        const float g = srgbToLinear(col.getFloatGreen());
        const float b = srgbToLinear(col.getFloatBlue());
        const float l = 0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * b;
        const float m = 0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * b;
        const float s = 0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * b;
        const float l_ = std::cbrt(l), m_ = std::cbrt(m), s_ = std::cbrt(s);
        return { 0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_,
                 1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_,
                 0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_ };
    }
    inline juce::Colour fromOKLab(OKLab c)
    {
        const float l_ = c.L + 0.3963377774f * c.a + 0.2158037573f * c.b;
        const float m_ = c.L - 0.1055613458f * c.a - 0.0638541728f * c.b;
        const float s_ = c.L - 0.0894841775f * c.a - 1.2914855480f * c.b;
        const float l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;
        const float r = +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
        const float g = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
        const float b = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;
        return juce::Colour::fromFloatRGBA(linearToSrgb(r), linearToSrgb(g), linearToSrgb(b), 1.0f);
    }
    inline juce::Colour lerp(juce::Colour ca, juce::Colour cb, float t)
    {
        const OKLab a = toOKLab(ca), b = toOKLab(cb);
        const float Ca = std::sqrt(a.a * a.a + a.b * a.b);
        const float Cb = std::sqrt(b.a * b.a + b.b * b.b);
        const float ha = std::atan2 (a.b, a.a);
        const float hb = std::atan2 (b.b, b.a);
        float dh = hb - ha;
        if(dh > juce::MathConstants<float>::pi) dh -= juce::MathConstants<float>::twoPi;
        else if(dh < -juce::MathConstants<float>::pi) dh += juce::MathConstants<float>::twoPi;
        const float L = a.L + (b.L - a.L) * t;
        const float C = Ca + (Cb - Ca) * t;
        const float h = ha + dh * t;
        return fromOKLab({ L, C * std::cos(h), C * std::sin(h) });
    }
}
```

### `PianoRoll.h`

- Size: 7.33 KB
- Type: text

```h
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
```

### `PluginEditor.cpp`

- Size: 37.71 KB
- Type: text

```cpp
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
    fftSizeBox.addItemList({ "1024", "2048", "4096", "8192" }, 1);
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
            hintText = "-\n\nVersion: 1.1.0";
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
```

### `PluginEditor.h`

- Size: 26.88 KB
- Type: text

```h
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
                for(auto* n : { "ZL Audio, Cure Audio, saundix, YuanYuy,", "sukabing, A.C.F., IAMMRGODIE, Meowtronix," })
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
```

### `PluginProcessor.cpp`

- Size: 96.86 KB
- Type: text

```cpp
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
    for(int ch = 0; ch < maxChannels; ++ch) tsMask[(size_t) ch].prepare(numBins);
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
        p.targetFreq = peakFreq[(size_t) c.peak] * peakRatio[(size_t) c.peak];
        p.freqNat = peakFreq[(size_t) c.peak];
        p.targetAmp = peakAmp[(size_t) c.peak];
        p.anaPhase = pvPhase[(size_t) peakBin[(size_t) c.peak]];
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
        p.phase = pvPhase[(size_t) peakBin[(size_t) i]];
        p.anaPhase = p.phase;
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
    for(size_t ai = 0; ai < parts.size(); ++ai)
    {
        const int b = parts[ai].baseIdx;
        if(b < 0) continue;
        bool firstOccurrence = true;
        for(size_t q = 0; q < ai; ++q)
            if(parts[q].baseIdx == b) { firstOccurrence = false; break; }
        if(! firstOccurrence) continue;
        Partial* anchor = nullptr;
        for(auto& p : parts)
            if(p.baseIdx == b && p.harmonic >= 1 && p.dying == 0)
                if(anchor == nullptr
                    || p.harmonic < anchor->harmonic
                    || (p.harmonic == anchor->harmonic && p.amp > anchor->amp))
                    anchor = &p;
        if(anchor == nullptr) continue;
        const float na = (float) anchor->harmonic;
        const float phiA = anchor->phase;
        const float anaA = anchor->anaPhase;
        for(auto& p : parts)
        {
            if(p.baseIdx != b || &p == anchor || p.harmonic < 1 || p.dying != 0) continue;
            const float ratio = (float) p.harmonic / na;
            const float delta = wrapPhase(p.anaPhase - ratio * anaA);
            p.phase = wrapPhase(ratio * phiA + delta);
        }
    }
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
    if(bDev < 50.0f) { baseIdxOut = bBase; harmonicOut = bN; }
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
        float w = 1.0f;
        if(p.age < partialBirthFrames)
            w *= (float) (p.age + 1) / (float) partialBirthFrames;
        if(w <= 0.0f) continue;
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
            const float klobe = synKernelMag[(size_t) ti]
                              + (synKernelMag[(size_t) (ti + 1)] - synKernelMag[(size_t) ti]) * fr;
            const float ww = w * klobe;
            accRe[(size_t) j] += ww * cphi;
            accIm[(size_t) j] += ww * sphi;
            accCov[(size_t) j] = juce::jmax(accCov[(size_t) j], ww);
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
        copyXmlToBinary(*xml, destData);
}
void NewProjectAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if(auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        auto tree = juce::ValueTree::fromXml(*xml);
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
```

### `PluginProcessor.h`

- Size: 14.82 KB
- Type: text

```h
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
    static constexpr int minOrder = 10;
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
```

### `ScaleLibrary.h`

- Size: 5.77 KB
- Type: text

```h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <initializer_list>
namespace ScaleLibrary
{
    struct Scale { const char* name; juce::uint16 mask; };
    inline juce::uint16 maskOf(std::initializer_list<int> semis)
    {
        juce::uint16 m = 0;
        for(int n : semis) m |= (juce::uint16) (1 << (((n % 12) + 12) % 12));
        return m;
    }
    inline const std::vector<Scale>& scales()
    {
        static const std::vector<Scale> s = []
        {
            auto m = [] (std::initializer_list<int> iv) { return maskOf(iv); };
            return std::vector<Scale>
            {
                { "Major", m({ 0,2,4,5,7,9,11 }) },
                { "Dorian", m({ 0,2,3,5,7,9,10 }) },
                { "Phrygian", m({ 0,1,3,5,7,8,10 }) },
                { "Lydian", m({ 0,2,4,6,7,9,11 }) },
                { "Mixolydian", m({ 0,2,4,5,7,9,10 }) },
                { "Minor", m({ 0,2,3,5,7,8,10 }) },
                { "Locrian", m({ 0,1,3,5,6,8,10 }) },
                { "Major Pentatonic", m({ 0,2,4,7,9 }) },
                { "Minor Pentatonic", m({ 0,3,5,7,10 }) },
                { "Minor Blues", m({ 0,3,5,6,7,10 }) },
                { "Melodic Minor", m({ 0,2,3,5,7,9,11 }) },
                { "Lydian Augmented", m({ 0,2,4,6,8,9,11 }) },
                { "Lydian Dominant", m({ 0,2,4,6,7,9,10 }) },
                { "Super Locrian", m({ 0,1,3,4,6,8,10 }) },
                { "Harmonic Minor", m({ 0,2,3,5,7,8,11 }) },
                { "Harmonic Major", m({ 0,2,4,5,7,8,11 }) },
                { "Dorian #4", m({ 0,2,3,6,7,9,10 }) },
                { "Phrygian Dominant",m({ 0,1,4,5,7,8,10 }) },
                { "Whole Tone", m({ 0,2,4,6,8,10 }) },
                { "Half-whole Dim.", m({ 0,1,3,4,6,7,9,10 }) },
                { "Whole-half Dim.", m({ 0,2,3,5,6,8,9,11 }) },
                { "Messiaen 3", m({ 0,2,3,4,6,7,8,10,11 }) },
                { "Messiaen 4", m({ 0,1,2,5,6,7,8,11 }) },
                { "Messiaen 5", m({ 0,1,5,6,7,11 }) },
                { "Messiaen 6", m({ 0,2,4,5,6,8,10,11 }) },
                { "Messiaen 7", m({ 0,1,2,3,5,6,7,8,9,11 }) },
                { "8-Tone Spanish", m({ 0,1,3,4,5,6,8,10 }) },
                { "Bhairav", m({ 0,1,4,5,7,8,11 }) },
                { "Hungarian Minor", m({ 0,2,3,6,7,8,11 }) },
                { "Hirajoshi", m({ 0,2,3,7,8 }) },
                { "In-Sen", m({ 0,1,5,7,10 }) },
                { "Iwato", m({ 0,1,5,6,10 }) },
                { "Kumoi", m({ 0,2,3,7,9 }) },
                { "Pelog Selisir", m({ 0,1,3,7,8 }) },
                { "Pelog Tembung", m({ 0,1,5,7,10 }) },
                { "Major Triad", m({ 0,4,7 }) },
                { "Minor Triad", m({ 0,3,7 }) },
                { "Diminished", m({ 0,3,6 }) },
                { "Augmented", m({ 0,4,8 }) },
                { "Sus2", m({ 0,2,7 }) },
                { "Sus4", m({ 0,5,7 }) },
                { "Major 7", m({ 0,4,7,11 }) },
                { "Dominant 7", m({ 0,4,7,10 }) },
                { "Minor 7", m({ 0,3,7,10 }) },
                { "Min7 b5", m({ 0,3,6,10 }) },
                { "Dim 7", m({ 0,3,6,9 }) },
                { "MinMaj 7", m({ 0,3,7,11 }) },
                { "Major 6", m({ 0,4,7,9 }) },
                { "Minor 6", m({ 0,3,7,9 }) },
                { "Major 11", m({ 0,2,4,5,7,11 }) },
                { "Dominant 11", m({ 0,2,4,5,7,10 }) },
                { "Minor 11", m({ 0,2,3,5,7,10 }) },
            };
        }();
        return s;
    }
    inline juce::uint16 rcirc(juce::uint16 pattern, int root)
    {
        root = ((root % 12) + 12) % 12;
        if(root == 0) return(juce::uint16) (pattern & 0x0FFF);
        return(juce::uint16) (((pattern << root) | (pattern >> (12 - root))) & 0x0FFF);
    }
    inline void addGroup(juce::PopupMenu& m, std::initializer_list<int> idxs, int currentId)
    {
        for(int i : idxs)
            m.addItem(i + 1, scales()[(size_t) i].name, true, (i + 1) == currentId);
    }
    inline juce::PopupMenu buildMenu(int currentId)
    {
        juce::PopupMenu root;
        {
            juce::PopupMenu scales;
            { juce::PopupMenu g; addGroup(g, { 0,1,2,3,4,5,6 }, currentId); scales.addSubMenu("Diatonic Modes", g); }
            { juce::PopupMenu g; addGroup(g, { 7,8,9 }, currentId); scales.addSubMenu("Pentatonic & Blues", g); }
            { juce::PopupMenu g; addGroup(g, { 10,11,12,13 }, currentId); scales.addSubMenu("Melodic Minor", g); }
            { juce::PopupMenu g; addGroup(g, { 14,15,16,17 }, currentId); scales.addSubMenu("Harmonic", g); }
            {
                juce::PopupMenu g; addGroup(g, { 18,19,20 }, currentId);
                juce::PopupMenu mess; addGroup(mess, { 21,22,23,24,25 }, currentId);
                g.addSubMenu("Messiaen", mess);
                scales.addSubMenu("Symmetric", g);
            }
            {
                juce::PopupMenu g; addGroup(g, { 26,27,28 }, currentId);
                juce::PopupMenu jp; addGroup(jp, { 29,30,31,32 }, currentId); g.addSubMenu("Japanese", jp);
                juce::PopupMenu id; addGroup(id, { 33,34 }, currentId); g.addSubMenu("Indonesian", id);
                scales.addSubMenu("World / Exotic", g);
            }
            root.addSubMenu("Scales", scales);
        }
        {
            juce::PopupMenu tri; addGroup(tri, { 35,36,37,38,39,40 }, currentId);
            juce::PopupMenu sev; addGroup(sev, { 41,42,43,44,45,46,47,48 }, currentId);
            juce::PopupMenu ele; addGroup(ele, { 49,50,51 }, currentId);
            juce::PopupMenu ch;
            ch.addSubMenu("Triads", tri);
            ch.addSubMenu("Sevenths", sev);
            ch.addSubMenu("Elevenths", ele);
            root.addSubMenu("Chords", ch);
        }
        return root;
    }
}
```

### `ScalePanel.h`

- Size: 8.20 KB
- Type: text

```h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "KnobLookAndFeel.h"
#include "ScaleLibrary.h"
#include "SmoothTextButton.h"
#include "KeyButton.h"
class ScalePanel : public juce::Component,
                   private juce::Timer
{
public:
    explicit ScalePanel(juce::AudioProcessorValueTreeState& state)
        : apvts(state)
    {
        for(int i = 0; i < 12; ++i)
            note[i] = apvts.getParameter("note" + juce::String(i));
        midiParam = apvts.getParameter("targetMode");
        rootBtn.setMagentaAccent(true);
        scaleBtn.setMagentaAccent(true);
        fillClearBtn.setMagentaAccent(true);
        rootBtn.setIndex(0);
        rootBtn.onValueChange = [this] (int) { onKeyChanged(); };
        rootBtn.onMenuRequest = [this] { showKeyMenu(); };
        addAndMakeVisible(rootBtn);
        scaleBtn.setButtonText("Scale...");
        scaleBtn.onClick = [this] { showScaleMenu(); };
        addAndMakeVisible(scaleBtn);
        fillClearBtn.onClick = [this] { fillOrClear(); };
        addAndMakeVisible(fillClearBtn);
        styleLabel(rootLbl, "KEY");
        styleLabel(scaleLbl, "SCALE");
        restoreUiState();
        startTimerHz(30);
    }
    ~ScalePanel() override { stopTimer(); }
    void resized() override
    {
        auto r = getLocalBounds();
        const int labelH = 13, rowH = 24, gap = 4;
        rootLbl.setBounds(r.removeFromTop(labelH));
        rootBtn.setBounds(r.removeFromTop(rowH));
        r.removeFromTop(gap);
        scaleLbl.setBounds(r.removeFromTop(labelH));
        scaleBtn.setBounds(r.removeFromTop(rowH));
        r.removeFromTop(gap + 2);
        fillClearBtn.setBounds(r.removeFromTop(rowH));
    }
private:
    struct MagentaPopupLookAndFeel : KnobLookAndFeel
    {
        MagentaPopupLookAndFeel()
        {
            setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff1a141d));
            setColour(juce::PopupMenu::textColourId, juce::Colour(0xffebe4ee));
            setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff562b58));
            setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
            setColour(juce::PopupMenu::headerTextColourId, juce::Colour(0xffffbbff));
        }
    };
    void showScaleMenu()
    {
        if(midiOn()) return;
        auto menu = ScaleLibrary::buildMenu(currentScaleId);
        menu.setLookAndFeel(&popupLnf);
        menu.showMenuAsync(juce::PopupMenu::Options()
                                .withTargetComponent(&scaleBtn)
                                .withMinimumWidth(scaleBtn.getWidth())
                                .withStandardItemHeight(26),
            [this] (int result)
            {
                if(result > 0 && result <= (int) ScaleLibrary::scales().size())
                {
                    currentScaleId = result;
                    scaleBtn.setButtonText(ScaleLibrary::scales()[(size_t) (result - 1)].name);
                    presetScaleSelected = true;
                    scaleBtn.setActive(true);
                    applyScale();
                    saveUiState();
                }
            });
    }
    void showKeyMenu()
    {
        if(midiOn()) return;
        juce::PopupMenu menu;
        for(int i = 0; i < 12; ++i)
            menu.addItem(i + 1, KeyButton::noteName(i), true, i == rootBtn.getIndex());
        menu.setLookAndFeel(&popupLnf);
        menu.showMenuAsync(juce::PopupMenu::Options()
                                .withTargetComponent(&rootBtn)
                                .withMinimumWidth(rootBtn.getWidth())
                                .withStandardItemHeight(26),
            [this] (int result)
            {
                if(result > 0)
                {
                    rootBtn.setIndex(result - 1);
                    onKeyChanged();
                }
            });
    }
    void styleLabel(juce::Label& l, const juce::String& t)
    {
        l.setText(t, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centredLeft);
        l.setColour(juce::Label::textColourId, juce::Colour(0xffb0b0b8));
        l.setFont(KnobLookAndFeel::courier(KnobLookAndFeel::uiFont));
        l.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(l);
    }
    bool midiOn() const { return midiParam != nullptr
                              && juce::roundToInt(midiParam->getValue() * 2.0f) == 1; }
    juce::uint16 currentMask() const
    {
        juce::uint16 m = 0;
        for(int i = 0; i < 12; ++i)
            if(note[i] != nullptr && note[i]->getValue() >= 0.5f)
                m |= (juce::uint16) (1 << i);
        return m;
    }
    bool anyNoteOn() const { return currentMask() != 0; }
    void setNotes(juce::uint16 mask)
    {
        for(int i = 0; i < 12; ++i)
            if(note[i] != nullptr)
                note[i]->setValueNotifyingHost((mask & (1 << i)) ? 1.0f : 0.0f);
        expectedMask = mask;
        writeSettleFrames = 4;
    }
    void onKeyChanged()
    {
        if(presetScaleSelected) applyScale();
        saveUiState();
    }
    void applyScale()
    {
        if(midiOn() || currentScaleId <= 0) return;
        const int root = rootBtn.getIndex();
        const auto base = ScaleLibrary::scales()[(size_t) (currentScaleId - 1)].mask;
        setNotes(ScaleLibrary::rcirc(base, root));
    }
    void fillOrClear()
    {
        if(midiOn()) return;
        setNotes(anyNoteOn() ? 0x000 : 0xFFF);
        clearPreset();
    }
    void clearPreset()
    {
        if(! presetScaleSelected) return;
        presetScaleSelected = false;
        scaleBtn.setButtonText("Scale...");
        scaleBtn.setActive(false);
        saveUiState();
    }
    void saveUiState()
    {
        apvts.state.setProperty("uiScaleRoot", rootBtn.getIndex(), nullptr);
        apvts.state.setProperty("uiScaleId", presetScaleSelected ? currentScaleId : 0, nullptr);
    }
    void restoreUiState()
    {
        const int savedRoot = (int) apvts.state.getProperty("uiScaleRoot", 0);
        const int savedId = (int) apvts.state.getProperty("uiScaleId", 0);
        rootBtn.setIndex(savedRoot);
        if(savedId > 0 && savedId <= (int) ScaleLibrary::scales().size())
        {
            const auto base = ScaleLibrary::scales()[(size_t) (savedId - 1)].mask;
            const auto wanted = ScaleLibrary::rcirc(base, savedRoot);
            if(currentMask() == wanted)
            {
                currentScaleId = savedId;
                presetScaleSelected = true;
                scaleBtn.setButtonText(ScaleLibrary::scales()[(size_t) (savedId - 1)].name);
                scaleBtn.setActive(true);
                expectedMask = wanted;
                lastSeenMask = wanted;
                writeSettleFrames = 4;
            }
            else
            {
                currentScaleId = 0;
                presetScaleSelected = false;
                apvts.state.setProperty("uiScaleId", 0, nullptr);
            }
        }
    }
    void timerCallback() override
    {
        const bool enabled = ! midiOn();
        rootBtn.setEnabled(enabled);
        scaleBtn.setEnabled(enabled);
        fillClearBtn.setEnabled(enabled);
        fillClearBtn.setButtonText(anyNoteOn() ? "CLEAR" : "FULL");
        if(enabled && presetScaleSelected)
        {
            const juce::uint16 nowMask = currentMask();
            if(writeSettleFrames > 0)
                --writeSettleFrames;
            else if(nowMask != expectedMask && nowMask == lastSeenMask)
                clearPreset();
            lastSeenMask = nowMask;
        }
    }
    juce::AudioProcessorValueTreeState& apvts;
    juce::RangedAudioParameter* note[12] { };
    juce::RangedAudioParameter* midiParam = nullptr;
    KeyButton rootBtn;
    SmoothTextButton scaleBtn, fillClearBtn;
    juce::Label rootLbl, scaleLbl;
    MagentaPopupLookAndFeel popupLnf;
    int currentScaleId = 0;
    bool presetScaleSelected = false;
    juce::uint16 expectedMask = 0;
    juce::uint16 lastSeenMask = 0;
    int writeSettleFrames = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScalePanel)
};
```

### `SecondaryMenu.h`

- Size: 15.41 KB
- Type: text

```h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "SmoothColour.h"
#include <cmath>
#include <functional>
class MenuDotsButton : public juce::Button,
                       private juce::Timer
{
public:
    MenuDotsButton() : juce::Button("secondaryMenuA")
    {
        setWantsKeyboardFocus(false);
        startTimerHz(60);
    }
    ~MenuDotsButton() override { stopTimer(); }
    void setActive(bool a) { active = a; }
private:
    void paintButton(juce::Graphics& g, bool, bool) override
    {
        const auto b = getLocalBounds().toFloat().reduced(1.5f);
        const float corner = juce::jmin(12.0f, b.getWidth() * 0.28f);
        g.setColour(bg.get());
        g.fillRoundedRectangle(b, corner);
        g.setColour(border.get());
        g.drawRoundedRectangle(b, corner, 1.5f);
        const float r = juce::jmax(1.7f, b.getWidth() * 0.052f);
        const float gap = r * 3.2f;
        const float cx = b.getCentreX();
        const float cy = b.getCentreY();
        g.setColour(dot.get());
        for(int i = -1; i <= 1; ++i)
            g.fillEllipse(cx + (float) i * gap - r, cy - r, 2.0f * r, 2.0f * r);
    }
    void timerCallback() override
    {
        const bool over = isOver(), down = isDown();
        const bool on = active;
        const juce::Colour bgT = on ? (down ? juce::Colour(0xff1b2a2d)
                                      : over ? juce::Colour(0xff31474b)
                                             : juce::Colour(0xff263436))
                                    : (down ? juce::Colour(0xff1f1f26)
                                      : over ? juce::Colour(0xff31313c)
                                             : juce::Colour(0xff26262e));
        const juce::Colour borderT = down ? juce::Colour(0xff307a7c)
                                   : over ? juce::Colour(0xff6ecdd0)
                                   : on ? juce::Colour(0xff45aeb1)
                                          : juce::Colour(0xff3a3a46);
        const juce::Colour dotT = down ? juce::Colour(0xff7fc9cc)
                                   : over ? juce::Colour(0xff9fe3e5)
                                   : on ? juce::Colour(0xff9fe3e5)
                                          : juce::Colour(0xff8a8a92);
        if(! primed) { bg.set(bgT); border.set(borderT); dot.set(dotT); primed = true; repaint(); return; }
        bool moving = false;
        moving |= bg.approach(bgT, rate);
        moving |= border.approach(borderT, rate);
        moving |= dot.approach(dotT, rate);
        if(moving) repaint();
    }
    SmoothColour bg, border, dot;
    bool active = false, primed = false;
    const float rate = 1.0f / 3.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MenuDotsButton)
};
class RoundToggle : public juce::Button,
                    private juce::Timer
{
public:
    RoundToggle() : juce::Button("roundToggle")
    {
        setWantsKeyboardFocus(false);
        setClickingTogglesState(true);
        startTimerHz(60);
    }
    ~RoundToggle() override { stopTimer(); }
private:
    void paintButton(juce::Graphics& g, bool, bool) override
    {
        auto b = getLocalBounds().toFloat().reduced(1.0f);
        const float d = juce::jmin(b.getWidth(), b.getHeight());
        const auto circle = juce::Rectangle<float> (d, d).withCentre(b.getCentre());
        g.setColour(fill.get());
        g.fillEllipse(circle);
    }
    void timerCallback() override
    {
        const bool over = isOver(), down = isDown(), on = getToggleState();
        const juce::Colour fillT = on ? (down ? juce::Colour(0xff307a7c)
                                       : over ? juce::Colour(0xff6ecdd0)
                                              : juce::Colour(0xff45aeb1))
                                      : (down ? juce::Colour(0xff5a5a64)
                                       : over ? juce::Colour(0xffb8b8c0)
                                              : juce::Colour(0xff8a8a92));
        if(! primed) { fill.set(fillT); primed = true; repaint(); return; }
        if(fill.approach(fillT, rate)) repaint();
    }
    SmoothColour fill;
    bool primed = false;
    const float rate = 1.0f / 3.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RoundToggle)
};
class FlyoutPanel : public juce::Component,
                    private juce::Timer
{
public:
    FlyoutPanel()
    {
        setVisible(false);
        setAlpha(0.0f);
        setInterceptsMouseClicks(true, true);
        startTimerHz(60);
    }
    ~FlyoutPanel() override { stopTimer(); }
    std::function<void()> onClose;
    std::function<void(bool)> onPinnedChanged;
    bool isOpen() const noexcept { return open; }
    bool isPinned() const noexcept { return pinned; }
    void setPinned(bool p)
    {
        if(p != pinned) { pinned = p; repaint(); }
    }
    bool hitTestPin(juce::Point<float> localPos) const { return pinHitArea().contains(localPos); }
    void prewarmBackdrop(bool forceRefresh = false)
    {
        if(forceRefresh || ! blurredBackdrop.isValid())
            refreshBlurredBackdrop();
    }
    void show()
    {
        if(open) return;
        open = true;
        blurTickCounter = 0;
        setVisible(true);
        toFront(false);
        prewarmBackdrop(true);
    }
    void close()
    {
        if(! open) return;
        open = false;
        blurTickCounter = 0;
        blurredBackdrop = {};
        if(onClose) onClose();
    }
    void toggle() { open ? close() : show(); }
    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(0.75f);
        if(blurredBackdrop.isValid())
        {
            juce::Path clipPath;
            clipPath.addRoundedRectangle(b, 12.0f);
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(clipPath);
            g.drawImageWithin(blurredBackdrop,
                               0, 0, getWidth(), getHeight(),
                               juce::RectanglePlacement::stretchToFit);
        }
        g.setColour(juce::Colour(0xff17171c).withAlpha(0.5f));
        g.fillRoundedRectangle(b, 12.0f);
        g.setColour(juce::Colour(0xff34343e));
        g.drawRoundedRectangle(b, 12.0f, 1.0f);
        drawPin(g);
    }
    void drawPin(juce::Graphics& g)
    {
        const auto area = pinHitArea();
        const float s = juce::jmin(area.getWidth(), area.getHeight());
        const float cx = area.getCentreX();
        const float cy = area.getCentreY();
        const float tilt = 0.38f;
        const float dx = -std::sin(tilt);
        const float dy = std::cos(tilt);
        const float headR = s * 0.155f;
        const juce::Point<float> headC(cx - dx * s * 0.13f, cy - dy * s * 0.13f);
        const juce::Point<float> tip(cx + dx * s * 0.36f, cy + dy * s * 0.36f);
        const juce::Point<float> start(headC.x + dx * headR * 0.5f,
                                        headC.y + dy * headR * 0.5f);
        g.setColour(pinS.get());
        g.fillEllipse(headC.x - headR, headC.y - headR, headR * 2.0f, headR * 2.0f);
        juce::Path shaft;
        shaft.startNewSubPath(start);
        shaft.lineTo(tip);
        g.strokePath(shaft, juce::PathStrokeType(s * 0.09f, juce::PathStrokeType::curved,
                                                              juce::PathStrokeType::rounded));
    }
    void moved() override { blurredBackdrop = {}; }
    void resized() override { blurredBackdrop = {}; }
    void mouseDown(const juce::MouseEvent& e) override
    {
        if(pinHitArea().contains(e.position))
        {
            pinArmed = true;
            pinHover = true;
            repaint();
        }
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if(! pinArmed) return;
        const bool over = pinHitArea().contains(e.position);
        if(over != pinHover) { pinHover = over; repaint(); }
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        const bool commit = pinArmed && pinHitArea().contains(e.position);
        pinArmed = false;
        if(commit)
        {
            pinned = ! pinned;
            if(onPinnedChanged) onPinnedChanged(pinned);
        }
        pinHover = pinHitArea().contains(e.position);
        repaint();
    }
    void mouseMove(const juce::MouseEvent& e) override
    {
        const bool h = pinHitArea().contains(e.position);
        if(h != pinHover) { pinHover = h; repaint(); }
    }
    void mouseEnter(const juce::MouseEvent& e) override { mouseMove(e); }
    void mouseExit(const juce::MouseEvent&) override
    {
        if(pinHover && ! pinArmed) { pinHover = false; repaint(); }
    }
private:
    juce::Rectangle<float> pinHitArea() const { return { 5.0f, 5.0f, 24.0f, 24.0f }; }
    juce::Colour pinTarget() const
    {
        const bool pressed = pinArmed && pinHover;
        if(pinned)
            return pressed ? juce::Colour(0xff307a7c)
                 : pinHover ? juce::Colour(0xff6ecdd0)
                            : juce::Colour(0xff45aeb1);
        return pressed ? juce::Colour(0xff5a5a64)
             : pinHover ? juce::Colour(0xffb8b8c0)
                        : juce::Colour(0xff8a8a92);
    }
    void updatePinColour()
    {
        const auto t = pinTarget();
        if(! pinPrimed) { pinS.set(t); pinPrimed = true; return; }
        if(pinS.approach(t, 1.0f / 3.0f) && open)
            repaint();
    }
    static juce::Image downsampledImage(const juce::Image& src, int downsample)
    {
        const int factor = juce::jmax(1, downsample);
        const int srcW = src.getWidth();
        const int srcH = src.getHeight();
        const int w = juce::jmax(1, (srcW + factor - 1) / factor);
        const int h = juce::jmax(1, (srcH + factor - 1) / factor);
        juce::Image dst(juce::Image::ARGB, w, h, true);
        for(int y = 0; y < h; ++y)
        {
            const int y0 = y * factor;
            const int y1 = juce::jmin(srcH, y0 + factor);
            for(int x = 0; x < w; ++x)
            {
                const int x0 = x * factor;
                const int x1 = juce::jmin(srcW, x0 + factor);
                int a = 0, r = 0, g = 0, b = 0, n = 0;
                for(int sy = y0; sy < y1; ++sy)
                {
                    for(int sx = x0; sx < x1; ++sx)
                    {
                        const auto c = src.getPixelAt(sx, sy);
                        a += (int) c.getAlpha();
                        r += (int) c.getRed();
                        g += (int) c.getGreen();
                        b += (int) c.getBlue();
                        ++n;
                    }
                }
                dst.setPixelAt(x, y, juce::Colour((juce::uint8) (r / n),
                                                    (juce::uint8) (g / n),
                                                    (juce::uint8) (b / n),
                                                    (juce::uint8) (a / n)));
            }
        }
        return dst;
    }
    static void boxBlur1DHorizontal(const juce::Image& src, juce::Image& dst, int radius)
    {
        const int w = src.getWidth();
        const int h = src.getHeight();
        for(int y = 0; y < h; ++y)
        {
            for(int x = 0; x < w; ++x)
            {
                int a = 0, r = 0, g = 0, b = 0, n = 0;
                for(int k = -radius; k <= radius; ++k)
                {
                    const int sx = juce::jlimit(0, w - 1, x + k);
                    const auto c = src.getPixelAt(sx, y);
                    a += (int) c.getAlpha();
                    r += (int) c.getRed();
                    g += (int) c.getGreen();
                    b += (int) c.getBlue();
                    ++n;
                }
                dst.setPixelAt(x, y, juce::Colour((juce::uint8) (r / n),
                                                    (juce::uint8) (g / n),
                                                    (juce::uint8) (b / n),
                                                    (juce::uint8) (a / n)));
            }
        }
    }
    static void boxBlur1DVertical(const juce::Image& src, juce::Image& dst, int radius)
    {
        const int w = src.getWidth();
        const int h = src.getHeight();
        for(int y = 0; y < h; ++y)
        {
            for(int x = 0; x < w; ++x)
            {
                int a = 0, r = 0, g = 0, b = 0, n = 0;
                for(int k = -radius; k <= radius; ++k)
                {
                    const int sy = juce::jlimit(0, h - 1, y + k);
                    const auto c = src.getPixelAt(x, sy);
                    a += (int) c.getAlpha();
                    r += (int) c.getRed();
                    g += (int) c.getGreen();
                    b += (int) c.getBlue();
                    ++n;
                }
                dst.setPixelAt(x, y, juce::Colour((juce::uint8) (r / n),
                                                    (juce::uint8) (g / n),
                                                    (juce::uint8) (b / n),
                                                    (juce::uint8) (a / n)));
            }
        }
    }
    static void fastBoxBlur(juce::Image& img, int radius, int passes)
    {
        if(! img.isValid() || radius <= 0 || passes <= 0)
            return;
        juce::Image temp(juce::Image::ARGB, img.getWidth(), img.getHeight(), true);
        for(int i = 0; i < passes; ++i)
        {
            boxBlur1DHorizontal(img, temp, radius);
            boxBlur1DVertical(temp, img, radius);
        }
    }
    void refreshBlurredBackdrop()
    {
        blurredBackdrop = {};
        auto* parent = getParentComponent();
        if(parent == nullptr)
            return;
        const auto areaInParent = getBounds();
        if(areaInParent.isEmpty())
            return;
        const float oldAlpha = getAlpha();
        setAlpha(0.0f);
        auto snapshot = parent->createComponentSnapshot(areaInParent);
        setAlpha(oldAlpha);
        if(! snapshot.isValid())
            return;
        auto low = downsampledImage(snapshot, blurDownsampleFactor);
        fastBoxBlur(low, blurRadiusLowRes, blurPasses);
        blurredBackdrop = std::move(low);
    }
    void timerCallback() override
    {
        updatePinColour();
        if(open)
        {
            if(++blurTickCounter >= blurRefreshEveryTicks)
            {
                blurTickCounter = 0;
                refreshBlurredBackdrop();
                repaint();
            }
        }
        const float target = open ? 1.0f : 0.0f;
        const float d = target - anim;
        if(std::abs(d) < 0.004f)
        {
            if(! open && isVisible())
            {
                anim = 0.0f;
                applyAnim();
                setVisible(false);
            }
            return;
        }
        anim += d * (1.0f / 5.0f);
        applyAnim();
    }
    void applyAnim()
    {
        setAlpha(anim);
    }
    bool open = false;
    float anim = 0.0f;
    int blurTickCounter = 0;
    static constexpr int blurRefreshEveryTicks = 2;
    static constexpr int blurDownsampleFactor = 4;
    static constexpr int blurRadiusLowRes = 2;
    static constexpr int blurPasses = 3;
    juce::Image blurredBackdrop;
    bool pinned = false;
    bool pinHover = false;
    bool pinArmed = false;
    bool pinPrimed = false;
    SmoothColour pinS;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FlyoutPanel)
};
```

### `SidechainDetector.h`

- Size: 12.75 KB
- Type: text

```h
#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>
#include <atomic>
#include "SpectrumBridge.h"
template <int MaxBins, int MaxBases>
class SidechainDetector
{
public:
    using BridgeType = SpectrumBridge<MaxBins, MaxBases>;
    struct Params
    {
        float fMin = 20.0f;
        float fMax = 20000.0f;
        float thr = 0.5f;
        bool moreBases = false;
        float pitchRatio = 1.0f;
    };
    void prepare(int maxFftSize)
    {
        scFifo.assign((size_t) maxFftSize, 0.0f);
        fftData.assign((size_t) (2 * maxFftSize), 0.0f);
        pvMag.assign((size_t) MaxBins, 0.0f);
        pvFreq.assign((size_t) MaxBins, 0.0f);
        pvPhase.assign((size_t) MaxBins, 0.0f);
        prevPhase.assign((size_t) MaxBins, 0.0f);
        peakFreq.assign((size_t) maxPeaks, 0.0f);
        peakAmp.assign((size_t) maxPeaks, 0.0f);
        peakWork.assign((size_t) maxPeaks, 0.0f);
        baseFreq.assign((size_t) MaxBases, 0.0f);
        baseSal.assign((size_t) MaxBases, 0.0f);
        baseConf.assign((size_t) MaxBases, 0.0f);
        analysisSeed = true;
        prevPrimary = 0.0f;
        numPeaks = numBases = 0;
    }
    void configure(juce::dsp::FFT* engine, const float* windowPtr,
                    int fftSize_, int hopSize_, int numBins_,
                    float binWidth_, double sampleRate_, float spectrumNorm_)
    {
        fft = engine;
        window = windowPtr;
        fftSize = fftSize_;
        fftMask = fftSize_ - 1;
        hopSize = hopSize_;
        numBins = numBins_;
        binWidth = binWidth_;
        sampleRate = sampleRate_;
        spectrumNorm = spectrumNorm_;
        std::fill(scFifo.begin(), scFifo.end(), 0.0f);
        std::fill(prevPhase.begin(), prevPhase.end(), 0.0f);
        analysisSeed = true;
        prevPrimary = 0.0f;
        numPeaks = numBases = 0;
    }
    void pushSample(int pos, float s) noexcept
    {
        scFifo[(size_t) pos] = s;
    }
    void processHop(int pos, const Params& p) noexcept
    {
        if(fft == nullptr || window == nullptr || numBins <= 0)
            return;
        float* fd = fftData.data();
        for(int i = 0; i < fftSize; ++i)
            fd[i] = scFifo[(size_t) ((pos + i) & fftMask)] * window[i];
        fft->performRealOnlyForwardTransform(fd, false);
        analyze();
        detect(p);
        publish(p.pitchRatio);
    }
    void processBypassHop(int pos) noexcept
    {
        if(fft == nullptr || window == nullptr || numBins <= 0)
            return;
        float* fd = fftData.data();
        for(int i = 0; i < fftSize; ++i)
            fd[i] = scFifo[(size_t) ((pos + i) & fftMask)] * window[i];
        fft->performRealOnlyForwardTransform(fd, false);
        analyze();
        numBases = 0;
        lastNumBases.store(0, std::memory_order_relaxed);
        publishBypassed();
    }
    BridgeType& getBridge() noexcept { return bridge; }
    int debugNumBases() const noexcept { return lastNumBases.load(std::memory_order_relaxed); }
    const std::atomic<int>& numBasesAtomic() const noexcept { return lastNumBases; }
    int copyTargets(float* dst, int maxDst) const noexcept
    {
        const int n = juce::jmin(numBases, maxDst);
        for(int i = 0; i < n; ++i) dst[i] = baseFreq[(size_t) i];
        return n;
    }
    void publishGatedFrame(bool bypassed) noexcept
    {
        auto& s = bridge.startWrite();
        s.numBins = numBins;
        s.binWidth = binWidth;
        s.sampleRate = sampleRate;
        s.pvBypassed = bypassed;
        const int nb = juce::jmin(numBins, (int) s.mag.size());
        for(int j = 0; j < nb; ++j)
        {
            s.mag [(size_t) j] = pvMag [(size_t) j] * spectrumNorm;
            s.freq[(size_t) j] = pvFreq[(size_t) j];
        }
        s.numBases = 0;
        bridge.publish();
    }
private:
    static constexpr int maxPeaks = 128;
    static constexpr int harmonicsMax = 32;
    static constexpr float centsTol = 45.0f;
    static constexpr float salAlpha = 52.0f;
    static constexpr float salBeta = 320.0f;
    static constexpr float octaveBias = 0.80f;
    static constexpr float continuityBias = 0.30f;
    static constexpr float peakFloorRel = 0.02f;
    static constexpr float silenceMagFloor = 1.0e-4f;
    static constexpr float tonalRefScale = 0.35f;
    static constexpr int defaultMaxPeaks = 64;
    static constexpr int defaultMaxBases = 16;
    static float wrapPhase(float x) noexcept
    {
        return x - juce::MathConstants<float>::twoPi
                   * std::round(x / juce::MathConstants<float>::twoPi);
    }
    void analyze() noexcept
    {
        float* fd = fftData.data();
        const float twoPi = juce::MathConstants<float>::twoPi;
        const float expectPerBin = twoPi * (float) hopSize / (float) fftSize;
        const float freqScale = (float) (sampleRate / (twoPi * hopSize));
        for(int k = 0; k < numBins; ++k)
        {
            const float re = fd[2 * k];
            const float im = fd[2 * k + 1];
            pvPhase[(size_t) k] = std::atan2 (im, re);
            pvMag [(size_t) k] = std::sqrt(re * re + im * im);
        }
        if(analysisSeed)
        {
            for(int k = 0; k < numBins; ++k)
                prevPhase[(size_t) k] = wrapPhase(pvPhase[(size_t) k] - expectPerBin * (float) k);
            analysisSeed = false;
        }
        for(int k = 0; k < numBins; ++k)
        {
            const float dev = wrapPhase((pvPhase[(size_t) k] - prevPhase[(size_t) k]) - expectPerBin * (float) k);
            pvFreq[(size_t) k] = (float) k * binWidth + dev * freqScale;
            prevPhase[(size_t) k] = pvPhase[(size_t) k];
        }
    }
    float salience(float F) const noexcept
    {
        if(F <= 0.0f) return 0.0f;
        const float nyq = (float) (sampleRate * 0.5);
        float sal = 0.0f;
        for(int n = 1; n <= harmonicsMax; ++n)
        {
            const float target = (float) n * F;
            if(target >= nyq) break;
            float best = 0.0f;
            for(int i = 0; i < numPeaks; ++i)
            {
                if(peakWork[(size_t) i] <= 0.0f) continue;
                const float cents = 1200.0f * std::log2 (peakFreq[(size_t) i] / target);
                if(std::abs(cents) < centsTol)
                    best = std::max(best, peakWork[(size_t) i]);
            }
            sal += ((F + salAlpha) / (target + salBeta)) * best;
        }
        return sal;
    }
    void detect(const Params& p) noexcept
    {
        numPeaks = 0;
        numBases = 0;
        const int peakLimit = p.moreBases ? maxPeaks : defaultMaxPeaks;
        const int baseLimit = p.moreBases ? MaxBases : defaultMaxBases;
        const float salienceGateScale = p.moreBases ? 0.85f : 1.0f;
        const float gatedThr = juce::jmax(0.01f, p.thr * salienceGateScale);
        float maxMag = 0.0f;
        for(int k = 0; k < numBins; ++k) maxMag = std::max(maxMag, pvMag[(size_t) k]);
        if(maxMag < silenceMagFloor) { lastNumBases.store(0, std::memory_order_relaxed); return; }
        const float floorMag = maxMag * peakFloorRel;
        for(int k = 2; k + 2 < numBins && numPeaks < peakLimit; ++k)
        {
            const float m = pvMag[(size_t) k];
            if(m > floorMag
                && m > pvMag[(size_t) (k - 1)] && m > pvMag[(size_t) (k - 2)]
                && m >= pvMag[(size_t) (k + 1)] && m >= pvMag[(size_t) (k + 2)])
            {
                peakFreq[(size_t) numPeaks] = pvFreq[(size_t) k] * p.pitchRatio;
                peakAmp [(size_t) numPeaks] = m;
                ++numPeaks;
            }
        }
        if(numPeaks == 0) { lastNumBases.store(0, std::memory_order_relaxed); return; }
        float totalPeak = 0.0f;
        for(int i = 0; i < numPeaks; ++i)
        {
            peakWork[(size_t) i] = peakAmp[(size_t) i];
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
                if(peakWork[(size_t) c] <= 0.0f) continue;
                if(Fc < p.fMin || Fc > p.fMax) continue;
                const float sal = salience(Fc);
                float score = sal;
                if(numBases == 0 && prevPrimary > 0.0f
                    && std::abs(1200.0f * std::log2 (Fc / prevPrimary)) < centsTol)
                    score *= (1.0f + continuityBias);
                if(score > bestScore) { bestScore = score; bestSal = sal; bestF = Fc; bestIdx = c; }
            }
            if(bestIdx < 0) break;
            bool improved = true;
            while(improved)
            {
                improved = false;
                const float halfF = bestF * 0.5f;
                if(halfF >= p.fMin)
                    for(int c = 0; c < numPeaks; ++c)
                    {
                        if(peakWork[(size_t) c] <= 0.0f) continue;
                        const float fc = peakFreq[(size_t) c];
                        if(std::abs(1200.0f * std::log2 (fc / halfF)) < centsTol)
                        {
                            const float subSal = salience(fc);
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
            for(int i = 0; i < numPeaks; ++i)
            {
                if(peakWork[(size_t) i] <= 0.0f) continue;
                const int n = (int) std::lround(peakFreq[(size_t) i] / bestF);
                if(n < 1 || n > harmonicsMax) continue;
                const float cents = 1200.0f * std::log2 (peakFreq[(size_t) i] / ((float) n * bestF));
                if(std::abs(cents) < centsTol) peakWork[(size_t) i] = 0.0f;
            }
        }
        if(numBases == 0) { lastNumBases.store(0, std::memory_order_relaxed); return; }
        prevPrimary = baseFreq[0];
        const float tonal = juce::jlimit(0.0f, 1.0f,
                                          firstSal / (tonalRefScale * (totalPeak + 1.0e-12f)));
        for(int b = 0; b < numBases; ++b)
        {
            const float rel = (firstSal > 0.0f)
                                ? juce::jlimit(0.0f, 1.0f, baseSal[(size_t) b] / firstSal) : 0.0f;
            baseConf[(size_t) b] = rel * tonal;
        }
        lastNumBases.store(numBases, std::memory_order_relaxed);
    }
    void publish(float pitchRatio) noexcept
    {
        auto& s = bridge.startWrite();
        s.numBins = numBins;
        s.binWidth = binWidth;
        s.sampleRate = sampleRate;
        s.pvBypassed = false;
        const int nb = juce::jmin(numBins, (int) s.mag.size());
        for(int j = 0; j < nb; ++j)
        {
            s.mag [(size_t) j] = pvMag [(size_t) j] * spectrumNorm;
            s.freq[(size_t) j] = pvFreq[(size_t) j] * pitchRatio;
        }
        const int nbases = juce::jmin(numBases, (int) s.baseHz.size());
        for(int i = 0; i < nbases; ++i)
        {
            s.baseHz [(size_t) i] = baseFreq[(size_t) i];
            s.baseConf[(size_t) i] = baseConf[(size_t) i];
        }
        s.numBases = nbases;
        bridge.publish();
    }
    void publishBypassed() noexcept
    {
        auto& s = bridge.startWrite();
        s.numBins = numBins;
        s.binWidth = binWidth;
        s.sampleRate = sampleRate;
        s.pvBypassed = true;
        const int nb = juce::jmin(numBins, (int) s.mag.size());
        for(int j = 0; j < nb; ++j)
        {
            s.mag [(size_t) j] = pvMag [(size_t) j] * spectrumNorm;
            s.freq[(size_t) j] = pvFreq[(size_t) j];
        }
        s.numBases = 0;
        bridge.publish();
    }
    juce::dsp::FFT* fft = nullptr;
    const float* window = nullptr;
    int fftSize = 0;
    int fftMask = 0;
    int hopSize = 0;
    int numBins = 0;
    float binWidth = 0.0f;
    float spectrumNorm = 1.0f;
    double sampleRate = 44100.0;
    std::vector<float> scFifo;
    std::vector<float> fftData;
    std::vector<float> pvMag, pvFreq, pvPhase, prevPhase;
    std::vector<float> peakFreq, peakAmp, peakWork;
    std::vector<float> baseFreq, baseSal, baseConf;
    int numPeaks = 0;
    int numBases = 0;
    float prevPrimary = 0.0f;
    bool analysisSeed = true;
    std::atomic<int> lastNumBases { 0 };
    BridgeType bridge;
};
```

### `SmoothColour.h`

- Size: 1.13 KB
- Type: text

```h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
struct SmoothColour
{
    float r = 0, g = 0, b = 0, a = 1;
    void set(juce::Colour c)
    {
        r = c.getFloatRed(); g = c.getFloatGreen();
        b = c.getFloatBlue(); a = c.getFloatAlpha();
    }
    juce::Colour get() const { return juce::Colour::fromFloatRGBA(r, g, b, a); }
    bool approach(juce::Colour c, float rate)
    {
        const float dr = c.getFloatRed() - r, dg = c.getFloatGreen() - g,
                    db = c.getFloatBlue() - b, da = c.getFloatAlpha() - a;
        r += dr * rate; g += dg * rate; b += db * rate; a += da * rate;
        return std::abs(dr) + std::abs(dg) + std::abs(db) + std::abs(da) > 0.002f;
    }
};
struct SmoothValue
{
    double v = 0.0;
    bool primed = false;
    void prime(double target) { v = target; primed = true; }
    bool approach(double target, double rate, double eps)
    {
        if(! primed) { v = target; primed = true; return false; }
        const double d = target - v;
        v += d * rate;
        return std::abs(d) > eps;
    }
    double get() const { return v; }
};
```

### `SmoothComboBox.h`

- Size: 2.91 KB
- Type: text

```h
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
```

### `SmoothTextButton.h`

- Size: 4.63 KB
- Type: text

```h
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
```

### `SmoothToggle.h`

- Size: 3.94 KB
- Type: text

```h
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
```

### `SpectrumBridge.h`

- Size: 1.74 KB
- Type: text

```h
#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
template <int MaxBins, int MaxBases>
struct SpectrumSnapshot
{
    int numBins = 0;
    float binWidth = 0.0f;
    double sampleRate = 44100.0;
    int numBases = 0;
    bool pvBypassed = false;
    std::array<float, MaxBins> mag {};
    std::array<float, MaxBins> freq {};
    std::array<float, MaxBases> baseHz {};
    std::array<float, MaxBases> baseConf {};
};
template <int MaxBins, int MaxBases>
class SpectrumBridge
{
public:
    using Snapshot = SpectrumSnapshot<MaxBins, MaxBases>;
    Snapshot& startWrite() noexcept
    {
        const auto s = sequence.load(std::memory_order_relaxed);
        sequence.store(s + 1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        return buffer;
    }
    void publish() noexcept
    {
        std::atomic_thread_fence(std::memory_order_release);
        sequence.fetch_add(1, std::memory_order_relaxed);
    }
    bool read(Snapshot& out) noexcept
    {
        for(;;)
        {
            const auto s1 = sequence.load(std::memory_order_acquire);
            if(s1 & 1u)
                continue;
            std::atomic_thread_fence(std::memory_order_acquire);
            out = buffer;
            std::atomic_thread_fence(std::memory_order_acquire);
            const auto s2 = sequence.load(std::memory_order_relaxed);
            if(s1 == s2)
            {
                const bool isNew = (s1 != lastSeenSequence);
                lastSeenSequence = s1;
                return isNew;
            }
        }
    }
private:
    Snapshot buffer {};
    std::atomic<std::uint32_t> sequence { 0 };
    std::uint32_t lastSeenSequence = 0;
};
```

### `SpectrumDisplay.h`

- Size: 20.29 KB
- Type: text

```h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "SmoothColour.h"
#include "Oklch.h"
#include "KnobLookAndFeel.h"
#include <cmath>
#include <cstdlib>
#include <array>
#include <atomic>
class SpectrumDisplay : public juce::Component,
                        private juce::Timer
{
public:
    SpectrumDisplay(juce::AudioProcessorValueTreeState& state,
                     NewProjectAudioProcessor::SpectrumBridgeType& bridgeIn)
        : apvts(state), bridge(bridgeIn)
    {
        centerParam = apvts.getParameter("detectCenter");
        spreadParam = apvts.getParameter("detectSpread");
        attractionParam = apvts.getParameter("attraction");
        bypassParam = apvts.getParameter("pvBypass");
        midiParam = apvts.getParameter("midiMode");
        targetModeParam = apvts.getParameter("targetMode");
        for(int i = 0; i < 12; ++i)
            noteParam[i] = apvts.getParameter("note" + juce::String(i));
        heatImage = juce::Image(juce::Image::ARGB, imageWidth, 1, true);
        setWantsKeyboardFocus(true);
        startTimerHz(60);
    }
    ~SpectrumDisplay() override { stopTimer(); }
    void setMidiMask(const std::atomic<int>* m) { midiMask = m; }
    void setSidechainTargetCount(const std::atomic<int>* c) { scTargetCount = c; }
    void setForceTargetColour(bool b) { forceTargetColour = b; }
    void paint(juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        const float W = r.getWidth();
        const float H = r.getHeight();
        g.fillAll(juce::Colour(0xff0a0a0c));
        g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
        g.drawImageWithin(heatImage, 0, 0, getWidth(), getHeight(),
                           juce::RectanglePlacement::stretchToFit);
        const float centerHz = dispCenterHz();
        const float spread = dispSpreadVal();
        const float lowHz = centerHz / std::pow(2.0f, spread);
        const float highHz = centerHz * std::pow(2.0f, spread);
        const float xLow = freqToX(lowHz, W);
        const float xHigh = freqToX(highHz, W);
        const juce::Colour thresholdCol = thresholdLineColour();
        drawInwardGlow(g, xLow, xHigh, H, thresholdCol);
        drawInwardGlow(g, xHigh, xLow, H, thresholdCol);
        g.setColour(thresholdCol.withAlpha(thresholdLineAlpha));
        g.drawVerticalLine((int) std::round(xLow), 0.0f, H);
        g.drawVerticalLine((int) std::round(xHigh), 0.0f, H);
        const juce::Colour baseCol = baseLineColour();
        const float stripAlphaScale = 1.0f - bypassVisualAmt;
        for(int i = 0; i < snapshot.numBases; ++i)
        {
            const float f = snapshot.baseHz[(size_t) i];
            const float conf = snapshot.baseConf[(size_t) i];
            if(f <= 0.0f || conf < 0.05f) continue;
            if(stripAlphaScale < 0.02f)
                continue;
            const float a = juce::jlimit(0.0f, 1.0f, conf * stripAlphaScale);
            const float x = freqToX(f, W);
            for(int gx = 3; gx >= 1; --gx)
            {
                g.setColour(baseCol.withAlpha(0.12f * (float) gx * a));
                g.fillRect(x - (float) gx, 0.0f, 2.0f * (float) gx, H);
            }
            g.setColour(baseCol.withAlpha(a));
            g.drawVerticalLine((int) std::round(x), 0.0f, H);
        }
        const float cx = freqToX(centerHz, W);
        const float cy = spreadToY(spread, H);
        const float rad = 9.0f * 0.67f;
        const float stroke = rad * 0.33f;
        const float haloPad = rad * 0.45f;
        const float fillAlpha = juce::jlimit(0.0f, 1.0f, dragFillAmt);
        const float ringAlpha = 1.0f - fillAlpha;
        if(selected != Handle::none)
        {
            g.setColour(juce::Colours::white.withAlpha(0.25f));
            g.fillEllipse(cx - rad - haloPad, cy - rad - haloPad,
                           2.0f * (rad + haloPad), 2.0f * (rad + haloPad));
        }
        if(fillAlpha > 0.001f)
        {
            g.setColour(juce::Colour(0xfff0f0f0).withAlpha(fillAlpha));
            g.fillEllipse(cx - rad, cy - rad, 2.0f * rad, 2.0f * rad);
        }
        if(ringAlpha > 0.001f)
        {
            g.setColour(juce::Colour(0xfff0f0f0).withAlpha(ringAlpha));
            g.drawEllipse(cx - rad, cy - rad, 2.0f * rad, 2.0f * rad, stroke);
        }
        if(selected != Handle::none)
        {
            g.setColour(juce::Colours::black.withAlpha(0.7f));
            g.fillRect(4.0f, 4.0f, 168.0f, 22.0f);
            g.setColour(juce::Colours::white);
            g.setFont(KnobLookAndFeel::courier(KnobLookAndFeel::uiFont));
            const juce::String label = (selected == Handle::center ? "centre " : "spread ");
            g.drawText(label + entryBuffer + "_",
                        8, 4, 160, 22, juce::Justification::centredLeft);
        }
    }
    float coordOverlayGetAlpha() const noexcept { return coordOverlayAlpha; }
    juce::String coordOverlayGetText() const
    {
        return "(" + juce::String(dispCenterHz(), 1) + " Hz, "
                   + juce::String(dispSpreadVal(), 2) + " spread)";
    }
    void mouseEnter(const juce::MouseEvent&) override
    {
        mouseOverSpectrum = true;
        scheduleCoordOverlayShow();
    }
    void mouseExit(const juce::MouseEvent&) override
    {
        mouseOverSpectrum = false;
        coordOverlayTargetAlpha = 0.0f;
        ++coordOverlayToken;
        repaint();
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        if(e.mods.isPopupMenu())
        {
            juce::PopupMenu menu;
            menu.addItem(1, "Type value: Centre", true, selected == Handle::center);
            menu.addItem(2, "Type value: Spread", true, selected == Handle::spread);
            menu.setLookAndFeel(&getLookAndFeel());
            menu.showMenuAsync(juce::PopupMenu::Options{}
                                    .withTargetComponent(this)
                                    .withStandardItemHeight(26),
                                [this] (int choice)
            {
                if(choice == 1) { selected = Handle::center; typing = true; }
                else if(choice == 2) { selected = Handle::spread; typing = true; }
                entryBuffer.clear();
                repaint();
            });
            return;
        }
        if(typing || selected != Handle::none)
        {
            typing = false;
            selected = Handle::none;
            entryBuffer.clear();
            repaint();
        }
        dragging = true;
        anchorCenter = getCenterHz();
        anchorSpread = getSpread();
        anchorPos = e.position;
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if(! dragging) return;
        const float W = (float) getWidth();
        const float H = (float) getHeight();
        const float dx = e.position.x - anchorPos.x;
        const float dy = e.position.y - anchorPos.y;
        setCenterHz(xToFreq(freqToX(anchorCenter, W) + dx, W));
        setSpread(yToSpread(spreadToY(anchorSpread, H) + dy, H));
        revealCoordOverlay();
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override { dragging = false; }
    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        const bool both = (selected == Handle::none);
        if(both || selected == Handle::center) resetParam(centerParam);
        if(both || selected == Handle::spread) resetParam(spreadParam);
        revealCoordOverlay();
        repaint();
    }
    bool keyPressed(const juce::KeyPress& key) override
    {
        if(selected == Handle::none) return false;
        const auto c = key.getTextCharacter();
        if(key == juce::KeyPress::returnKey)
        {
            commitEntry();
            return true;
        }
        if(key == juce::KeyPress::escapeKey)
        {
            typing = false;
            selected = Handle::none;
            entryBuffer.clear();
            repaint();
            return true;
        }
        if(key == juce::KeyPress::backspaceKey)
        {
            if(entryBuffer.isNotEmpty())
                entryBuffer = entryBuffer.dropLastCharacters(1);
            typing = true; repaint();
            return true;
        }
        if((c >= '0' && c <= '9') || c == '.' || c == '-')
        {
            entryBuffer += juce::String::charToString(c);
            typing = true; repaint();
            return true;
        }
        return false;
    }
    void focusLost(juce::Component::FocusChangeType) override
    {
        if(typing || selected != Handle::none)
        {
            typing = false;
            selected = Handle::none;
            entryBuffer.clear();
            repaint();
        }
    }
private:
    enum class Handle { none, center, spread };
    using Snapshot = NewProjectAudioProcessor::SpectrumBridgeType::Snapshot;
    static constexpr int imageWidth = 1024;
    static constexpr float fMin = 20.0f;
    static constexpr float minSpread = 0.25f;
    static constexpr float maxSpread = 5.0f;
    static constexpr float thresholdLineAlpha = 0.5f;
    static constexpr float thresholdGlowAlpha = 0.14f;
    static constexpr float smoothRateThird = 1.0f / 3.0f;
    void timerCallback() override
    {
        const bool isNew = bridge.read(snapshot);
        if(isNew)
        {
            fMax = (snapshot.sampleRate > 0.0)
                     ? juce::jmin(20000.0f, (float) (snapshot.sampleRate * 0.5))
                     : 20000.0f;
            rebuildImage();
        }
        const float tgtC = getCenterHz();
        const float tgtS = getSpread();
        if(! dispPrimed) { dispCenter = tgtC; dispSpread = tgtS; dispPrimed = true; }
        else
        {
            dispCenter += (tgtC - dispCenter) * (1.0f / 1.4f);
            dispSpread += (tgtS - dispSpread) * (1.0f / 1.4f);
        }
        const float tgtThresholdBlend = targetAttractionBlend();
        if(! thresholdBlendPrimed)
        {
            thresholdBlendAmt = tgtThresholdBlend;
            thresholdBlendPrimed = true;
        }
        else
        {
            thresholdBlendAmt += (tgtThresholdBlend - thresholdBlendAmt) * smoothRateThird;
        }
        const bool bypassed = (bypassParam == nullptr || bypassParam->getValue() < 0.5f);
        const float tgtBypass = bypassed ? 1.0f : 0.0f;
        if(! bypassVisualPrimed)
        {
            bypassVisualAmt = tgtBypass;
            bypassVisualPrimed = true;
        }
        else
        {
            bypassVisualAmt += (tgtBypass - bypassVisualAmt) * smoothRateThird;
        }
        dragFillAmt += ((dragging ? 1.0f : 0.0f) - dragFillAmt) * smoothRateThird;
        coordOverlayAlpha += (coordOverlayTargetAlpha - coordOverlayAlpha) * smoothRateThird;
        coordOverlayAlpha = juce::jlimit(0.0f, 1.0f, coordOverlayAlpha);
        repaint();
    }
    void rebuildImage()
    {
        std::array<float, imageWidth> peak {};
        peak.fill(0.0f);
        for(int j = 0; j < snapshot.numBins; ++j)
        {
            const float f = snapshot.freq[(size_t) j];
            if(f <= 0.0f) continue;
            const float pos = freqPos(f);
            if(pos < 0.0f || pos > 1.0f) continue;
            const int px = juce::jlimit(0, imageWidth - 1,
                                         (int) std::lround(pos * (imageWidth - 1)));
            peak[(size_t) px] = std::max(peak[(size_t) px], snapshot.mag[(size_t) j]);
        }
        std::array<float, imageWidth> bright {};
        for(int x = 0; x < imageWidth; ++x)
        {
            const float amp = juce::jmax(0.0f, peak[(size_t) x]);
            const float db = 20.0f * std::log10 (juce::jmax(amp, 1.0e-9f));
            const float norm = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
            bright[(size_t) x] = norm * std::sqrtf(norm);
        }
        constexpr int R = 8;
        constexpr float sigma = 4.0f;
        constexpr float glowExp = 3.0f;
        constexpr float glowGain = 0.6f;
        std::array<float, (size_t) (2 * R + 1)> kernel {};
        for(int k = -R; k <= R; ++k)
            kernel[(size_t) (k + R)] = std::exp(-(float) (k * k) / (2.0f * sigma * sigma));
        std::array<float, imageWidth> src {};
        for(int x = 0; x < imageWidth; ++x)
            src[(size_t) x] = std::pow(bright[(size_t) x], glowExp);
        juce::Image::BitmapData bd(heatImage, juce::Image::BitmapData::writeOnly);
        for(int x = 0; x < imageWidth; ++x)
        {
            float glow = 0.0f;
            for(int k = -R; k <= R; ++k)
            {
                const int sx = x + k;
                if(sx < 0 || sx >= imageWidth) continue;
                glow += kernel[(size_t) (k + R)] * src[(size_t) sx];
            }
            const float g = juce::jlimit(0.0f, 1.0f, glowGain * glow);
            const float b = bright[(size_t) x];
            const float lit = b + (1.0f - b) * g;
            const auto v = (juce::uint8) juce::jlimit(0, 255, (int) std::lround(lit * 255.0f));
            bd.setPixelColour(x, 0, juce::Colour(v, v, v));
        }
    }
    void drawInwardGlow(juce::Graphics& g, float edgeX, float towardX, float H,
                         juce::Colour colour)
    {
        const float band = std::abs(towardX - edgeX);
        const float dir = (towardX > edgeX) ? 1.0f : -1.0f;
        const float glowW = juce::jmin(44.0f, band * 0.5f);
        const float intensity = juce::jmin(thresholdGlowAlpha, thresholdLineAlpha)
                      * juce::jlimit(0.0f, 1.0f, band / 80.0f);
        if(glowW < 1.0f || intensity <= 0.001f) return;
        const float x0 = edgeX;
        const float x1 = edgeX + dir * glowW;
        juce::ColourGradient grad(colour.withAlpha(intensity), x0, 0.0f,
                                   colour.withAlpha(0.0f), x1, 0.0f, false);
        g.setGradientFill(grad);
        g.fillRect(juce::Rectangle<float> (juce::Point<float> (std::min(x0, x1), 0.0f),
                                            juce::Point<float> (std::max(x0, x1), H)));
    }
    float freqPos(float f) const
    {
        f = juce::jlimit(fMin, fMax, f);
        return std::log(f / fMin) / std::log(fMax / fMin);
    }
    float freqToX(float f, float W) const { return juce::jlimit(0.0f, 1.0f, freqPos(f)) * W; }
    float xToFreq(float x, float W) const
    {
        const float pos = juce::jlimit(0.0f, 1.0f, x / W);
        return fMin * std::pow(fMax / fMin, pos);
    }
    float spreadToY(float s, float H) const
    {
        const float t = (juce::jlimit(minSpread, maxSpread, s) - minSpread) / (maxSpread - minSpread);
        return H - t * H;
    }
    float yToSpread(float y, float H) const
    {
        const float t = juce::jlimit(0.0f, 1.0f, 1.0f - y / H);
        return minSpread + t * (maxSpread - minSpread);
    }
    float getCenterHz() const
    {
        return centerParam != nullptr
                 ? centerParam->convertFrom0to1 (centerParam->getValue()) : 640.0f;
    }
    float getSpread() const
    {
        return spreadParam != nullptr
                 ? spreadParam->convertFrom0to1 (spreadParam->getValue()) : 2.5f;
    }
    void setCenterHz(float hz) { setParam(centerParam, hz); }
    void setSpread(float s) { setParam(spreadParam, s); }
    float dispCenterHz() const { return dispPrimed ? dispCenter : getCenterHz(); }
    float dispSpreadVal() const { return dispPrimed ? dispSpread : getSpread(); }
    static void setParam(juce::RangedAudioParameter* p, float realValue)
    {
        if(p == nullptr) return;
        const auto& range = p->getNormalisableRange();
        const float v = range.snapToLegalValue(realValue);
        p->setValueNotifyingHost(range.convertTo0to1 (v));
    }
    static void resetParam(juce::RangedAudioParameter* p)
    {
        if(p != nullptr) p->setValueNotifyingHost(p->getDefaultValue());
    }
    void commitEntry()
    {
        float value = 0.0f;
        if(parseFloat(entryBuffer, value))
        {
            setParam(selected == Handle::center ? centerParam : spreadParam, value);
            revealCoordOverlay();
        }
        typing = false;
        selected = Handle::none;
        entryBuffer.clear();
        repaint();
    }
    void scheduleCoordOverlayShow()
    {
        const int token = ++coordOverlayToken;
        juce::Timer::callAfterDelay(250, [safe = juce::Component::SafePointer<SpectrumDisplay> (this), token]
        {
            if(safe == nullptr)
                return;
            if(token != safe->coordOverlayToken)
                return;
            if(! safe->mouseOverSpectrum)
                return;
            safe->coordOverlayTargetAlpha = 1.0f;
            safe->repaint();
        });
    }
    void revealCoordOverlay()
    {
        coordOverlayAlpha = 1.0f;
        coordOverlayTargetAlpha = 1.0f;
        repaint();
    }
    static bool parseFloat(const juce::String& text, float& out)
    {
        const auto s = text.trim();
        if(s.isEmpty()) return false;
        const auto utf8 = s.toRawUTF8();
        char* end = nullptr;
        const double v = std::strtod(utf8, &end);
        if(end == utf8 || *end != '\0') return false;
        out = (float) v;
        return true;
    }
    bool midiOn() const { return targetModeParam != nullptr
                              && juce::roundToInt(targetModeParam->getValue() * 2.0f) == 1; }
    bool anyNoteSelected() const
    {
        if(midiOn() && midiMask != nullptr)
            return midiMask->load() != 0;
        for(auto* p : noteParam)
            if(p != nullptr && p->getValue() >= 0.5f)
                return true;
        return false;
    }
    juce::Colour baseLineColour() const
    {
        return oklch::lerp(juce::Colour(0xff45aeb1), juce::Colour(0xffeb8fff),
                            targetAttractionBlend());
    }
    juce::Colour thresholdLineColour() const
    {
        const auto activeCol = oklch::lerp(juce::Colour(0xff45aeb1), juce::Colour(0xffeb8fff),
                                            juce::jlimit(0.0f, 1.0f, thresholdBlendAmt));
        return oklch::lerp(activeCol, juce::Colour(0xff8a8a92), juce::jlimit(0.0f, 1.0f, bypassVisualAmt));
    }
    float targetAttractionBlend() const
    {
        if(forceTargetColour) return 1.0f;
        const float attract = attractionParam != nullptr ? attractionParam->getValue() : 0.0f;
        const bool hasTarget = sidechainModeOn() ? sidechainHasTargets()
                                                 : anyNoteSelected();
        return hasTarget ? attract * attract : 0.0f;
    }
    bool sidechainModeOn() const
    {
        return targetModeParam != nullptr
            && juce::roundToInt(targetModeParam->getValue() * 2.0f) == 2;
    }
    bool sidechainHasTargets() const
    {
        return scTargetCount != nullptr && scTargetCount->load() > 0;
    }
    juce::AudioProcessorValueTreeState& apvts;
    NewProjectAudioProcessor::SpectrumBridgeType& bridge;
    juce::RangedAudioParameter* centerParam = nullptr;
    juce::RangedAudioParameter* spreadParam = nullptr;
    juce::RangedAudioParameter* attractionParam = nullptr;
    juce::RangedAudioParameter* bypassParam = nullptr;
    juce::RangedAudioParameter* midiParam = nullptr;
    const std::atomic<int>* midiMask = nullptr;
    juce::RangedAudioParameter* noteParam[12] = { nullptr };
    juce::RangedAudioParameter* targetModeParam = nullptr;
    const std::atomic<int>* scTargetCount = nullptr;
    bool forceTargetColour = false;
    juce::Image heatImage;
    Snapshot snapshot;
    float fMax = 20000.0f;
    bool dragging = false;
    juce::Point<float> anchorPos;
    float anchorCenter = 640.0f;
    float anchorSpread = 2.5f;
    float dispCenter = 640.0f;
    float dispSpread = 2.5f;
    bool dispPrimed = false;
    float dragFillAmt = 0.0f;
    float thresholdBlendAmt = 0.0f;
    bool thresholdBlendPrimed = false;
    float bypassVisualAmt = 0.0f;
    bool bypassVisualPrimed = false;
    bool mouseOverSpectrum = false;
    float coordOverlayAlpha = 0.0f;
    float coordOverlayTargetAlpha = 0.0f;
    int coordOverlayToken = 0;
    Handle selected = Handle::none;
    bool typing = false;
    juce::String entryBuffer;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumDisplay)
};
```

### `TransientSplitter.h`

- Size: 6.37 KB
- Type: text

```h
/**
 * CREDITS TO ZL-AUDIO AND DERRY FITZGERALD FOR THE ENHANCED TRANSIENT DETECTION ALGORITHM!!!
 * PLEASE ALSO ADHERE TO THEIR LICENSING CONDITIONS FOR THIS PART!
 */
#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
namespace transientsplit
{
    template <typename T, int N>
    class HeapFilter
    {
    public:
        HeapFilter() { clear(); }
        void clear()
        {
            data_.fill(T(0)); pos_.fill(0); heap_.fill(0);
            idx_ = 0; minCt_ = 0; maxCt_ = 0; dataSize_ = 0;
            int n = N;
            while(n--) { pos_[(size_t) n] = ((n + 1) / 2) * ((n & 1) ? -1 : 1);
                          heap_[(size_t) (centerPos_ + pos_[(size_t) n])] = n; }
        }
        void insert(T v)
        {
            dataSize_ = (dataSize_ + 1) % N;
            const int p = pos_[(size_t) idx_];
            T old = data_[(size_t) idx_];
            data_[(size_t) idx_] = v;
            idx_ = (idx_ + 1) % N;
            if(p > 0)
            {
                if(minCt_ < (N - 1) / 2) ++minCt_;
                else if(v > old) { minSortDown(p); return; }
                if(minSortUp(p) && mmCmpExchange(0, -1)) maxSortDown(-1);
            }
            else if(p < 0)
            {
                if(maxCt_ < N / 2) ++maxCt_;
                else if(v < old) { maxSortDown(p); return; }
                if(maxSortUp(p) && minCt_ && mmCmpExchange(1, 0)) minSortDown(1);
            }
            else
            {
                if(maxCt_ && maxSortUp(-1)) maxSortDown(-1);
                if(minCt_ && minSortUp(1)) minSortDown(1);
            }
        }
        T getMedian()
        {
            if(minCt_ < maxCt_)
                return(data_[(size_t) heap_[(size_t) centerPos_]]
                      + data_[(size_t) heap_[(size_t) (centerPos_ - 1)]]) / T(2);
            return data_[(size_t) heap_[(size_t) centerPos_]];
        }
    private:
        std::array<T, N> data_;
        std::array<int, N> pos_;
        std::array<int, N> heap_;
        static constexpr int centerPos_ = N / 2;
        int idx_ = 0, minCt_ = 0, maxCt_ = 0, dataSize_ = 0;
        int mmExchange(int i, int j)
        {
            const auto ii = (size_t) (centerPos_ + i), jj = (size_t) (centerPos_ + j);
            std::swap(heap_[ii], heap_[jj]);
            pos_[(size_t) heap_[ii]] = i; pos_[(size_t) heap_[jj]] = j; return 1;
        }
        void minSortDown(int i) { for(i *= 2; i <= minCt_; i *= 2) { if(i < minCt_ && mmLess(i + 1, i)) ++i; if(! mmCmpExchange(i, i / 2)) break; } }
        void maxSortDown(int i) { for(i *= 2; i >= -maxCt_; i *= 2) { if(i > -maxCt_ && mmLess(i, i - 1)) --i; if(! mmCmpExchange(i / 2, i)) break; } }
        int mmLess(int i, int j) { return data_[(size_t) heap_[(size_t) (centerPos_ + i)]] < data_[(size_t) heap_[(size_t) (centerPos_ + j)]]; }
        int mmCmpExchange(int i, int j) { return(mmLess(i, j) && mmExchange(i, j)); }
        int minSortUp(int i) { while(i > 0 && mmCmpExchange(i, i / 2)) i /= 2; return(i == 0); }
        int maxSortUp(int i) { while(i < 0 && mmCmpExchange(i / 2, i)) i /= 2; return(i == 0); }
    };
    class TransientMask
    {
    public:
        void prepare(int numBins)
        {
            numBins_ = numBins;
            timeMed_.assign((size_t) numBins_, {});
            mask_.assign((size_t) numBins_, 0.0f);
            reset();
        }
        void reset()
        {
            for(auto& m : timeMed_) m.clear();
            freqMed_.clear();
            std::fill(mask_.begin(), mask_.end(), 0.0f);
        }
        void setThreshold(float x) { const float xc = juce::jlimit(0.0f, 1.0f, x); threshold_ = juce::jlimit(0.0f, 1.0f, 1.0f - xc * xc * std::sqrt(xc)); }
        void setBalance(float x) { cBalance_ = std::pow(16.0f, x - 0.75f); }
        void setSeparation(float x) { cSeparation_ = std::exp(x * 4.0f) - 1.0f; }
        void setHold(float x) { cHold_ = (32.0f - std::pow(32.0f, 1.0f - x)) / 31.0f * 0.75f + 0.24f; }
        void setSmooth(float x) { cSmooth_ = x; }
        void compute(const float* mag, float* maskOut)
        {
            freqMed_.clear();
            for(int i = 0; i < kFreqHalf; ++i) freqMed_.insert(mag[0]);
            for(int i = 0; i < kFreqHalf; ++i) freqMed_.insert(mag[(size_t) i]);
            for(int i = 0; i < numBins_ - kFreqHalf; ++i)
            {
                freqMed_.insert(mag[(size_t) (i + kFreqHalf)]);
                timeMed_[(size_t) i].insert(mag[(size_t) i]);
                mask_[(size_t) i] = std::max(mask_[(size_t) i] * cHold_,
                                              portion(freqMed_.getMedian(), timeMed_[(size_t) i].getMedian()));
            }
            for(int i = numBins_ - kFreqHalf; i < numBins_; ++i)
            {
                freqMed_.insert(mag[(size_t) (numBins_ - 1)]);
                timeMed_[(size_t) i].insert(mag[(size_t) i]);
                mask_[(size_t) i] = std::max(mask_[(size_t) i] * cHold_,
                                              portion(freqMed_.getMedian(), timeMed_[(size_t) i].getMedian()));
            }
            double sum = 0.0;
            for(int i = 0; i < numBins_; ++i) sum += mask_[(size_t) i];
            const float mean = (float) (sum / (double) numBins_);
            for(int i = 0; i < numBins_; ++i)
                maskOut[i] = (mean - mask_[(size_t) i]) * cSmooth_ + mask_[(size_t) i];
        }
    private:
        float portion(float transientWeight, float steadyWeight) const
        {
            const float t = transientWeight * cBalance_, s = steadyWeight;
            const float p = (t * t) / std::max(t * t + s * s, 1.0e-8f);
            const float lo = threshold_;
            const float hi = std::min(1.0f, threshold_ + ramp());
            float g = std::clamp((p - lo) / std::max(1.0e-6f, hi - lo), 0.0f, 1.0f);
            return g * g * (3.0f - 2.0f * g);
        }
        float ramp() const { return std::clamp(0.40f - 0.10f * cSeparation_, 0.06f, 0.40f); }
        static constexpr int kFreqSize = 5, kFreqHalf = kFreqSize / 2, kTimeSize = 5;
        int numBins_ = 0;
        std::vector<HeapFilter<float, kTimeSize>> timeMed_;
        HeapFilter<float, kFreqSize> freqMed_;
        std::vector<float> mask_;
        float cBalance_ = 1.0f, cSeparation_ = 1.0f, cHold_ = 0.9f, cSmooth_ = 0.5f, threshold_ = 1.0f;
    };
}
```

### `ValueKnob.h`

- Size: 16.21 KB
- Type: text

```h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "KnobLookAndFeel.h"
#include "SmoothColour.h"
#include "Oklch.h"
#include <cstdlib>
#include <cmath>
class ValueKnob : public juce::Slider,
                  private juce::Timer
{
public:
    enum class LinearEndCap
    {
        Square,
        LeftRounded,
        RightRounded,
        BothRounded
    };
    ValueKnob()
    {
        setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        setVelocityBasedMode(false);
        setWantsKeyboardFocus(true);
        editor.setMultiLine(false);
        editor.setJustification(juce::Justification::centred);
        editor.setBorder(juce::BorderSize<int> (1));
        editor.setFont(KnobLookAndFeel::courier(13.0f));
        editor.onReturnKey = [this] { commitEntry(); };
        editor.onEscapeKey = [this] { cancelEntry(); grabKeyboardFocus(); };
        editor.onFocusLost = [this] { cancelEntry(); };
        addChildComponent(editor);
        startTimerHz(60);
    }
    ~ValueKnob() override { stopTimer(); }
    void paint(juce::Graphics& g) override
    {
        const auto style = getSliderStyle();
        const bool rotary = (style == juce::Slider::Rotary
                          || style == juce::Slider::RotaryHorizontalDrag
                          || style == juce::Slider::RotaryVerticalDrag
                          || style == juce::Slider::RotaryHorizontalVerticalDrag);
        if(rotary) drawRotary(g);
        else if(style == juce::Slider::LinearVertical) drawVertical(g);
        else drawLinear(g);
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        smoothPos = e.position;
        dragging = true;
        juce::Slider::mouseDown(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        smoothPos += (e.position - smoothPos) * dragSmoothing;
        juce::Slider::mouseDrag(e.withNewPosition(smoothPos));
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        dragging = false;
        juce::Slider::mouseUp(e);
    }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if(wheelStep <= 0.0)
        {
            juce::Slider::mouseWheelMove(e, wheel);
            return;
        }
        const float axis = std::abs(wheel.deltaY) > std::abs(wheel.deltaX) ? wheel.deltaY
                                                                            : wheel.deltaX;
        if(std::abs(axis) < 1.0e-6f)
            return;
        const double stepDir = axis > 0.0f ? 1.0 : -1.0;
        const double next = juce::jlimit(getMinimum(), getMaximum(), getValue() + stepDir * wheelStep);
        setValue(next, juce::sendNotificationSync);
    }
    void setDragSmoothing(float rate) { dragSmoothing = juce::jlimit(0.05f, 1.0f, rate); }
    void setWheelStep(double step) { wheelStep = juce::jmax(0.0, step); }
    double wheelStep = 0.1;
    juce::String getTextFromValue(double value) override
    {
        return juce::String(value, displayDecimals) + getTextValueSuffix();
    }
    void setDisplayDecimals(int dp) { displayDecimals = juce::jmax(0, dp); }
    void setGradientArc(bool g) { gradientArc = g; arcImage = juce::Image(); repaint(); }
    void setMagentaAccent(bool enabled) { magentaAccent = enabled; repaint(); }
    void setLinearTrackEndCap(LinearEndCap c) { linearTrackEndCap = c; repaint(); }
    void setLinearFillEndCap(LinearEndCap c) { linearFillEndCap = c; repaint(); }
    bool keyPressed(const juce::KeyPress& k) override
    {
        const auto c = k.getTextCharacter();
        if((c >= '0' && c <= '9') || c == '.' || c == '-')
        {
            beginEntry(juce::String::charToString(c));
            return true;
        }
        return juce::Slider::keyPressed(k);
    }
    void resized() override
    {
        juce::Slider::resized();
        entryBounds = getLocalBounds().removeFromBottom(18).reduced(2, 0);
    }
private:
    void timerCallback() override
    {
        bool moving = false;
        const juce::Colour discTarget = dragging ? juce::Colour(0xff30303a)
                                                  : juce::Colour(0xff1d1d23);
        if(! discPrimed) { discS.set(discTarget); discPrimed = true; moving = true; }
        else if(discS.approach(discTarget, 1.0f / 3.0f)) moving = true;
        const double eps = (getMaximum() - getMinimum()) * 1.0e-4 + 1.0e-9;
        if(disp.approach(getValue(), 1.0 / 1.4, eps)) moving = true;
        if(moving) repaint();
    }
    double dispVal() const
    {
        return disp.primed ? juce::jlimit(getMinimum(), getMaximum(), disp.get())
                           : getValue();
    }
    void drawRotary(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat().reduced(6.0f);
        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();
        const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        if(radius < 2.0f) return;
        const float trackW = juce::jmax(2.0f, radius * 0.16f);
        const float rArc = radius - trackW * 0.5f;
        const float rInner = rArc - trackW * 0.5f;
        const float rBig = rInner - juce::jmax(1.5f, radius * 0.04f);
        const auto rp = getRotaryParameters();
        const float startAngle = rp.startAngleRadians;
        const float endAngle = rp.endAngleRadians;
        const float pos = (float) valueToProportionOfLength(dispVal());
        const double mn = getMinimum(), mx = getMaximum();
        const double splitVal = (mn < 0.0 && mx > 0.0) ? 0.0 : mn;
        const float splitPos = (float) valueToProportionOfLength(splitVal);
        const float valueAngle = startAngle + pos * (endAngle - startAngle);
        const float splitAngle = startAngle + splitPos * (endAngle - startAngle);
        if(rBig > 1.0f)
        {
            g.setColour(discPrimed ? discS.get() : juce::Colour(0xff1d1d23));
            g.fillEllipse(cx - rBig, cy - rBig, 2.0f * rBig, 2.0f * rBig);
        }
        juce::Path track;
        track.addCentredArc(cx, cy, rArc, rArc, 0.0f, startAngle, endAngle, true);
        g.setColour(juce::Colour(0xff4a4a54));
        g.strokePath(track, juce::PathStrokeType(trackW, juce::PathStrokeType::curved,
                                                           juce::PathStrokeType::butt));
        if(std::abs(valueAngle - splitAngle) > 1.0e-3f)
        {
            if(gradientArc)
            {
                if(arcImage.isNull() || arcImage.getWidth() != getWidth()
                                      || arcImage.getHeight() != getHeight())
                    buildArcImage(getWidth(), getHeight(), cx, cy, rArc, trackW,
                                   startAngle, endAngle);
                juce::Path wedge;
                wedge.startNewSubPath(cx, cy);
                wedge.addCentredArc(cx, cy, radius, radius, 0.0f,
                                     juce::jmin(splitAngle, valueAngle),
                                     juce::jmax(splitAngle, valueAngle), false);
                wedge.closeSubPath();
                juce::Graphics::ScopedSaveState ss(g);
                g.reduceClipRegion(wedge);
                g.drawImageAt(arcImage, 0, 0);
            }
            else
            {
                juce::Path fill;
                fill.addCentredArc(cx, cy, rArc, rArc, 0.0f,
                                    juce::jmin(splitAngle, valueAngle),
                                    juce::jmax(splitAngle, valueAngle), true);
                g.setColour(juce::Colour(0xff45aeb1));
                g.strokePath(fill, juce::PathStrokeType(trackW, juce::PathStrokeType::curved,
                                                                  juce::PathStrokeType::butt));
            }
        }
        const float handleW = 3.0f;
        const float handleLen = 10.0f;
        const float handleGap = 5.0f;
        const float rOuter = juce::jmax(0.0f, rBig - handleGap);
        const float rInnerH = juce::jmax(0.0f, rOuter - handleLen);
        juce::Path ptr;
        ptr.startNewSubPath(0.0f, -rInnerH);
        ptr.lineTo(0.0f, -rOuter);
        g.setColour(juce::Colour(0xffe8e8ee));
        g.strokePath(ptr, juce::PathStrokeType(handleW, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::butt),
                      juce::AffineTransform::rotation(valueAngle).translated(cx, cy));
    }
    static float smoothstep(float e0, float e1, float v)
    {
        if(e1 <= e0) return v < e0 ? 0.0f : 1.0f;
        const float t = juce::jlimit(0.0f, 1.0f, (v - e0) / (e1 - e0));
        return t * t * (3.0f - 2.0f * t);
    }
    void buildArcImage(int w, int h, float cx, float cy, float rArc, float trackW,
                        float startAngle, float endAngle)
    {
        arcImage = juce::Image(juce::Image::ARGB, juce::jmax(1, w), juce::jmax(1, h), true);
        const float twoPi = juce::MathConstants<float>::twoPi;
        const float rInner = rArc - trackW * 0.5f;
        const float rOuter = rArc + trackW * 0.5f;
        const float startN = startAngle - std::floor(startAngle / twoPi) * twoPi;
        const float sweep = endAngle - startAngle;
        const juce::Colour cTeal(0xff45aeb1), cMagenta(0xffeb8fff);
        juce::Image::BitmapData bmp(arcImage, juce::Image::BitmapData::writeOnly);
        for(int y = 0; y < h; ++y)
            for(int x = 0; x < w; ++x)
            {
                const float dx = (float) x + 0.5f - cx;
                const float dy = (float) y + 0.5f - cy;
                const float d = std::sqrt(dx * dx + dy * dy);
                const float cov = smoothstep(rInner - 0.75f, rInner + 0.75f, d)
                                * (1.0f - smoothstep(rOuter - 0.75f, rOuter + 0.75f, d));
                if(cov <= 0.0f) continue;
                float a = std::atan2 (dx, -dy);
                if(a < 0.0f) a += twoPi;
                float rel = a - startN;
                if(rel < 0.0f) rel += twoPi;
                if(sweep > 0.0f && rel > sweep) continue;
                const float prop = (sweep > 0.0f) ? juce::jlimit(0.0f, 1.0f, rel / sweep) : 0.0f;
                const juce::Colour col = oklch::lerp(cTeal, cMagenta, prop * prop);
                bmp.setPixelColour(x, y, col.withAlpha(cov));
            }
    }
    void drawLinear(juce::Graphics& g)
    {
        auto b = getLocalBounds().toFloat().reduced(4.0f);
        const float midY = b.getCentreY();
        const float h = b.getHeight();
        const float top = midY - h * 0.5f;
        const float hw = juce::jmax(9.0f, h * 0.5f);
        const float corner = juce::jmin(h * 0.5f, hw * 0.5f);
        const float travelW = juce::jmax(0.0f, b.getWidth() - hw);
        g.setColour(juce::Colour(0xff4a4a54));
        fillHorizontalWithEndCap(g, b.getX(), top, b.getWidth(), h, corner, linearTrackEndCap);
        const double mn = getMinimum(), mx = getMaximum();
        const bool bipolar = (mn < 0.0 && mx > 0.0);
        const double splitVal = bipolar ? 0.0 : mn;
        const float sp = (float) valueToProportionOfLength(splitVal);
        const float vp = (float) valueToProportionOfLength(dispVal());
        const float xS = b.getX() + hw * 0.5f + sp * travelW;
        const float xV = b.getX() + hw * 0.5f + vp * travelW;
        float x0 = juce::jmin(xS, xV);
        float x1 = juce::jmax(xS, xV);
        if(! bipolar)
        {
            if(mn >= 0.0)
                x0 = b.getX();
            else
                x1 = b.getRight();
        }
        g.setColour(accentColour());
        fillHorizontalWithEndCap(g, x0, top, juce::jmax(1.0f, x1 - x0), h, corner, linearFillEndCap);
        const float tx = xV;
        g.setColour(juce::Colour(0xffe8e8ee));
        g.fillRoundedRectangle(tx - hw * 0.5f, top, hw, h, corner);
    }
    void drawVertical(juce::Graphics& g)
    {
        auto b = getLocalBounds().toFloat().reduced(4.0f);
        const float cx = b.getCentreX();
        const float w = b.getWidth();
        const float left = cx - w * 0.5f;
        const float hh = juce::jmax(9.0f, w * 0.5f);
        const float corner = juce::jmin(w * 0.5f, hh * 0.5f);
        const float travelH = juce::jmax(0.0f, b.getHeight() - hh);
        g.setColour(juce::Colour(0xff4a4a54));
        g.fillRoundedRectangle(left, b.getY(), w, b.getHeight(), corner);
        const double mn = getMinimum(), mx = getMaximum();
        const double splitVal = (mn < 0.0 && mx > 0.0) ? 0.0 : mn;
        const float sp = (float) valueToProportionOfLength(splitVal);
        const float vp = (float) valueToProportionOfLength(dispVal());
        const float yS = b.getBottom() - hh * 0.5f - sp * travelH;
        const float yV = b.getBottom() - hh * 0.5f - vp * travelH;
        const float y0 = juce::jmin(yS, yV);
        const float y1 = juce::jmax(yS, yV);
        g.setColour(accentColour());
        g.fillRect(left, y0, w, juce::jmax(1.0f, y1 - y0));
        const float ty = yV;
        g.setColour(juce::Colour(0xffe8e8ee));
        g.fillRoundedRectangle(left, ty - hh * 0.5f, w, hh, corner);
    }
    static void fillHorizontalWithEndCap(juce::Graphics& g,
                                          float x, float y, float w, float h,
                                          float corner, LinearEndCap cap)
    {
        if(w <= 0.0f || h <= 0.0f)
            return;
        switch(cap)
        {
            case LinearEndCap::Square:
                g.fillRect(x, y, w, h);
                break;
            case LinearEndCap::BothRounded:
                g.fillRoundedRectangle(x, y, w, h, corner);
                break;
            case LinearEndCap::LeftRounded:
            {
                juce::Path p;
                p.addRoundedRectangle(x, y, w, h, corner, corner,
                                       true, false, true, false);
                g.fillPath(p);
                break;
            }
            case LinearEndCap::RightRounded:
            {
                juce::Path p;
                p.addRoundedRectangle(x, y, w, h, corner, corner,
                                       false, true, false, true);
                g.fillPath(p);
                break;
            }
        }
    }
    void beginEntry(const juce::String& initial)
    {
        editor.setBounds(entryBounds);
        editor.setText(initial, false);
        editor.setVisible(true);
        editor.toFront(true);
        editor.grabKeyboardFocus();
        editor.moveCaretToEnd();
    }
    void commitEntry()
    {
        if(! editor.isVisible()) return;
        double v = 0.0;
        if(parse(editor.getText(), v))
            setValue(juce::jlimit(getMinimum(), getMaximum(), v), juce::sendNotificationSync);
        editor.setVisible(false);
        grabKeyboardFocus();
        repaint();
    }
    void cancelEntry()
    {
        if(! editor.isVisible()) return;
        editor.setVisible(false);
        repaint();
    }
    static bool parse(const juce::String& text, double& out)
    {
        const auto s = text.trim();
        if(s.isEmpty()) return false;
        const auto utf8 = s.toRawUTF8();
        char* end = nullptr;
        const double v = std::strtod(utf8, &end);
        if(end == utf8 || *end != '\0') return false;
        out = v;
        return true;
    }
    juce::Colour accentColour() const
    {
        return magentaAccent ? juce::Colour(0xffeb8fff).withMultipliedSaturation(0.62f)
                                               .withMultipliedBrightness(0.80f)
                             : juce::Colour(0xff45aeb1);
    }
    juce::TextEditor editor;
    juce::Rectangle<int> entryBounds;
    juce::Point<float> smoothPos;
    float dragSmoothing = 0.6f;
    int displayDecimals = 2;
    SmoothValue disp;
    SmoothColour discS;
    bool discPrimed = false;
    bool dragging = false;
    bool gradientArc = false;
    bool magentaAccent = false;
    LinearEndCap linearTrackEndCap = LinearEndCap::BothRounded;
    LinearEndCap linearFillEndCap = LinearEndCap::Square;
    juce::Image arcImage;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ValueKnob)
};
```

