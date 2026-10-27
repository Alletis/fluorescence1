#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "KnobLookAndFeel.h"
#include "SmoothColour.h"
#include <array>
#include <cstddef>
class SmoothComboBox : public juce::ComboBox,
                       private juce::Timer
{
public:
    SmoothComboBox()
    {
        setColour(juce::ComboBox::textColourId, juce::Colours::transparentBlack);
        setScrollWheelEnabled(false);
        startTimerHz(60);
    }
    ~SmoothComboBox() override { stopTimer(); }
    void setWrapAround(bool shouldWrap) noexcept
    {
        wrapAround = shouldWrap;
        repaint();
    }
    bool getWrapAround() const noexcept { return wrapAround; }
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
            const auto region = static_cast<Region>(i);
            g.setColour(colours[(std::size_t) i].fill.get());
            g.fillRect(regionBounds(region));
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
        const auto arrowFont = KnobLookAndFeel::courier(
            juce::jlimit(9.0f, 18.0f, (float) getHeight() * 0.62f));
        g.setFont(arrowFont);
        g.setColour(colours[0].foreground.get());
        g.drawFittedText("<", left, juce::Justification::centred, 1);
        g.setColour(colours[2].foreground.get());
        g.drawFittedText(">", right, juce::Justification::centred, 1);
        auto text = getText();
        if(text.isEmpty())
            text = getTextWhenNothingSelected();
        g.setColour(colours[1].foreground.get());
        g.setFont(KnobLookAndFeel::courier(
            juce::jlimit(8.0f, 40.0f, (float) getHeight() * 0.50f)));
        g.drawFittedText(text, regionBounds(Region::title).reduced(3, 1),
                         juce::Justification::centred, 1);
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        pressedRegion = Region::none;
        dragStartIndex = getSelectedItemIndex();
        appliedDragSteps = 0;
        dragChanged = false;
        draggingTitle = false;
        if(! isEnabled() || isPopupActive() || ! e.mods.isLeftButtonDown()
            || e.mods.isPopupMenu())
            return;
        const auto region = hitTestRegion(e.getEventRelativeTo(this).getPosition());
        if(isAvailable(region))
            pressedRegion = region;
        repaint();
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if(! isEnabled() || isPopupActive() || pressedRegion != Region::title)
            return;
        draggingTitle = true;
        const auto offset = e.getOffsetFromDragStart();
        const float along = (float) offset.x - (float) offset.y;
        const float stepDistance = dragPixelsPerStep * (e.mods.isCtrlDown() ? 3.0f : 1.0f);
        const int steps = (int) (along / stepDistance);
        if(steps == appliedDragSteps)
            return;
        int target = dragStartIndex;
        const int direction = steps >= 0 ? 1 : -1;
        for(int i = 0; i < std::abs(steps); ++i)
        {
            const int next = steppedIndexFrom(target, direction);
            if(next < 0)
                break;
            target = next;
        }
        if(target >= 0 && target != getSelectedItemIndex())
            setSelectedItemIndex(target, juce::sendNotificationAsync);
        appliedDragSteps = steps;
        dragChanged = true;
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        const auto wasPressed = pressedRegion;
        pressedRegion = Region::none;
        draggingTitle = false;
        repaint();
        if(! isEnabled() || isPopupActive() || wasPressed == Region::none)
            return;
        if(hitTestRegion(e.getEventRelativeTo(this).getPosition()) != wasPressed
            || ! isAvailable(wasPressed))
            return;
        if(wasPressed == Region::title)
        {
            if(! dragChanged && ! e.mouseWasDraggedSinceMouseDown())
                openPopupSoon();
        }
        else
        {
            stepSelection(wasPressed == Region::previous ? -1 : 1);
        }
    }
    void mouseWheelMove(const juce::MouseEvent& e,
                        const juce::MouseWheelDetails& wheel) override
    {
        if(! isEnabled() || isPopupActive())
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
            stepSelection(direction);
            return;
        }
        smoothWheelAccumulator += primaryDelta * 5.0f;
        while(smoothWheelAccumulator > 1.0f)
        {
            smoothWheelAccumulator -= 1.0f;
            stepSelection(1);
        }
        while(smoothWheelAccumulator < -1.0f)
        {
            smoothWheelAccumulator += 1.0f;
            stepSelection(-1);
        }
    }
    bool keyPressed(const juce::KeyPress& key) override
    {
        if(! isEnabled())
            return false;
        if(key == juce::KeyPress::leftKey || key == juce::KeyPress::upKey)
        {
            stepSelection(-1);
            return true;
        }
        if(key == juce::KeyPress::rightKey || key == juce::KeyPress::downKey)
        {
            stepSelection(1);
            return true;
        }
        return juce::ComboBox::keyPressed(key);
    }
    void enablementChanged() override
    {
        juce::ComboBox::enablementChanged();
        if(! isEnabled())
            pressedRegion = Region::none;
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
    Region hitTestRegion(juce::Point<int> position) const
    {
        if(! getLocalBounds().contains(position))
            return Region::none;
        if(regionBounds(Region::previous).contains(position))
            return Region::previous;
        if(regionBounds(Region::next).contains(position))
            return Region::next;
        return Region::title;
    }
    int steppedIndexFrom(int selected, int direction) const
    {
        const int count = getNumItems();
        if(! isEnabled() || isPopupActive() || count == 0 || direction == 0)
            return -1;
        if(selected < 0)
        {
            for(int i = direction > 0 ? 0 : count - 1;
                 i >= 0 && i < count; i += direction)
                if(isItemEnabled(getItemId(i)))
                    return i;
            return -1;
        }
        if(wrapAround)
        {
            for(int offset = 1; offset < count; ++offset)
            {
                const int next = (selected + direction * offset + count) % count;
                if(isItemEnabled(getItemId(next)))
                    return next;
            }
        }
        else
        {
            for(int next = selected + direction;
                 next >= 0 && next < count; next += direction)
                if(isItemEnabled(getItemId(next)))
                    return next;
        }
        return -1;
    }
    int steppedIndex(int direction) const
    {
        return steppedIndexFrom(getSelectedItemIndex(), direction);
    }
    bool isAvailable(Region region) const
    {
        if(! isEnabled() || isPopupActive())
            return false;
        if(region == Region::title)
            return true;
        if(region == Region::previous)
            return steppedIndex(-1) >= 0;
        if(region == Region::next)
            return steppedIndex(1) >= 0;
        return false;
    }
    void stepSelection(int direction)
    {
        const int next = steppedIndex(direction);
        if(next >= 0)
            setSelectedItemIndex(next, juce::sendNotificationAsync);
    }
    void openPopupSoon()
    {
        juce::Component::SafePointer<SmoothComboBox> safe(this);
        juce::MessageManager::callAsync([safe]
        {
            if(safe != nullptr && safe->isShowing() && safe->isEnabled()
                && ! safe->isPopupActive())
                safe->showPopup();
        });
    }
    void timerCallback() override
    {
        const bool keepTitleHovered = isEnabled() && draggingTitle
                                   && pressedRegion == Region::title;
        const Region hovered = keepTitleHovered ? Region::title
                             : (isEnabled() && ! isPopupActive() && isMouseOver(true))
                                 ? hitTestRegion(getMouseXYRelative()) : Region::none;
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
        const bool pressed = hovered != Region::none && hovered == pressedRegion
                             && isMouseButtonDown(true);
        const auto outline = ! isEnabled() ? juce::Colour(0xff2c2c32)
                           : pressed ? juce::Colour(0xff307a7c)
                           : hovered != Region::none && isAvailable(hovered)
                                          ? juce::Colour(0xff6ecdd0)
                                          : juce::Colour(0xff4a4a54);
        smooth(baseFill, isEnabled() ? juce::Colour(0xff15151a)
                                     : juce::Colour(0xff121216));
        smooth(baseOutline, outline);
        for(int i = 0; i < 3; ++i)
        {
            const auto region = static_cast<Region>(i);
            const bool available = isAvailable(region);
            const bool hot = available && hovered == region;
            const bool down = hot && pressedRegion == region && isMouseButtonDown(true);
            const auto fill = ! available ? juce::Colour(0xff121216)
                            : down ? juce::Colour(0xff0c0c10)
                            : hot ? juce::Colour(0xff20202a)
                                          : juce::Colour(0xff15151a);
            const auto foreground = ! available ? juce::Colour(0xff44444c)
                                  : down ? juce::Colour(0xff307a7c)
                                  : hot ? juce::Colour(0xff9fe3e5)
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
    static constexpr float dragPixelsPerStep = 48.0f;
    Region pressedRegion = Region::none;
    int dragStartIndex = -1;
    int appliedDragSteps = 0;
    bool dragChanged = false;
    bool draggingTitle = false;
    juce::int64 lastWheelEventTimeMs = -1;
    float smoothWheelAccumulator = 0.0f;
    bool wrapAround = false;
    bool primed = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SmoothComboBox)
};