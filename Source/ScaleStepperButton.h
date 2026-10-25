#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "KnobLookAndFeel.h"
#include "SmoothColour.h"
#include <array>
#include <cstddef>
#include <functional>
class ScaleStepperButton : public juce::Button,
                           private juce::Timer
{
public:
    std::function<void(int)> onStep;
    ScaleStepperButton() : juce::Button("Scale selector")
    {
        startTimerHz(60);
    }
    ~ScaleStepperButton() override { stopTimer(); }
    void setMagentaAccent(bool enabled)
    {
        magentaAccent = enabled;
        repaint();
    }
    void setActive(bool selected)
    {
        active = selected;
        repaint();
    }
    bool isActive() const noexcept { return active; }
    void paintButton(juce::Graphics& g, bool, bool) override
    {
        if(getWidth() <= 0 || getHeight() <= 0)
            return;
        const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
        constexpr float corner = 3.0f;
        g.setColour(baseFill.get());
        g.fillRoundedRectangle(bounds, corner);
        juce::Path clip;
        clip.addRoundedRectangle(bounds, corner);
        g.saveState();
        g.reduceClipRegion(clip);
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
        g.drawFittedText(getButtonText(), regionBounds(Region::title).reduced(3, 1),
                         juce::Justification::centred, 1);
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        pressedRegion = Region::none;
        appliedDragSteps = 0;
        dragChanged = false;
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
        const auto offset = e.getOffsetFromDragStart();
        const float along = (float) offset.x - (float) offset.y;
        const float stepDistance = dragPixelsPerStep * (e.mods.isCtrlDown() ? 3.0f : 1.0f);
        const int steps = (int) (along / stepDistance);
        const int delta = steps - appliedDragSteps;
        if(delta == 0)
            return;
        const int direction = delta > 0 ? 1 : -1;
        for(int i = 0; i < std::abs(delta); ++i)
            if(onStep) onStep(direction);
        appliedDragSteps = steps;
        dragChanged = true;
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        const auto origin = pressedRegion;
        pressedRegion = Region::none;
        draggingTitle = false;
        repaint();
        if(! isEnabled() || origin == Region::none)
            return;
        if(hitTestRegion(e.getEventRelativeTo(this).getPosition()) != origin)
            return;
        if(origin == Region::previous)
        {
            if(onStep) onStep(-1);
        }
        else if(origin == Region::next)
        {
            if(onStep) onStep(1);
        }
        else if(origin == Region::title && ! dragChanged
                 && ! e.mouseWasDraggedSinceMouseDown())
        {
            if(onClick) onClick();
        }
    }
    void mouseWheelMove(const juce::MouseEvent& e,
                        const juce::MouseWheelDetails& wheel) override
    {
        if(! isEnabled())
            return;
        const auto eventTimeMs = e.eventTime.toMilliseconds();
        if(eventTimeMs == lastWheelEventTimeMs)
            return;
        lastWheelEventTimeMs = eventTimeMs;
        const float primaryDelta = std::abs(wheel.deltaY) >= std::abs(wheel.deltaX)
                                     ? wheel.deltaY : wheel.deltaX;
        const int direction = primaryDelta > 0.0f ? 1 : primaryDelta < 0.0f ? -1 : 0;
        if(direction == 0)
            return;
        if(! wheel.isSmooth)
        {
            smoothWheelAccumulator = 0.0f;
            if(onStep) onStep(direction);
            return;
        }
        smoothWheelAccumulator += primaryDelta * 5.0f;
        while(smoothWheelAccumulator > 1.0f)
        {
            smoothWheelAccumulator -= 1.0f;
            if(onStep) onStep(1);
        }
        while(smoothWheelAccumulator < -1.0f)
        {
            smoothWheelAccumulator += 1.0f;
            if(onStep) onStep(-1);
        }
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
        const auto hovered = keepTitleHovered ? Region::title
                           : (isEnabled() && isMouseOver(true))
                               ? hitTestRegion(getMouseXYRelative()) : Region::none;
        const bool pressed = hovered != Region::none && hovered == pressedRegion
                             && isMouseButtonDown(true);
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
            const auto region = static_cast<Region>(i);
            const bool hot = isEnabled() && hovered == region;
            const bool down = hot && pressedRegion == region && isMouseButtonDown(true);
            const auto fill = ! isEnabled() ? juce::Colour(0xff121216)
                            : down ? pressedFill()
                            : hot ? hoverFill()
                            : (active && region == Region::title)
                                ? (magentaAccent ? juce::Colour(0xff261a29)
                                                 : juce::Colour(0xff18262a))
                                : juce::Colour(0xff15151a);
            const auto foreground = ! isEnabled() ? juce::Colour(0xff55555c)
                                  : down ? pressedLine()
                                  : hot ? hoverForeground()
                                  : region == Region::title
                                      ? juce::Colour(0xffe8e8ee)
                                      : juce::Colour(0xff8a8a92);
            smooth(colours[(std::size_t) i].fill, fill);
            smooth(colours[(std::size_t) i].foreground, foreground);
        }
        primed = true;
        if(moving)
            repaint();
    }
    std::array<RegionColours, 3> colours;
    SmoothColour baseFill, baseOutline;
    static constexpr float dragPixelsPerStep = 16.0f;
    Region pressedRegion = Region::none;
    int appliedDragSteps = 0;
    bool dragChanged = false;
    bool draggingTitle = false;
    juce::int64 lastWheelEventTimeMs = -1;
    float smoothWheelAccumulator = 0.0f;
    bool primed = false;
    bool active = false;
    bool magentaAccent = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScaleStepperButton)
};