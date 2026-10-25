#pragma once
#include <juce_graphics/juce_graphics.h>
#include <cmath>
namespace OverlayRendering
{
    inline juce::Image makeCoverImage(const juce::Image& source, int width, int height)
    {
        if(! source.isValid() || width < 1 || height < 1) return {};
        juce::Image result(juce::Image::ARGB, width, height, true);
        juce::Graphics g(result);
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(source, juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height),
                    juce::RectanglePlacement(juce::RectanglePlacement::fillDestination | juce::RectanglePlacement::centred));
        return result;
    }
    inline juce::Image additiveBackground(const juce::Image& cover,
                                          juce::Colour underlying, float brightness)
    {
        if(! cover.isValid()) return {};
        const int w = cover.getWidth(), h = cover.getHeight();
        juce::Image result(juce::Image::RGB, w, h, false);
        const float b = juce::jlimit(0.0f, 1.0f, brightness);
        juce::Image::BitmapData src(cover, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData dst(result, juce::Image::BitmapData::writeOnly);
        for(int y = 0; y < h; ++y)
            for(int x = 0; x < w; ++x)
            {
                const auto c = src.getPixelColour(x, y);
                const float f = b * c.getFloatAlpha();
                dst.setPixelColour(x, y, juce::Colour(
                    (juce::uint8) juce::jmin(255, (int) underlying.getRed() + juce::roundToInt(c.getRed() * f)),
                    (juce::uint8) juce::jmin(255, (int) underlying.getGreen() + juce::roundToInt(c.getGreen() * f)),
                    (juce::uint8) juce::jmin(255, (int) underlying.getBlue() + juce::roundToInt(c.getBlue() * f))));
            }
        return result;
    }
}