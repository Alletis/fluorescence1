#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "SmoothColour.h"
#include "Oklch.h"
#include "KnobLookAndFeel.h"
#include <cmath>
#include <cstdlib>
#include <array>
#include <atomic>
class SpectrumDisplay : public juce::Component,
                        private juce::Timer
{
public:
    SpectrumDisplay(juce::AudioProcessorValueTreeState& state,
                     NewProjectAudioProcessor::SpectrumBridgeType& bridgeIn)
        : apvts(state), bridge(bridgeIn)
    {
        centerParam = apvts.getParameter("detectCenter");
        spreadParam = apvts.getParameter("detectSpread");
        attractionParam = apvts.getParameter("attraction");
        bypassParam = apvts.getParameter("pvBypass");
        midiParam = apvts.getParameter("midiMode");
        targetModeParam = apvts.getParameter("targetMode");
        for(int i = 0; i < 12; ++i)
            noteParam[i] = apvts.getParameter("note" + juce::String(i));
        heatImage = juce::Image(juce::Image::ARGB, 64, 1, true);
        setWantsKeyboardFocus(true);
        startTimerHz(60);
    }
    ~SpectrumDisplay() override { stopTimer(); }
    void setMidiMask(const std::atomic<int>* m) { midiMask = m; }
    void setSidechainTargetCount(const std::atomic<int>* c) { scTargetCount = c; }
    void setForceTargetColour(bool b) { forceTargetColour = b; }
    void paint(juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        const float W = r.getWidth();
        const float H = r.getHeight();
        g.fillAll(juce::Colour(0xff0a0a0c));
        g.drawImage(heatImage,
                     juce::Rectangle<float> (0.0f, 0.0f, W, H),
                     juce::RectanglePlacement::stretchToFit);
        const float centerHz = dispCenterHz();
        const float spread = dispSpreadVal();
        const float lowHz = centerHz / std::pow(2.0f, spread);
        const float highHz = centerHz * std::pow(2.0f, spread);
        const float xLow = freqToX(lowHz, W);
        const float xHigh = freqToX(highHz, W);
        const juce::Colour thresholdCol = thresholdLineColour();
        drawInwardGlow(g, xLow, xHigh, H, thresholdCol);
        drawInwardGlow(g, xHigh, xLow, H, thresholdCol);
        g.setColour(thresholdCol.withAlpha(thresholdLineAlpha));
        g.fillRect(xLow - 1.0f, 0.0f, 2.0f, H);
        g.fillRect(xHigh - 1.0f, 0.0f, 2.0f, H);
        const juce::Colour baseCol = baseLineColour();
        const float stripAlphaScale = 1.0f - bypassVisualAmt;
        for(int i = 0; i < snapshot.numBases; ++i)
        {
            const float f = snapshot.baseHz[(size_t) i];
            const float conf = snapshot.baseConf[(size_t) i];
            if(f <= 0.0f || conf < 0.05f) continue;
            if(stripAlphaScale < 0.02f)
                continue;
            const float a = juce::jlimit(0.0f, 1.0f, conf * stripAlphaScale);
            const float x = freqToX(f, W);
            for(int gx = 3; gx >= 1; --gx)
            {
                g.setColour(baseCol.withAlpha(0.12f * (float) gx * a));
                g.fillRect(x - (float) gx, 0.0f, 2.0f * (float) gx, H);
            }
            g.setColour(baseCol.withAlpha(a));
            g.fillRect(x - 1.0f, 0.0f, 2.0f, H);
        }
        const float cx = freqToX(centerHz, W);
        const float cy = spreadToY(spread, H);
        const float rad = 9.0f * 0.67f;
        const float stroke = rad * 0.33f;
        const float haloPad = rad * 0.45f;
        const float fillAlpha = juce::jlimit(0.0f, 1.0f, dragFillAmt);
        const float ringAlpha = 1.0f - fillAlpha;
        if(selected != Handle::none)
        {
            g.setColour(juce::Colours::white.withAlpha(0.25f));
            g.fillEllipse(cx - rad - haloPad, cy - rad - haloPad,
                           2.0f * (rad + haloPad), 2.0f * (rad + haloPad));
        }
        if(fillAlpha > 0.001f)
        {
            g.setColour(juce::Colour(0xfff0f0f0).withAlpha(fillAlpha));
            g.fillEllipse(cx - rad, cy - rad, 2.0f * rad, 2.0f * rad);
        }
        if(ringAlpha > 0.001f)
        {
            g.setColour(juce::Colour(0xfff0f0f0).withAlpha(ringAlpha));
            g.drawEllipse(cx - rad, cy - rad, 2.0f * rad, 2.0f * rad, stroke);
        }
        if(selected != Handle::none)
        {
            g.setColour(juce::Colours::black.withAlpha(0.7f));
            g.fillRect(4.0f, 4.0f, 168.0f, 22.0f);
            g.setColour(juce::Colours::white);
            g.setFont(KnobLookAndFeel::courier(KnobLookAndFeel::uiFont));
            const juce::String label = (selected == Handle::center ? "centre " : "spread ");
            g.drawText(label + entryBuffer + "_",
                        8, 4, 160, 22, juce::Justification::centredLeft);
        }
    }
    float coordOverlayGetAlpha() const noexcept { return coordOverlayAlpha; }
    juce::String coordOverlayGetText() const
    {
        return "(" + juce::String(dispCenterHz(), 1) + " Hz, "
                   + juce::String(dispSpreadVal(), 2) + " spread)";
    }
    void mouseEnter(const juce::MouseEvent&) override
    {
        mouseOverSpectrum = true;
        scheduleCoordOverlayShow();
    }
    void mouseExit(const juce::MouseEvent&) override
    {
        mouseOverSpectrum = false;
        coordOverlayTargetAlpha = 0.0f;
        ++coordOverlayToken;
        repaint();
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        if(e.mods.isPopupMenu())
        {
            juce::PopupMenu menu;
            menu.addItem(1, "Type value: Centre", true, selected == Handle::center);
            menu.addItem(2, "Type value: Spread", true, selected == Handle::spread);
            menu.setLookAndFeel(&getLookAndFeel());
            menu.showMenuAsync(juce::PopupMenu::Options{}
                                    .withTargetComponent(this)
                                    .withStandardItemHeight(26),
                                [this] (int choice)
            {
                if(choice == 1) { selected = Handle::center; typing = true; }
                else if(choice == 2) { selected = Handle::spread; typing = true; }
                entryBuffer.clear();
                repaint();
            });
            return;
        }
        if(typing || selected != Handle::none)
        {
            typing = false;
            selected = Handle::none;
            entryBuffer.clear();
            repaint();
        }
        dragging = true;
        anchorCenter = getCenterHz();
        anchorSpread = getSpread();
        anchorPos = e.position;
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if(! dragging) return;
        const float W = (float) getWidth();
        const float H = (float) getHeight();
        const float dx = e.position.x - anchorPos.x;
        const float dy = e.position.y - anchorPos.y;
        setCenterHz(xToFreq(freqToX(anchorCenter, W) + dx, W));
        setSpread(yToSpread(spreadToY(anchorSpread, H) + dy, H));
        revealCoordOverlay();
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override { dragging = false; }
    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        const bool both = (selected == Handle::none);
        if(both || selected == Handle::center) resetParam(centerParam);
        if(both || selected == Handle::spread) resetParam(spreadParam);
        revealCoordOverlay();
        repaint();
    }
    bool keyPressed(const juce::KeyPress& key) override
    {
        if(selected == Handle::none) return false;
        const auto c = key.getTextCharacter();
        if(key == juce::KeyPress::returnKey)
        {
            commitEntry();
            return true;
        }
        if(key == juce::KeyPress::escapeKey)
        {
            typing = false;
            selected = Handle::none;
            entryBuffer.clear();
            repaint();
            return true;
        }
        if(key == juce::KeyPress::backspaceKey)
        {
            if(entryBuffer.isNotEmpty())
                entryBuffer = entryBuffer.dropLastCharacters(1);
            typing = true; repaint();
            return true;
        }
        if((c >= '0' && c <= '9') || c == '.' || c == '-')
        {
            entryBuffer += juce::String::charToString(c);
            typing = true; repaint();
            return true;
        }
        return false;
    }
    void focusLost(juce::Component::FocusChangeType) override
    {
        if(typing || selected != Handle::none)
        {
            typing = false;
            selected = Handle::none;
            entryBuffer.clear();
            repaint();
        }
    }
private:
    enum class Handle { none, center, spread };
    using Snapshot = NewProjectAudioProcessor::SpectrumBridgeType::Snapshot;
    static constexpr float fMin = 20.0f;
    static constexpr float minSpread = 0.25f;
    static constexpr float maxSpread = 5.0f;
    static constexpr float splatHalfPx = 1.0f;
    static constexpr float splatFlatPx = 0.50f;
    static constexpr float thresholdLineAlpha = 0.5f;
    static constexpr float thresholdGlowAlpha = 0.14f;
    static constexpr float smoothRateThird = 1.0f / 3.0f;
    std::vector<float> peak, bright, src;
    void resized() override
    {
        const int w = juce::jmax(1, getWidth());
        if(heatImage.getWidth() != w)
        {
            heatImage = juce::Image(juce::Image::ARGB, w, 1, true);
            peak.assign((size_t) w, 0.0f);
            bright.assign((size_t) w, 0.0f);
            src.assign((size_t) w, 0.0f);
            rebuildImage();
        }
    }
    static float splatProfile(float dist) noexcept
    {
        if(dist <= splatFlatPx) return 1.0f;
        if(dist >= splatHalfPx) return 0.0f;
        const float t = (dist - splatFlatPx) / (splatHalfPx - splatFlatPx);
        return 1.0f - t * t * (3.0f - 2.0f * t);
    }
    void timerCallback() override
    {
        const bool isNew = bridge.read(snapshot);
        if(isNew)
        {
            fMax = (snapshot.sampleRate > 0.0)
                     ? juce::jmin(20000.0f, (float) (snapshot.sampleRate * 0.5))
                     : 20000.0f;
            rebuildImage();
        }
        const float tgtC = getCenterHz();
        const float tgtS = getSpread();
        if(! dispPrimed) { dispCenter = tgtC; dispSpread = tgtS; dispPrimed = true; }
        else
        {
            dispCenter += (tgtC - dispCenter) * (1.0f / 1.4f);
            dispSpread += (tgtS - dispSpread) * (1.0f / 1.4f);
        }
        const float tgtThresholdBlend = targetAttractionBlend();
        if(! thresholdBlendPrimed)
        {
            thresholdBlendAmt = tgtThresholdBlend;
            thresholdBlendPrimed = true;
        }
        else
        {
            thresholdBlendAmt += (tgtThresholdBlend - thresholdBlendAmt) * smoothRateThird;
        }
        const bool bypassed = (bypassParam == nullptr || bypassParam->getValue() < 0.5f);
        const float tgtBypass = bypassed ? 1.0f : 0.0f;
        if(! bypassVisualPrimed)
        {
            bypassVisualAmt = tgtBypass;
            bypassVisualPrimed = true;
        }
        else
        {
            bypassVisualAmt += (tgtBypass - bypassVisualAmt) * smoothRateThird;
        }
        dragFillAmt += ((dragging ? 1.0f : 0.0f) - dragFillAmt) * smoothRateThird;
        coordOverlayAlpha += (coordOverlayTargetAlpha - coordOverlayAlpha) * smoothRateThird;
        coordOverlayAlpha = juce::jlimit(0.0f, 1.0f, coordOverlayAlpha);
        repaint();
    }
    void rebuildImage()
    {
        const int W = heatImage.getWidth();
        if(W <= 0 || (int) peak.size() != W) return;
        std::fill(peak.begin(), peak.end(), 0.0f);
        const int reach = (int) std::ceil(splatHalfPx);
        for(int j = 0; j < snapshot.numBins; ++j)
        {
            const float f = snapshot.freq[(size_t) j];
            if(f <= 0.0f) continue;
            const float pos = freqPos(f);
            if(pos < 0.0f || pos > 1.0f) continue;
            const float m = snapshot.mag[(size_t) j];
            const float fx = pos * (float) (W - 1);
            const int c = (int) std::floor(fx);
            for(int d = -reach; d <= reach + 1; ++d)
            {
                const int px = c + d;
                if(px < 0 || px >= W) continue;
                const float wgt = splatProfile(std::abs((float) px - fx));
                if(wgt <= 0.0f) continue;
                peak[(size_t) px] = std::max(peak[(size_t) px], m * wgt);
            }
        }
        for(int x = 0; x < W; ++x)
        {
            const float amp = juce::jmax(0.0f, peak[(size_t) x]);
            const float db = 20.0f * std::log10 (juce::jmax(amp, 1.0e-9f));
            const float norm = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
            bright[(size_t) x] = norm * std::sqrtf(norm);
        }
        constexpr int R = 8;
        constexpr float sigma = 4.0f;
        constexpr float glowExp = 3.0f;
        constexpr float glowGain = 0.6f;
        std::array<float, (size_t) (2 * R + 1)> kernel {};
        for(int k = -R; k <= R; ++k)
            kernel[(size_t) (k + R)] = std::exp(-(float) (k * k) / (2.0f * sigma * sigma));
        for(int x = 0; x < W; ++x)
            src[(size_t) x] = std::pow(bright[(size_t) x], glowExp);
        juce::Image::BitmapData bd(heatImage, juce::Image::BitmapData::writeOnly);
        for(int x = 0; x < W; ++x)
        {
            float glow = 0.0f;
            for(int k = -R; k <= R; ++k)
            {
                const int sx = x + k;
                if(sx < 0 || sx >= W) continue;
                glow += kernel[(size_t) (k + R)] * src[(size_t) sx];
            }
            const float g = juce::jlimit(0.0f, 1.0f, glowGain * glow);
            const float b = bright[(size_t) x];
            const float lit = b + (1.0f - b) * g;
            const auto v = (juce::uint8) juce::jlimit(0, 255, (int) std::lround(lit * 255.0f));
            bd.setPixelColour(x, 0, juce::Colour(v, v, v));
        }
    }
    void drawInwardGlow(juce::Graphics& g, float edgeX, float towardX, float H,
                         juce::Colour colour)
    {
        const float band = std::abs(towardX - edgeX);
        const float dir = (towardX > edgeX) ? 1.0f : -1.0f;
        const float glowW = juce::jmin(44.0f, band * 0.5f);
        const float intensity = juce::jmin(thresholdGlowAlpha, thresholdLineAlpha)
                      * juce::jlimit(0.0f, 1.0f, band / 80.0f);
        if(glowW < 1.0f || intensity <= 0.001f) return;
        const float x0 = edgeX;
        const float x1 = edgeX + dir * glowW;
        juce::ColourGradient grad(colour.withAlpha(intensity), x0, 0.0f,
                                   colour.withAlpha(0.0f), x1, 0.0f, false);
        g.setGradientFill(grad);
        g.fillRect(juce::Rectangle<float> (juce::Point<float> (std::min(x0, x1), 0.0f),
                                            juce::Point<float> (std::max(x0, x1), H)));
    }
    float freqPos(float f) const
    {
        f = juce::jlimit(fMin, fMax, f);
        return std::log(f / fMin) / std::log(fMax / fMin);
    }
    float freqToX(float f, float W) const { return juce::jlimit(0.0f, 1.0f, freqPos(f)) * W; }
    float xToFreq(float x, float W) const
    {
        const float pos = juce::jlimit(0.0f, 1.0f, x / W);
        return fMin * std::pow(fMax / fMin, pos);
    }
    float spreadToY(float s, float H) const
    {
        const float t = (juce::jlimit(minSpread, maxSpread, s) - minSpread) / (maxSpread - minSpread);
        return H - t * H;
    }
    float yToSpread(float y, float H) const
    {
        const float t = juce::jlimit(0.0f, 1.0f, 1.0f - y / H);
        return minSpread + t * (maxSpread - minSpread);
    }
    float getCenterHz() const
    {
        return centerParam != nullptr
                 ? centerParam->convertFrom0to1 (centerParam->getValue()) : 640.0f;
    }
    float getSpread() const
    {
        return spreadParam != nullptr
                 ? spreadParam->convertFrom0to1 (spreadParam->getValue()) : 2.5f;
    }
    void setCenterHz(float hz) { setParam(centerParam, hz); }
    void setSpread(float s) { setParam(spreadParam, s); }
    float dispCenterHz() const { return dispPrimed ? dispCenter : getCenterHz(); }
    float dispSpreadVal() const { return dispPrimed ? dispSpread : getSpread(); }
    static void setParam(juce::RangedAudioParameter* p, float realValue)
    {
        if(p == nullptr) return;
        const auto& range = p->getNormalisableRange();
        const float v = range.snapToLegalValue(realValue);
        p->setValueNotifyingHost(range.convertTo0to1 (v));
    }
    static void resetParam(juce::RangedAudioParameter* p)
    {
        if(p != nullptr) p->setValueNotifyingHost(p->getDefaultValue());
    }
    void commitEntry()
    {
        float value = 0.0f;
        if(parseFloat(entryBuffer, value))
        {
            setParam(selected == Handle::center ? centerParam : spreadParam, value);
            revealCoordOverlay();
        }
        typing = false;
        selected = Handle::none;
        entryBuffer.clear();
        repaint();
    }
    void scheduleCoordOverlayShow()
    {
        const int token = ++coordOverlayToken;
        juce::Timer::callAfterDelay(250, [safe = juce::Component::SafePointer<SpectrumDisplay> (this), token]
        {
            if(safe == nullptr)
                return;
            if(token != safe->coordOverlayToken)
                return;
            if(! safe->mouseOverSpectrum)
                return;
            safe->coordOverlayTargetAlpha = 1.0f;
            safe->repaint();
        });
    }
    void revealCoordOverlay()
    {
        coordOverlayAlpha = 1.0f;
        coordOverlayTargetAlpha = 1.0f;
        repaint();
    }
    static bool parseFloat(const juce::String& text, float& out)
    {
        const auto s = text.trim();
        if(s.isEmpty()) return false;
        const auto utf8 = s.toRawUTF8();
        char* end = nullptr;
        const double v = std::strtod(utf8, &end);
        if(end == utf8 || *end != '\0') return false;
        out = (float) v;
        return true;
    }
    bool midiOn() const { return targetModeParam != nullptr
                              && juce::roundToInt(targetModeParam->getValue() * 2.0f) == 1; }
    bool anyNoteSelected() const
    {
        if(midiOn() && midiMask != nullptr)
            return midiMask->load() != 0;
        for(auto* p : noteParam)
            if(p != nullptr && p->getValue() >= 0.5f)
                return true;
        return false;
    }
    juce::Colour baseLineColour() const
    {
        return oklch::lerp(juce::Colour(0xff45aeb1), juce::Colour(0xffeb8fff),
                            targetAttractionBlend());
    }
    juce::Colour thresholdLineColour() const
    {
        const auto activeCol = oklch::lerp(juce::Colour(0xff45aeb1), juce::Colour(0xffeb8fff),
                                            juce::jlimit(0.0f, 1.0f, thresholdBlendAmt));
        return oklch::lerp(activeCol, juce::Colour(0xff8a8a92), juce::jlimit(0.0f, 1.0f, bypassVisualAmt));
    }
    float targetAttractionBlend() const
    {
        if(forceTargetColour) return 1.0f;
        const float attract = attractionParam != nullptr ? attractionParam->getValue() : 0.0f;
        const bool hasTarget = sidechainModeOn() ? sidechainHasTargets()
                                                 : anyNoteSelected();
        return hasTarget ? attract * attract : 0.0f;
    }
    bool sidechainModeOn() const
    {
        return targetModeParam != nullptr
            && juce::roundToInt(targetModeParam->getValue() * 2.0f) == 2;
    }
    bool sidechainHasTargets() const
    {
        return scTargetCount != nullptr && scTargetCount->load() > 0;
    }
    juce::AudioProcessorValueTreeState& apvts;
    NewProjectAudioProcessor::SpectrumBridgeType& bridge;
    juce::RangedAudioParameter* centerParam = nullptr;
    juce::RangedAudioParameter* spreadParam = nullptr;
    juce::RangedAudioParameter* attractionParam = nullptr;
    juce::RangedAudioParameter* bypassParam = nullptr;
    juce::RangedAudioParameter* midiParam = nullptr;
    const std::atomic<int>* midiMask = nullptr;
    juce::RangedAudioParameter* noteParam[12] = { nullptr };
    juce::RangedAudioParameter* targetModeParam = nullptr;
    const std::atomic<int>* scTargetCount = nullptr;
    bool forceTargetColour = false;
    juce::Image heatImage;
    Snapshot snapshot;
    float fMax = 20000.0f;
    bool dragging = false;
    juce::Point<float> anchorPos;
    float anchorCenter = 640.0f;
    float anchorSpread = 2.5f;
    float dispCenter = 640.0f;
    float dispSpread = 2.5f;
    bool dispPrimed = false;
    float dragFillAmt = 0.0f;
    float thresholdBlendAmt = 0.0f;
    bool thresholdBlendPrimed = false;
    float bypassVisualAmt = 0.0f;
    bool bypassVisualPrimed = false;
    bool mouseOverSpectrum = false;
    float coordOverlayAlpha = 0.0f;
    float coordOverlayTargetAlpha = 0.0f;
    int coordOverlayToken = 0;
    Handle selected = Handle::none;
    bool typing = false;
    juce::String entryBuffer;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumDisplay)
};