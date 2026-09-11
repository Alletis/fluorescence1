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
    double activeTarget = 0.0;
    bool primed = false;
    bool moving = false;
    void prime(double target)
    {
        v = activeTarget = target;
        primed = true;
        moving = false;
    }
    bool approach(double target, double rate, double eps)
    {
        if(! primed)
        {
            prime(target);
            return false;
        }
        if(std::abs(target - activeTarget) > eps)
        {
            if(moving)
                v = activeTarget;
            activeTarget = target;
            moving = std::abs(activeTarget - v) > eps;
        }
        const double d = activeTarget - v;
        if(std::abs(d) <= eps)
        {
            v = activeTarget;
            moving = false;
            return false;
        }
        v += d * rate;
        if(std::abs(activeTarget - v) <= eps)
        {
            v = activeTarget;
            moving = false;
        }
        else
        {
            moving = true;
        }
        return true;
    }
    double get() const { return v; }
};