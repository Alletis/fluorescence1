#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "KnobLookAndFeel.h"
#include "SmoothColour.h"
#include <array>
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
    void setMagentaAccent(bool enabled)
    {
        magentaAccent = enabled;
        repaint();
    }
    void setIndex(int i, bool notify = false)
    {
        const int n = ((i % 12) + 12) % 12;
        if(n != index)
        {
            index = n;
            repaint();
            if(notify && onValueChange)
                onValueChange(index);
        }
    }
    int getIndex() const noexcept { return index; }
    void paint(juce::Graphics& g) override
    {
        if(getWidth() <= 0 || getHeight() <= 0)
            return;
        const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
        constexpr float corner = 3.0f;
        g.setColour(baseFill.get());
        g.fillRoundedRectangle(bounds, corner);
        juce::Path clippingPath;
        clippingPath.addRoundedRectangle(bounds, corner);
        g.saveState();
        g.reduceClipRegion(clippingPath);
        for(int i = 0; i < 3; ++i)
        {
            g.setColour(colours[(std::size_t) i].fill.get());
            g.fillRect(regionBounds(static_cast<Region>(i)));
        }
        g.restoreState();
        const auto left = regionBounds(Region::previous);
        const auto right = regionBounds(Region::next);
        g.setColour(isEnabled() ? juce::Colour(0xff34343e)
                                : juce::Colour(0xff292930));
        g.drawLine((float) left.getRight(), bounds.getY() + 5.0f,
                   (float) left.getRight(), bounds.getBottom() - 5.0f, 1.0f);
        g.drawLine((float) right.getX(), bounds.getY() + 5.0f,
                   (float) right.getX(), bounds.getBottom() - 5.0f, 1.0f);
        g.setColour(baseOutline.get());
        g.drawRoundedRectangle(bounds, corner, 1.2f);
        g.setFont(KnobLookAndFeel::courier(
            juce::jlimit(9.0f, 18.0f, (float) getHeight() * 0.62f)));
        g.setColour(colours[0].foreground.get());
        g.drawFittedText("<", left, juce::Justification::centred, 1);
        g.setColour(colours[2].foreground.get());
        g.drawFittedText(">", right, juce::Justification::centred, 1);
        g.setColour(colours[1].foreground.get());
        g.setFont(KnobLookAndFeel::courier(
            juce::jlimit(8.0f, 40.0f, (float) getHeight() * 0.50f)));
        g.drawFittedText(noteName(index), regionBounds(Region::title).reduced(3, 1),
                         juce::Justification::centred, 1);
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        pressedRegion = Region::none;
        appliedSteps = 0;
        changed = false;
        draggingTitle = false;
        if(! isEnabled() || ! e.mods.isLeftButtonDown() || e.mods.isPopupMenu())
            return;
        pressedRegion = hitTestRegion(e.getEventRelativeTo(this).getPosition());
        repaint();
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if(! isEnabled() || pressedRegion != Region::title)
            return;
        draggingTitle = true;
        const auto off = e.getOffsetFromDragStart();
        const float along = (float) off.x - (float) off.y;
        const float stepDistance = pixelsPerStep * (e.mods.isCtrlDown() ? 3.0f : 1.0f);
        const int steps = (int) (along / stepDistance);
        if(steps != appliedSteps)
        {
            setIndex(index + (steps - appliedSteps), true);
            appliedSteps = steps;
            changed = true;
        }
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        const Region origin = pressedRegion;
        pressedRegion = Region::none;
        draggingTitle = false;
        repaint();
        if(! isEnabled() || origin == Region::none)
            return;
        if(hitTestRegion(e.getEventRelativeTo(this).getPosition()) != origin)
            return;
        switch(origin)
        {
            case Region::previous:
                setIndex(index - 1, true);
                break;
            case Region::next:
                setIndex(index + 1, true);
                break;
            case Region::title:
                if(! changed && ! e.mouseWasDraggedSinceMouseDown() && onMenuRequest)
                    onMenuRequest();
                break;
            case Region::none:
                break;
        }
    }
    void mouseWheelMove(const juce::MouseEvent&,
                        const juce::MouseWheelDetails& wheel) override
    {
        if(! isEnabled())
            return;
        const float primaryDelta = std::abs(wheel.deltaY) >= std::abs(wheel.deltaX)
                                     ? wheel.deltaY : wheel.deltaX;
        if(primaryDelta > 0.0f)
            setIndex(index + 1, true);
        else if(primaryDelta < 0.0f)
            setIndex(index - 1, true);
    }
    void enablementChanged() override
    {
        if(! isEnabled())
        {
            pressedRegion = Region::none;
            draggingTitle = false;
        }
        repaint();
    }
    static juce::String noteName(int i)
    {
        static const char* n[12] =
            { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return n[((i % 12) + 12) % 12];
    }
private:
    enum class Region { previous = 0, title = 1, next = 2, none = 3 };
    struct RegionColours
    {
        SmoothColour fill;
        SmoothColour foreground;
    };
    juce::Rectangle<int> regionBounds(Region region) const
    {
        auto bounds = getLocalBounds();
        const int arrowWidth = juce::jmin(bounds.getWidth() / 3,
                                         juce::jmax(20, bounds.getHeight()));
        if(region == Region::previous)
            return bounds.removeFromLeft(arrowWidth);
        if(region == Region::next)
            return bounds.removeFromRight(arrowWidth);
        if(region == Region::title)
            return bounds.withTrimmedLeft(arrowWidth).withTrimmedRight(arrowWidth);
        return {};
    }
    Region hitTestRegion(juce::Point<int> p) const
    {
        if(! getLocalBounds().contains(p))
            return Region::none;
        if(regionBounds(Region::previous).contains(p))
            return Region::previous;
        if(regionBounds(Region::next).contains(p))
            return Region::next;
        return Region::title;
    }
    juce::Colour hoverFill() const
    {
        return magentaAccent ? juce::Colour(0xff281d30) : juce::Colour(0xff20202a);
    }
    juce::Colour pressedFill() const
    {
        return magentaAccent ? juce::Colour(0xff160f18) : juce::Colour(0xff0c0c10);
    }
    juce::Colour hoverLine() const
    {
        return magentaAccent ? juce::Colour(0xffffbbff) : juce::Colour(0xff6ecdd0);
    }
    juce::Colour pressedLine() const
    {
        return magentaAccent ? juce::Colour(0xffb85cc7) : juce::Colour(0xff307a7c);
    }
    juce::Colour hoverForeground() const
    {
        return magentaAccent ? juce::Colour(0xffffd5ff) : juce::Colour(0xff9fe3e5);
    }
    void timerCallback() override
    {
        const bool keepTitleHovered = isEnabled() && draggingTitle
                                   && pressedRegion == Region::title;
        const Region hovered = keepTitleHovered ? Region::title
                             : (isEnabled() && isMouseOver(true))
                                   ? hitTestRegion(getMouseXYRelative()) : Region::none;
        const bool pressed = ! keepTitleHovered && hovered != Region::none
                             && hovered == pressedRegion && isMouseButtonDown(true);
        bool moving = false;
        constexpr float rate = 1.0f / 3.0f;
        const auto smooth = [this, &moving, rate](SmoothColour& colour, juce::Colour target)
        {
            if(! primed)
            {
                colour.set(target);
                moving = true;
            }
            else if(colour.approach(target, rate))
            {
                moving = true;
            }
        };
        const auto borderTarget = ! isEnabled() ? juce::Colour(0xff2c2c32)
                                : pressed ? pressedLine()
                                : hovered != Region::none ? hoverLine()
                                : juce::Colour(0xff4a4a54);
        smooth(baseFill, isEnabled() ? juce::Colour(0xff15151a)
                                     : juce::Colour(0xff121216));
        smooth(baseOutline, borderTarget);
        for(int i = 0; i < 3; ++i)
        {
            const Region region = static_cast<Region>(i);
            const bool hot = isEnabled() && hovered == region;
            const bool down = ! keepTitleHovered && hot
                              && pressedRegion == region && isMouseButtonDown(true);
            const auto fill = ! isEnabled() ? juce::Colour(0xff121216)
                            : down ? pressedFill()
                            : hot ? hoverFill()
                                            : juce::Colour(0xff15151a);
            const auto text = ! isEnabled() ? juce::Colour(0xff55555c)
                            : down ? pressedLine()
                            : hot ? hoverForeground()
                            : region == Region::title ? juce::Colour(0xffd0d0d8)
                                                       : juce::Colour(0xff8a8a92);
            smooth(colours[(std::size_t) i].fill, fill);
            smooth(colours[(std::size_t) i].foreground, text);
        }
        primed = true;
        if(moving)
            repaint();
    }
    static constexpr float pixelsPerStep = 16.0f;
    int index = 0;
    int appliedSteps = 0;
    bool changed = false;
    bool draggingTitle = false;
    Region pressedRegion = Region::none;
    std::array<RegionColours, 3> colours;
    SmoothColour baseFill, baseOutline;
    bool primed = false;
    bool magentaAccent = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeyButton)
};