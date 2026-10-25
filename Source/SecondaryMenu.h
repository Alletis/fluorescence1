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
        setOpaque(true);
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
        if(onClose) onClose();
    }
    void toggle() { open ? close() : show(); }
    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(0.75f);
        juce::Path clipPath;
        clipPath.addRoundedRectangle(b, 12.0f);
        g.fillAll(juce::Colour(0xff17171c));
        if(originalBackdrop.isValid())
        {
            g.drawImageWithin(originalBackdrop,
                               0, 0, getWidth(), getHeight(),
                               juce::RectanglePlacement::stretchToFit);
        }
        if(blurredBackdrop.isValid())
        {
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(clipPath);
            g.drawImageWithin(blurredBackdrop,
                               0, 0, getWidth(), getHeight(),
                               juce::RectanglePlacement::stretchToFit);
            g.setColour(juce::Colour(0xff17171c).withAlpha(0.5f));
            g.fillRoundedRectangle(b, 12.0f);
        }
        if(! blurredBackdrop.isValid())
        {
            g.setColour(juce::Colour(0xff17171c).withAlpha(0.5f));
            g.fillRoundedRectangle(b, 12.0f);
        }
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
    void moved() override { originalBackdrop = {}; blurredBackdrop = {}; }
    void resized() override { originalBackdrop = {}; blurredBackdrop = {}; }
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
    static void boxBlur1DHorizontal(const juce::Image& src, juce::Image& dst, int radius)
    {
        const int w = src.getWidth();
        const int h = src.getHeight();
        juce::Image::BitmapData srcData(src, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData dstData(dst, juce::Image::BitmapData::writeOnly);
        const int windowSize = radius * 2 + 1;
        for(int y = 0; y < h; ++y)
        {
            const auto* srcLine = srcData.getLinePointer(y);
            auto* dstLine = dstData.getLinePointer(y);
            int sumA = 0, sumR = 0, sumG = 0, sumB = 0;
            for(int k = -radius; k <= radius; ++k)
            {
                const int sx = juce::jlimit(0, w - 1, k);
                const auto* px = reinterpret_cast<const juce::PixelARGB*> (srcLine + sx * srcData.pixelStride);
                sumA += (int) px->getAlpha();
                sumR += (int) px->getRed();
                sumG += (int) px->getGreen();
                sumB += (int) px->getBlue();
            }
            for(int x = 0; x < w; ++x)
            {
                auto* out = reinterpret_cast<juce::PixelARGB*> (dstLine + x * dstData.pixelStride);
                out->setARGB((juce::uint8) (sumA / windowSize),
                              (juce::uint8) (sumR / windowSize),
                              (juce::uint8) (sumG / windowSize),
                              (juce::uint8) (sumB / windowSize));
                if(x == w - 1)
                    continue;
                const int sxOut = juce::jlimit(0, w - 1, x - radius);
                const int sxIn = juce::jlimit(0, w - 1, x + radius + 1);
                const auto* outPx = reinterpret_cast<const juce::PixelARGB*> (srcLine + sxOut * srcData.pixelStride);
                const auto* inPx = reinterpret_cast<const juce::PixelARGB*> (srcLine + sxIn * srcData.pixelStride);
                sumA += (int) inPx->getAlpha() - (int) outPx->getAlpha();
                sumR += (int) inPx->getRed() - (int) outPx->getRed();
                sumG += (int) inPx->getGreen() - (int) outPx->getGreen();
                sumB += (int) inPx->getBlue() - (int) outPx->getBlue();
            }
        }
    }
    static void boxBlur1DVertical(const juce::Image& src, juce::Image& dst, int radius)
    {
        const int w = src.getWidth();
        const int h = src.getHeight();
        juce::Image::BitmapData srcData(src, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData dstData(dst, juce::Image::BitmapData::writeOnly);
        const int windowSize = radius * 2 + 1;
        for(int x = 0; x < w; ++x)
        {
            int sumA = 0, sumR = 0, sumG = 0, sumB = 0;
            for(int k = -radius; k <= radius; ++k)
            {
                const int sy = juce::jlimit(0, h - 1, k);
                const auto* px = reinterpret_cast<const juce::PixelARGB*> (
                    srcData.getLinePointer(sy) + x * srcData.pixelStride);
                sumA += (int) px->getAlpha();
                sumR += (int) px->getRed();
                sumG += (int) px->getGreen();
                sumB += (int) px->getBlue();
            }
            for(int y = 0; y < h; ++y)
            {
                auto* out = reinterpret_cast<juce::PixelARGB*> (
                    dstData.getLinePointer(y) + x * dstData.pixelStride);
                out->setARGB((juce::uint8) (sumA / windowSize),
                              (juce::uint8) (sumR / windowSize),
                              (juce::uint8) (sumG / windowSize),
                              (juce::uint8) (sumB / windowSize));
                if(y == h - 1)
                    continue;
                const int syOut = juce::jlimit(0, h - 1, y - radius);
                const int syIn = juce::jlimit(0, h - 1, y + radius + 1);
                const auto* outPx = reinterpret_cast<const juce::PixelARGB*> (
                    srcData.getLinePointer(syOut) + x * srcData.pixelStride);
                const auto* inPx = reinterpret_cast<const juce::PixelARGB*> (
                    srcData.getLinePointer(syIn) + x * srcData.pixelStride);
                sumA += (int) inPx->getAlpha() - (int) outPx->getAlpha();
                sumR += (int) inPx->getRed() - (int) outPx->getRed();
                sumG += (int) inPx->getGreen() - (int) outPx->getGreen();
                sumB += (int) inPx->getBlue() - (int) outPx->getBlue();
            }
        }
    }
    void fastBoxBlur(juce::Image& img, int radius, int passes)
    {
        if(! img.isValid() || radius <= 0 || passes <= 0)
            return;
        const bool scratchNeedsRebuilding = ! blurScratch.isValid()
                                         || blurScratch.getWidth() != img.getWidth()
                                         || blurScratch.getHeight() != img.getHeight();
        if(scratchNeedsRebuilding)
        {
            blurScratch = juce::Image(juce::Image::ARGB,
                                       img.getWidth(),
                                       img.getHeight(),
                                       true);
        }
        for(int i = 0; i < passes; ++i)
        {
            boxBlur1DHorizontal(img, blurScratch, radius);
            boxBlur1DVertical(blurScratch, img, radius);
        }
    }
    void refreshBlurredBackdrop()
    {
        auto* parent = getParentComponent();
        if(parent == nullptr)
            return;
        const auto areaInParent = getBounds();
        if(areaInParent.isEmpty())
            return;
        const float oldAlpha = getAlpha();
        setAlpha(0.0f);
        auto newOriginalBackdrop = parent->createComponentSnapshot(areaInParent,
                                        true,
                                        1.0f,
                                        juce::SoftwareImageType {});
        constexpr float snapshotScale = 1.0f / static_cast<float> (blurDownsampleFactor);
        auto low = parent->createComponentSnapshot(areaInParent,
                                                    true,
                                                    snapshotScale,
                                                    juce::SoftwareImageType {});
        setAlpha(oldAlpha);
        if(newOriginalBackdrop.isValid())
            originalBackdrop = std::move(newOriginalBackdrop);
        if(! low.isValid())
            return;
        fastBoxBlur(low, blurRadiusLowRes, blurPasses);
        blurredBackdrop = std::move(low);
    }
    void timerCallback() override
    {
        updatePinColour();
        const bool shouldRefreshBackdrop = isVisible() && (open || animInProgress || anim > 0.0f);
        if(shouldRefreshBackdrop)
        {
            if(++blurTickCounter >= blurRefreshEveryTicks)
            {
                blurTickCounter = 0;
                refreshBlurredBackdrop();
                repaint();
            }
        }
        const float target = open ? 1.0f : 0.0f;
        if(! animInProgress || target != animEnd)
        {
            animStart = anim;
            animEnd = target;
            animPhase = 0.0f;
            animInProgress = true;
        }
        animPhase = juce::jmin(1.0f, animPhase + animPhaseStep);
        const float eased = 0.5f - 0.5f * std::cos(animPhase * juce::MathConstants<float>::pi);
        anim = animStart + (animEnd - animStart) * eased;
        applyAnim();
        if(animPhase >= 1.0f)
        {
            anim = animEnd;
            applyAnim();
            animInProgress = false;
            if(! open && isVisible())
            {
                setVisible(false);
                originalBackdrop = {};
                blurredBackdrop = {};
            }
        }
    }
    void applyAnim()
    {
        setAlpha(anim);
    }
    bool open = false;
    float anim = 0.0f;
    float animStart = 0.0f;
    float animEnd = 0.0f;
    float animPhase = 1.0f;
    bool animInProgress = false;
    static constexpr float animPhaseStep = 1.0f / 10.0f;
    int blurTickCounter = 0;
    static constexpr int blurRefreshEveryTicks = 2;
    static constexpr int blurDownsampleFactor = 4;
    static constexpr int blurRadiusLowRes = 2;
    static constexpr int blurPasses = 3;
    juce::Image originalBackdrop;
    juce::Image blurredBackdrop;
    juce::Image blurScratch;
    bool pinned = false;
    bool pinHover = false;
    bool pinArmed = false;
    bool pinPrimed = false;
    SmoothColour pinS;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FlyoutPanel)
};