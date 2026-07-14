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