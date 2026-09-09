#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
struct SmoothColour
{
    float r = 0, g = 0, b = 0, a = 1;
    void set(juce::Colour c)
    {
        r = c.getFloatRed(); g = c.getFloatGreen();
        b = c.getFloatBlue(); a = c.getFloatAlpha();
    }
    juce::Colour get() const { return juce::Colour::fromFloatRGBA(r, g, b, a); }
    bool approach(juce::Colour c, float rate)
    {
        const float dr = c.getFloatRed() - r, dg = c.getFloatGreen() - g,
                    db = c.getFloatBlue() - b, da = c.getFloatAlpha() - a;
        r += dr * rate; g += dg * rate; b += db * rate; a += da * rate;
        return std::abs(dr) + std::abs(dg) + std::abs(db) + std::abs(da) > 0.002f;
    }
};
struct SmoothValue
{
    double v = 0.0;
    bool primed = false;
    void prime(double target) { v = target; primed = true; }
    bool approach(double target, double rate, double eps)
    {
        if(! primed) { v = target; primed = true; return false; }
        const double d = target - v;
        v += d * rate;
        return std::abs(d) > eps;
    }
    double get() const { return v; }
};