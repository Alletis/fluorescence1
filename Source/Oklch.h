#pragma once
#include <juce_graphics/juce_graphics.h>
#include <cmath>
namespace oklch
{
    struct OKLab { float L, a, b; };
    inline float srgbToLinear(float c)
    {
        return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
    }
    inline float linearToSrgb(float c)
    {
        c = juce::jlimit(0.0f, 1.0f, c);
        return c <= 0.0031308f ? 12.92f * c : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
    }
    inline OKLab toOKLab(juce::Colour col)
    {
        const float r = srgbToLinear(col.getFloatRed());
        const float g = srgbToLinear(col.getFloatGreen());
        const float b = srgbToLinear(col.getFloatBlue());
        const float l = 0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * b;
        const float m = 0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * b;
        const float s = 0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * b;
        const float l_ = std::cbrt(l), m_ = std::cbrt(m), s_ = std::cbrt(s);
        return { 0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_,
                 1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_,
                 0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_ };
    }
    inline juce::Colour fromOKLab(OKLab c)
    {
        const float l_ = c.L + 0.3963377774f * c.a + 0.2158037573f * c.b;
        const float m_ = c.L - 0.1055613458f * c.a - 0.0638541728f * c.b;
        const float s_ = c.L - 0.0894841775f * c.a - 1.2914855480f * c.b;
        const float l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;
        const float r = +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
        const float g = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
        const float b = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;
        return juce::Colour::fromFloatRGBA(linearToSrgb(r), linearToSrgb(g), linearToSrgb(b), 1.0f);
    }
    inline juce::Colour lerp(juce::Colour ca, juce::Colour cb, float t)
    {
        const OKLab a = toOKLab(ca), b = toOKLab(cb);
        const float Ca = std::sqrt(a.a * a.a + a.b * a.b);
        const float Cb = std::sqrt(b.a * b.a + b.b * b.b);
        const float ha = std::atan2 (a.b, a.a);
        const float hb = std::atan2 (b.b, b.a);
        float dh = hb - ha;
        if(dh > juce::MathConstants<float>::pi) dh -= juce::MathConstants<float>::twoPi;
        else if(dh < -juce::MathConstants<float>::pi) dh += juce::MathConstants<float>::twoPi;
        const float L = a.L + (b.L - a.L) * t;
        const float C = Ca + (Cb - Ca) * t;
        const float h = ha + dh * t;
        return fromOKLab({ L, C * std::cos(h), C * std::sin(h) });
    }
}