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