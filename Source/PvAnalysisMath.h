#pragma once
#include <juce_core/juce_core.h>
#include <algorithm>
#include <cmath>
namespace pv_analysis
{
inline float wrapPhase(float phase) noexcept
{
    return phase - juce::MathConstants<float>::twoPi
                 * std::round(phase / juce::MathConstants<float>::twoPi);
}
inline float fastAtan2(float y, float x) noexcept
{
    if(x == 0.0f && y == 0.0f) return 0.0f;
    const float absX = std::abs(x);
    const float absY = std::abs(y);
    const float ratio = std::min(absX, absY) / (std::max(absX, absY) + 1.0e-20f);
    const float squared = ratio * ratio;
    float angle = ((-0.0464964749f * squared + 0.15931422f) * squared - 0.327622764f)
                * squared * ratio + ratio;
    if(absY > absX) angle = 1.57079637f - angle;
    if(x < 0.0f) angle = 3.14159274f - angle;
    if(y < 0.0f) angle = -angle;
    return angle;
}
}