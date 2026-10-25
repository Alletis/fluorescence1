#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "OverlayAssets.h"
#include "SmoothTextButton.h"
#include "ValueKnob.h"
#include "KnobLookAndFeel.h"
#include <array>
#include <functional>
class OverlaySettingsPage final : public juce::Component
{
public:
    using Slot = FluorescenceOverlayAssets::Slot;
    std::function<void(Slot)> onLoadImage;
    std::function<void(Slot)> onImageUnavailable;
    int offset = 26;
    OverlaySettingsPage()
    {
        for(int i = 0; i < 2; ++i)
        {
            auto& r = rows[(size_t) i];
            r.toggle.setButtonText("OVERLAY OFF");
            r.toggle.setMagentaAccent(i == 1);
            r.load.setButtonText("LOAD");
            r.load.setMagentaAccent(i == 1);
            addAndMakeVisible(r.toggle);
            addAndMakeVisible(r.load);
            for(auto* slider : { &r.bright, &r.blur })
            {
                slider->setSliderStyle(juce::Slider::LinearHorizontal);
                slider->setSliderSnapsToMousePosition(false);
                slider->setRange(0.0, 1.0, 0.0);
                slider->setDisplayDecimals(2);
                slider->setMagentaAccent(i == 1);
                slider->setLinearFillEndCap(ValueKnob::LinearEndCap::LeftRounded);
                slider->setDragSmoothing(1.0f);
                addAndMakeVisible(*slider);
            }
            const auto slot = static_cast<Slot>(i);
            r.toggle.onClick = [this, slot]
            {
                if(assets == nullptr) return;
                const bool next = ! assets->getSettings(slot).enabled;
                if(next)
                {
                    const auto snapshot = assets->getSnapshot(slot);
                    if(! snapshot.settings.imageFile.existsAsFile() || ! snapshot.validImage)
                    {
                        if(onImageUnavailable) onImageUnavailable(slot);
                        updateToggle(slot);
                        return;
                    }
                }
                assets->setEnabled(slot, next);
                updateToggle(slot);
            };
            r.load.onClick = [this, slot] { if(onLoadImage) onLoadImage(slot); };
            r.bright.onValueChange = [this, slot, i]
            {
                if(assets != nullptr)
                    assets->setBrightness(slot, (float) rows[(size_t) i].bright.getValue(),
                                          ! rows[(size_t) i].bright.isMouseButtonDown(true));
            };
            r.bright.onDragEnd = [this, slot, i]
            {
                if(assets != nullptr)
                    assets->setBrightness(slot, (float) rows[(size_t) i].bright.getValue(), true);
            };
            r.blur.onValueChange = [this, slot, i]
            {
                if(assets == nullptr) return;
                assets->setBlur(slot, (float) rows[(size_t) i].blur.getValue());
                if(! rows[(size_t) i].blur.isMouseButtonDown(true))
                    assets->commitBlur(slot);
            };
            r.blur.onDragEnd = [this, slot] { if(assets != nullptr) assets->commitBlur(slot); };
        }
    }
    void bind(FluorescenceOverlayAssets& manager)
    {
        assets = &manager;
        refresh();
    }
    void refresh()
    {
        if(assets == nullptr) return;
        for(int i = 0; i < 2; ++i)
        {
            const auto slot = static_cast<Slot>(i);
            const auto s = assets->getSettings(slot);
            auto& r = rows[(size_t) i];
            updateToggle(slot);
            if(! r.bright.isMouseButtonDown(true))
                r.bright.setValue(s.brightness, juce::dontSendNotification);
            if(! r.blur.isMouseButtonDown(true))
                r.blur.setValue(s.blur, juce::dontSendNotification);
        }
        repaint();
    }
    void paint(juce::Graphics& g) override
    {
        const auto font = KnobLookAndFeel::courier(11.0f);
        g.setFont(font);
        const int h = getHeight() / 2;
        for(int i = 0; i < 2; ++i)
        {
            const int top = i * h;
            const int visualTop = top + (i == 0 ? offset : 0);
            g.setColour(juce::Colour(i == 0 ? 0xff7fc7ca : 0xffefa7f3));
            g.drawText(i == 0 ? "BACKGROUND" : "SPECTRUM", 5, visualTop, getWidth() - 10, 14,
                       juce::Justification::centredLeft);
            g.setColour(juce::Colour(0xffb0b0b8));
            g.drawText("BRIGHT", 6, visualTop + 40, 58, 18, juce::Justification::centredLeft);
            g.drawText("BLUR", 6, visualTop + 59, 58, 18, juce::Justification::centredLeft);
            if(i == 0)
            {
                g.setColour(juce::Colour(0xff34343e));
                g.drawHorizontalLine(top + h - 2, 4.0f, (float) getWidth() - 4.0f);
            }
        }
    }
    void resized() override
    {
        const int h = getHeight() / 2;
        const int buttonGap = 8;
        const int rightW = juce::jmax(60, getWidth() / 3);
        for(int i = 0; i < 2; ++i)
        {
            auto& r = rows[(size_t) i];
            const int y = i * h + (i == 0 ? offset : 0);
            r.toggle.setBounds(5, y + 15, getWidth() - rightW - buttonGap - 10, 23);
            r.load.setBounds(getWidth() - rightW - 5, y + 15, rightW, 23);
            r.bright.setBounds(65, y + 42, getWidth() - 71, 16);
            r.blur.setBounds(65, y + 61, getWidth() - 71, 16);
            matchDragToVisualTravel(r.bright);
            matchDragToVisualTravel(r.blur);
        }
    }
private:
    static void matchDragToVisualTravel(ValueKnob& slider)
    {
        const auto track = slider.getLocalBounds().toFloat().reduced(4.0f);
        const float handleWidth = juce::jmax(9.0f, track.getHeight() * 0.5f);
        slider.setMouseDragSensitivity(juce::jmax(
            1, juce::roundToInt(track.getWidth() - handleWidth)));
    }
    struct Row
    {
        SmoothTextButton toggle, load;
        ValueKnob bright, blur;
    };
    void updateToggle(Slot slot)
    {
        if(assets == nullptr) return;
        auto& b = rows[(size_t) static_cast<int>(slot)].toggle;
        const bool enabled = assets->getSettings(slot).enabled;
        b.setButtonText(enabled ? "OVERLAY ON" : "OVERLAY OFF");
        b.setActive(enabled);
    }
    FluorescenceOverlayAssets* assets = nullptr;
    std::array<Row, 2> rows;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OverlaySettingsPage)
};