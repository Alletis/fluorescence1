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