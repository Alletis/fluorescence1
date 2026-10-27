#pragma once
#include <juce_graphics/juce_graphics.h>
#include <cmath>
namespace OverlayRendering
{
    enum class BoundMode
    {
        clamp = 0,
        repeat = 1,
        mirror = 2,
        none = 3,
        transparent = none
    };
    inline juce::Image makeCoverImage(const juce::Image& source, int width, int height)
    {
        if(! source.isValid() || width < 1 || height < 1) return {};
        const double scale = juce::jmax((double) width / (double) source.getWidth(),
                                        (double) height / (double) source.getHeight());
        const int fittedWidth = juce::jmax(width,
            juce::roundToInt((double) source.getWidth() * scale));
        const int fittedHeight = juce::jmax(height,
            juce::roundToInt((double) source.getHeight() * scale));
        juce::Image result(juce::Image::ARGB, fittedWidth, fittedHeight, true);
        juce::Graphics g(result);
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(source, result.getBounds().toFloat());
        return result;
    }
    inline float positiveModulo(float value, float period)
    {
        if(period <= 0.0f) return 0.0f;
        float result = std::fmod(value, period);
        if(result < 0.0f) result += period;
        return result;
    }
    inline float mapCoordinate(float value, int length, BoundMode mode)
    {
        if(length <= 1) return 0.0f;
        const float last = (float) (length - 1);
        switch(mode)
        {
            case BoundMode::repeat:
                return positiveModulo(value, (float) length);
            case BoundMode::mirror:
            {
                const float period = 2.0f * last;
                const float p = positiveModulo(value, period);
                return p <= last ? p : period - p;
            }
            case BoundMode::none:
            case BoundMode::clamp:
            default:
                return juce::jlimit(0.0f, last, value);
        }
    }
    inline juce::Colour sampleBilinear(juce::Image::BitmapData& src,
                                       float x, float y, BoundMode mode)
    {
        const int width = src.width;
        const int height = src.height;
        if(width < 1 || height < 1) return juce::Colours::transparentBlack;
        if(mode == BoundMode::none
            && (x < 0.0f || y < 0.0f || x > (float) (width - 1)
                || y > (float) (height - 1)))
            return juce::Colours::transparentBlack;
        x = mapCoordinate(x, width, mode);
        y = mapCoordinate(y, height, mode);
        const int x0 = juce::jlimit(0, width - 1, (int) std::floor(x));
        const int y0 = juce::jlimit(0, height - 1, (int) std::floor(y));
        const float fx = x - (float) x0;
        const float fy = y - (float) y0;
        const int x1 = mode == BoundMode::repeat ? (x0 + 1) % width
                                                  : juce::jmin(width - 1, x0 + 1);
        const int y1 = mode == BoundMode::repeat ? (y0 + 1) % height
                                                  : juce::jmin(height - 1, y0 + 1);
        const auto c00 = src.getPixelColour(x0, y0);
        const auto c10 = src.getPixelColour(x1, y0);
        const auto c01 = src.getPixelColour(x0, y1);
        const auto c11 = src.getPixelColour(x1, y1);
        const auto lerp = [fx, fy](float a, float b, float c, float d)
        {
            const float top = a + (b - a) * fx;
            const float bottom = c + (d - c) * fx;
            return top + (bottom - top) * fy;
        };
        const float a00 = c00.getFloatAlpha(), a10 = c10.getFloatAlpha();
        const float a01 = c01.getFloatAlpha(), a11 = c11.getFloatAlpha();
        const float alpha = juce::jlimit(0.0f, 1.0f, lerp(a00, a10, a01, a11));
        const float red = lerp(c00.getFloatRed() * a00, c10.getFloatRed() * a10,
                               c01.getFloatRed() * a01, c11.getFloatRed() * a11);
        const float green = lerp(c00.getFloatGreen() * a00, c10.getFloatGreen() * a10,
                                 c01.getFloatGreen() * a01, c11.getFloatGreen() * a11);
        const float blue = lerp(c00.getFloatBlue() * a00, c10.getFloatBlue() * a10,
                                c01.getFloatBlue() * a01, c11.getFloatBlue() * a11);
        if(alpha <= 0.00001f) return juce::Colours::transparentBlack;
        return juce::Colour::fromFloatRGBA(
            juce::jlimit(0.0f, 1.0f, red / alpha),
            juce::jlimit(0.0f, 1.0f, green / alpha),
            juce::jlimit(0.0f, 1.0f, blue / alpha), alpha);
    }
    inline juce::Image transformImage(const juce::Image& source, int width, int height,
                                      float resize, float translateX, float translateY,
                                      BoundMode boundMode)
    {
        if(! source.isValid() || width < 1 || height < 1) return {};
        if(! source.isValid()) return {};
        if(! std::isfinite(resize) || resize <= 0.0f) resize = 1.0f;
        const double destinationCentreX = ((double) width - 1.0) * 0.5;
        const double destinationCentreY = ((double) height - 1.0) * 0.5;
        const double sourceCentreX = ((double) source.getWidth() - 1.0) * 0.5;
        const double sourceCentreY = ((double) source.getHeight() - 1.0) * 0.5;
        const double minSourceX = (-destinationCentreX - translateX) / resize + sourceCentreX;
        const double maxSourceX = (destinationCentreX - translateX) / resize + sourceCentreX;
        const double minSourceY = (-destinationCentreY - translateY) / resize + sourceCentreY;
        const double maxSourceY = (destinationCentreY - translateY) / resize + sourceCentreY;
        const bool samplesInsideImage = minSourceX >= 0.0
                                     && maxSourceX <= (double) (source.getWidth() - 1)
                                     && minSourceY >= 0.0
                                     && maxSourceY <= (double) (source.getHeight() - 1);
        if(boundMode == BoundMode::none
            || (boundMode == BoundMode::clamp && samplesInsideImage))
        {
            juce::Image result(juce::Image::ARGB, width, height, true);
            juce::Graphics g(result);
            g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
            const float offsetX = (float) (destinationCentreX + translateX
                                           - resize * sourceCentreX);
            const float offsetY = (float) (destinationCentreY + translateY
                                           - resize * sourceCentreY);
            g.drawImageTransformed(
                source, juce::AffineTransform(resize, 0.0f, offsetX,
                                              0.0f, resize, offsetY));
            return result;
        }
        juce::Image result(juce::Image::ARGB, width, height, true);
        juce::Image::BitmapData src(source, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData dst(result, juce::Image::BitmapData::writeOnly);
        for(int y = 0; y < height; ++y)
            for(int x = 0; x < width; ++x)
            {
                const double sx = ((double) x - destinationCentreX - translateX) / resize
                                + sourceCentreX;
                const double sy = ((double) y - destinationCentreY - translateY) / resize
                                + sourceCentreY;
                const float sampleX = (float) juce::jlimit(-1.0e30, 1.0e30, sx);
                const float sampleY = (float) juce::jlimit(-1.0e30, 1.0e30, sy);
                dst.setPixelColour(x, y, sampleBilinear(src, sampleX, sampleY, boundMode));
            }
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