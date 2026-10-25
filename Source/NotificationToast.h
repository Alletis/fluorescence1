#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
class NotificationToast final : public juce::Component,
                                private juce::Timer
{
public:
    enum class Kind { critical, warning, info };
    NotificationToast()
    {
        setInterceptsMouseClicks(false, false);
        setVisible(false);
        setOpaque(false);
    }
    ~NotificationToast() override { stopTimer(); }
    void show(Kind newKind, juce::String newTitle, juce::String newBody = {})
    {
        kind = newKind;
        title = newTitle.trim();
        body = newBody.trim();
        startingOpacity = opacity;
        startedAtMs = juce::Time::getMillisecondCounterHiRes();
        setVisible(true);
        toFront(false);
        startTimerHz(60);
        repaint();
    }
    void paint(juce::Graphics& g) override
    {
        if(opacity <= 0.001f || getWidth() < 100 || getHeight() < 100)
            return;
        const float width = juce::jmin(488.0f, (float) getWidth() - 32.0f);
        const auto panel = juce::Rectangle<float>(width, 54.0f).withCentre(
            { (float) getWidth() * 0.5f, (float) getHeight() * 0.5f });
        juce::Colour background, outline, heading;
        switch(kind)
        {
            case Kind::critical:
                background = juce::Colour(0xff371d26);
                outline = juce::Colour(0xffdf6773);
                heading = juce::Colour(0xffffa5ad);
                break;
            case Kind::warning:
                background = juce::Colour(0xff38301d);
                outline = juce::Colour(0xffd4ae56);
                heading = juce::Colour(0xffffd884);
                break;
            case Kind::info:
                background = juce::Colour(0xff182c3d);
                outline = juce::Colour(0xff568bae);
                heading = juce::Colour(0xffa6d4ee);
                break;
        }
        g.setColour(background.withAlpha(0.48f * opacity));
        g.fillRoundedRectangle(panel, 10.0f);
        g.setColour(outline.withMultipliedAlpha(opacity));
        g.drawRoundedRectangle(panel.reduced(0.6f), 10.0f, 1.2f);
        const auto message = body.isEmpty() ? title : title + ": " + body;
        const auto inner = panel.toNearestInt().reduced(20, 10);
        g.setColour(heading.withMultipliedAlpha(opacity));
        g.setFont(juce::Font(juce::FontOptions("Courier New", 14.0f, juce::Font::bold)));
        g.drawFittedText(message, inner, juce::Justification::centred, 1, 0.6f);
    }
private:
    static float cosineEase(float t) noexcept
    {
        t = juce::jlimit(0.0f, 1.0f, t);
        return 0.5f - 0.5f * std::cos(t * juce::MathConstants<float>::pi);
    }
    void timerCallback() override
    {
        constexpr double fadeInMs = 100.0;
        constexpr double fadeOutMs = 300.0;
        const double holdMs = kind == Kind::critical ? 1000.0
                            : kind == Kind::warning ? 700.0 : 500.0;
        const double elapsed = juce::Time::getMillisecondCounterHiRes() - startedAtMs;
        if(elapsed < fadeInMs)
            opacity = startingOpacity + (1.0f - startingOpacity)
                                    * cosineEase((float) (elapsed / fadeInMs));
        else if(elapsed < fadeInMs + holdMs)
            opacity = 1.0f;
        else if(elapsed < fadeInMs + holdMs + fadeOutMs)
            opacity = 1.0f - cosineEase((float) ((elapsed - fadeInMs - holdMs) / fadeOutMs));
        else
        {
            opacity = 0.0f;
            stopTimer();
            setVisible(false);
        }
        repaint();
    }
    Kind kind = Kind::info;
    juce::String title, body;
    float opacity = 0.0f;
    float startingOpacity = 0.0f;
    double startedAtMs = 0.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NotificationToast)
};