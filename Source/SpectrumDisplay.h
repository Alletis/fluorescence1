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
#include <optional>
class SpectrumDisplay : public juce::Component,
                        private juce::Timer
{
public:
    enum class HoverZone { none, plot, disableRow };
    SpectrumDisplay(juce::AudioProcessorValueTreeState& state,
                     NewProjectAudioProcessor::SpectrumBridgeType& bridgeIn)
        : apvts(state), bridge(bridgeIn)
    {
        centerParam = apvts.getParameter("detectCenter");
        spreadParam = apvts.getParameter("detectSpread");
        disableFreqLoParam = apvts.getParameter("disableFreqLo");
        disableFreqHiParam = apvts.getParameter("disableFreqHi");
        disableActiveLoParam = apvts.getParameter("disableActiveLo");
        disableActiveHiParam = apvts.getParameter("disableActiveHi");
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
    void setDisableUiEnabled(bool enabled)
    {
        disableUiEnabled = enabled;
        repaint();
    }
    void paint(juce::Graphics& g) override
    {
        const auto plot = getPlotBounds();
        const auto handleBounds = getDisableControlBounds();
        const float W = plot.getWidth();
        const float H = plot.getHeight();
        const float plotX = plot.getX();
        const float plotY = plot.getY();
        juce::Path plotClip;
        plotClip.addRoundedRectangle(plot, spectrumCornerRadius);
        g.setColour(juce::Colour(0xff0a0a0c));
        g.fillRoundedRectangle(plot, spectrumCornerRadius);
        {
            juce::Graphics::ScopedSaveState ss(g);
            g.reduceClipRegion(plotClip);
            g.drawImage(heatImage,
                         juce::Rectangle<float> (plotX, plotY, W, H),
                         juce::RectanglePlacement::stretchToFit);
            if(disableUiEnabled)
            {
                const float xDisableLo = plotX + freqToX(dispDisableFreqLoHz(), W);
                const float xDisableHi = plotX + freqToX(dispDisableFreqHiHz(), W);
                const auto enabledBand = juce::Rectangle<float> (xDisableLo, plotY,
                                                                 juce::jmax(0.0f, xDisableHi - xDisableLo), H);
                g.setColour(juce::Colour(0xff000000).withAlpha(0.68f));
                g.fillRect(plot.withWidth(juce::jmax(0.0f, xDisableLo - plotX)));
                g.fillRect(juce::Rectangle<float> (xDisableHi, plotY,
                                                    juce::jmax(0.0f, plot.getRight() - xDisableHi), H));
                juce::ColourGradient enabledGrad(juce::Colour(0xff2b2b33).withAlpha(0.44f),
                                                  enabledBand.getCentreX(), plotY,
                                                  juce::Colour(0xff1e1e24).withAlpha(0.34f),
                                                  enabledBand.getCentreX(), plot.getBottom(), false);
                g.setGradientFill(enabledGrad);
                g.fillRect(enabledBand);
            }
        }
        const float centerHz = dispCenterHz();
        const float spread = dispSpreadVal();
        const float lowHz = centerHz / std::pow(2.0f, spread);
        const float highHz = centerHz * std::pow(2.0f, spread);
        const float xLow = plotX + freqToX(lowHz, W);
        const float xHigh = plotX + freqToX(highHz, W);
        const juce::Colour thresholdCol = thresholdLineColour();
        {
            juce::Graphics::ScopedSaveState ss(g);
            g.reduceClipRegion(plotClip);
            drawInwardGlow(g, xLow, xHigh, plotY, H, thresholdCol);
            drawInwardGlow(g, xHigh, xLow, plotY, H, thresholdCol);
            g.setColour(thresholdCol.withAlpha(thresholdLineAlpha));
            g.fillRect(xLow - 1.0f, plotY, 2.0f, H);
            g.fillRect(xHigh - 1.0f, plotY, 2.0f, H);
            if(disableUiEnabled)
            {
                drawDashedLine(g, plot, plotX + freqToX(dispDisableFreqLoHz(), W),
                                disableHandleStrokeColour(true, disableLoHighlightAmt, disableLoPressAmt).withAlpha(0.65f));
                drawDashedLine(g, plot, plotX + freqToX(dispDisableFreqHiHz(), W),
                                disableHandleStrokeColour(false, disableHiHighlightAmt, disableHiPressAmt).withAlpha(0.65f));
            }
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
                const float x = plotX + freqToX(f, W);
                for(int gx = 3; gx >= 1; --gx)
                {
                    g.setColour(baseCol.withAlpha(0.12f * (float) gx * a));
                    g.fillRect(x - (float) gx, plotY, 2.0f * (float) gx, H);
                }
                g.setColour(baseCol.withAlpha(a));
                g.fillRect(x - 1.0f, plotY, 2.0f, H);
            }
            const float rad = 9.0f * 0.67f;
            const float stroke = rad * 0.33f;
            const float haloPad = rad * 0.45f;
            const float cx = plotX + freqToX(centerHz, W);
            const float cy = plotY + spreadToY(spread, H);
            const float fillAlpha = juce::jlimit(0.0f, 1.0f, dragFillAmt);
            const float ringAlpha = 1.0f - fillAlpha;
            if(isDetectionSelection())
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
        }
        if(typing && isDetectionSelection())
        {
            g.setColour(juce::Colours::black.withAlpha(0.7f));
            g.fillRect(4.0f, 4.0f, 168.0f, 22.0f);
            g.setColour(juce::Colours::white);
            g.setFont(KnobLookAndFeel::courier(KnobLookAndFeel::uiFont));
            const juce::String label = entryLabelForSelection();
            g.drawText(label + entryBuffer + "_",
                        8, 4, 160, 22, juce::Justification::centredLeft);
        }
        if(disableUiEnabled)
        {
            drawDisableHandles(g, plot, handleBounds);
            drawDisableReadouts(g, plot);
        }
        g.setColour(juce::Colour(0xff303038));
        g.drawRoundedRectangle(plot, spectrumCornerRadius, 1.0f);
    }
    float coordOverlayGetAlpha() const noexcept { return coordOverlayAlpha; }
    juce::String coordOverlayGetText() const
    {
        return "(" + juce::String(getCenterHz(), 1) + " Hz, "
                   + juce::String(getSpread(), 2) + " spread)";
    }
    juce::Rectangle<int> coordOverlayAnchorBounds() const
    {
        return getPlotBounds().toNearestInt();
    }
    HoverZone hoverZoneAt(juce::Point<float> p) const
    {
        if(disableUiEnabled && getDisableControlBounds().contains(p))
            return HoverZone::disableRow;
        if(getPlotBounds().contains(p))
            return HoverZone::plot;
        return HoverZone::none;
    }
    void mouseEnter(const juce::MouseEvent& e) override
    {
        lastMousePos = e.position;
        updateHoverZone(hoverZoneAt(e.position));
    }
    void mouseExit(const juce::MouseEvent&) override
    {
        updateHoverZone(HoverZone::none);
    }
    void mouseMove(const juce::MouseEvent& e) override
    {
        const auto oldZone = hoverZone;
        const auto control = getDisableControlBounds();
        const auto oldClosest = (hoverZone == HoverZone::disableRow)
                                  ? closestDisableHandle(lastMousePos, getPlotBounds(), control)
                                  : DisableDragHandle::none;
        lastMousePos = e.position;
        updateHoverZone(hoverZoneAt(e.position));
        const auto newClosest = (hoverZone == HoverZone::disableRow)
                                  ? closestDisableHandle(lastMousePos, getPlotBounds(), control)
                                  : DisableDragHandle::none;
        if(oldZone == HoverZone::disableRow && newClosest != oldClosest)
            repaint();
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        currentPressDragged = false;
        pressStartPos = e.position;
        pressStartTimeMs = juce::Time::getMillisecondCounterHiRes();
        fineControlDrag = e.mods.isCtrlDown();
        if(e.mods.isPopupMenu())
        {
            const auto plot = getPlotBounds();
            if(! plot.contains(e.position))
                return;
            juce::PopupMenu menu;
            menu.addItem(1, "Type value: Centre", true, selected == Handle::center);
            menu.addItem(2, "Type value: Spread", true, selected == Handle::spread);
            menu.setLookAndFeel(&getLookAndFeel());
            menu.showMenuAsync(juce::PopupMenu::Options{}
                                    .withTargetScreenArea(localAreaToGlobal(plot.toNearestInt()))
                                    .withStandardItemHeight(26),
                                [this] (int choice)
            {
                if(choice == 1) selectEntryTarget(Handle::center);
                else if(choice == 2) selectEntryTarget(Handle::spread);
                entryBuffer.clear();
                repaint();
            });
            return;
        }
        if(disableUiEnabled)
        {
            const auto plot = getPlotBounds();
            const auto controls = getDisableControlBounds();
            if(controls.contains(e.position))
            {
                if(fineControlDrag)
                    e.source.enableUnboundedMouseMovement(true, false);
                disableDragging = closestDisableHandle(e.position, plot, controls);
                disableDragAnchorX = e.position.x;
                disableLoAnchorX = plot.getX() + freqToX(getDisableFreqLoHz(), plot.getWidth());
                disableHiAnchorX = plot.getX() + freqToX(dispTargetDisableFreqHiHz(), plot.getWidth());
                selected = (disableDragging == DisableDragHandle::low) ? Handle::disableLo : Handle::disableHi;
                typing = false;
                entryBuffer.clear();
                repaint();
                return;
            }
        }
        if(typing || selected != Handle::none)
        {
            typing = false;
            selected = Handle::none;
            entryBuffer.clear();
            repaint();
        }
        if(fineControlDrag)
            e.source.enableUnboundedMouseMovement(true, false);
        dragging = true;
        anchorCenter = getCenterHz();
        anchorSpread = getSpread();
        anchorPos = e.position;
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        lastMousePos = e.position;
        const bool draggingDisableHandle = (disableDragging != DisableDragHandle::none);
        if(! currentPressDragged)
        {
            const double heldMs = juce::Time::getMillisecondCounterHiRes() - pressStartTimeMs;
            const bool zeroDragGrace = fineControlDrag || heldMs > customDoubleClickWindowMs;
            const bool exceededDragGrace = draggingDisableHandle
                ? std::abs(e.position.x - pressStartPos.x) > (zeroDragGrace ? 0.0f : dragStartTriangleGracePx)
                : e.position.getDistanceFrom(pressStartPos) > (zeroDragGrace ? 0.0f : dragStartGracePx);
            if(! exceededDragGrace)
                return;
            currentPressDragged = true;
            pendingDoubleClick = false;
            lastClickTimeMs = 0.0;
        }
        if(draggingDisableHandle)
        {
            const auto plot = getPlotBounds();
            const float plotW = plot.getWidth();
            const float deltaScale = fineControlDrag ? fineControlScale : 1.0f;
            const float deltaX = (e.position.x - disableDragAnchorX) * deltaScale;
            float loX = disableLoAnchorX;
            float hiX = disableHiAnchorX;
            if(disableDragging == DisableDragHandle::low)
            {
                loX = juce::jlimit(plot.getX(), hiX, disableLoAnchorX + deltaX);
                setDisableFreqLoHz(xToFreq(loX - plot.getX(), plotW));
            }
            else
            {
                hiX = juce::jlimit(loX, plot.getRight(), disableHiAnchorX + deltaX);
                setDisableFreqHiHz(xToFreq(hiX - plot.getX(), plotW));
            }
            repaint();
            return;
        }
        if(! dragging) return;
        const auto plot = getPlotBounds();
        const float W = plot.getWidth();
        const float H = plot.getHeight();
        const float deltaScale = fineControlDrag ? fineControlScale : 1.0f;
        const float dx = (e.position.x - anchorPos.x) * deltaScale;
        const float dy = (e.position.y - anchorPos.y) * deltaScale;
        setCenterHz(xToFreq(freqToX(anchorCenter, W) + dx, W));
        setSpread(yToSpread(spreadToY(anchorSpread, H) + dy, H));
        revealCoordOverlay();
        repaint();
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        if(fineControlDrag)
        {
            const auto restorePos = currentFineControlScreenPosition();
            auto mouseSource = e.source;
            mouseSource.enableUnboundedMouseMovement(false);
            mouseSource.setScreenPosition(restorePos);
        }
        dragging = false;
        disableDragging = DisableDragHandle::none;
        fineControlDrag = false;
        if(e.mods.isPopupMenu())
            return;
        const double heldMs = juce::Time::getMillisecondCounterHiRes() - pressStartTimeMs;
        if(heldMs > customDoubleClickWindowMs)
        {
            currentPressDragged = false;
            pendingDoubleClick = false;
            pendingDoubleClickOnDisableHandle = false;
            lastClickTimeMs = 0.0;
            return;
        }
        if(currentPressDragged)
        {
            currentPressDragged = false;
            pendingDoubleClick = false;
            pendingDoubleClickOnDisableHandle = false;
            lastClickTimeMs = 0.0;
            return;
        }
        finishCustomDoubleClick(e);
        currentPressDragged = false;
    }
    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        juce::ignoreUnused(e);
    }
    bool finishCustomDoubleClick(const juce::MouseEvent& e)
    {
        const double nowMs = juce::Time::getMillisecondCounterHiRes();
        const bool withinTime = pendingDoubleClick
            && (nowMs - lastClickTimeMs) <= customDoubleClickWindowMs;
        const bool currentClickOnDisableHandle = disableUiEnabled && getDisableControlBounds().contains(e.position);
        const bool withinDistance = pendingDoubleClick
            && (pendingDoubleClickOnDisableHandle
                    ? std::abs(e.position.x - lastClickPos.x) <= customDoubleClickTriangleDistancePx
                    : e.position.getDistanceFrom(lastClickPos) <= customDoubleClickDistancePx);
        if(! (withinTime && withinDistance))
        {
            pendingDoubleClick = true;
            lastClickTimeMs = nowMs;
            lastClickPos = e.position;
            pendingDoubleClickOnDisableHandle = currentClickOnDisableHandle;
            return false;
        }
        pendingDoubleClick = false;
        lastClickTimeMs = 0.0;
        pendingDoubleClickOnDisableHandle = false;
        if(disableUiEnabled)
        {
            const auto plot = getPlotBounds();
            const auto controls = getDisableControlBounds();
            if(controls.contains(e.position))
            {
                if(closestDisableHandle(e.position, plot, controls) == DisableDragHandle::low) toggleDisableLoActive();
                else toggleDisableHiActive();
                selected = Handle::none;
                typing = false;
                entryBuffer.clear();
                repaint();
                return true;
            }
        }
        const bool both = (selected == Handle::none);
        if(both || selected == Handle::center) resetParam(centerParam);
        if(both || selected == Handle::spread) resetParam(spreadParam);
        revealCoordOverlay();
        repaint();
        return true;
    }
    juce::Point<float> currentFineControlScreenPosition() const
    {
        const auto localPos = currentFineControlLocalPosition();
        return localPointToGlobal(juce::Point<float> { localPos.x, localPos.y });
    }
    juce::Point<float> currentFineControlLocalPosition() const
    {
        const auto plot = getPlotBounds();
        if(disableDragging != DisableDragHandle::none)
        {
            const auto controls = getDisableControlBounds();
            const float x = plot.getX() + freqToX(disableDragging == DisableDragHandle::low
                                                       ? getDisableFreqLoHz()
                                                       : dispTargetDisableFreqHiHz(),
                                                   plot.getWidth());
            return { x, controls.getCentreY() };
        }
        return { plot.getX() + freqToX(getCenterHz(), plot.getWidth()),
                 plot.getY() + spreadToY(getSpread(), plot.getHeight()) };
    }
    bool keyPressed(const juce::KeyPress& key) override
    {
        const auto c = key.getTextCharacter();
        if(selected == Handle::none)
            return false;
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
            if(! typing)
                entryBuffer.clear();
            entryBuffer += juce::String::charToString(c);
            typing = true;
            repaint();
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
    enum class Handle { none, center, spread, disableLo, disableHi };
    enum class DisableDragHandle { none, low, high };
    using Snapshot = NewProjectAudioProcessor::SpectrumBridgeType::Snapshot;
    static constexpr float fMin = 20.0f;
    static constexpr float minSpread = 0.25f;
    static constexpr float maxSpread = 5.0f;
    static constexpr float splatHalfPx = 1.0f;
    static constexpr float splatFlatPx = 0.50f;
    static constexpr float thresholdLineAlpha = 0.5f;
    static constexpr float thresholdGlowAlpha = 0.14f;
    static constexpr float smoothRateThird = 1.0f / 3.0f;
    static constexpr float disableControlGutterHeight = 22.0f;
    static constexpr float spectrumCornerRadius = 7.0f;
    static constexpr float minDisableVisualGapPx = 2.0f;
    static constexpr float disableHandleCornerRadius = 4.0f;
    static constexpr float disableHandleHeightRatio = 0.66f;
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
        const float tgtDisableLo = getDisableFreqLoHz();
        const float tgtDisableHi = dispTargetDisableFreqHiHz();
        if(! disableDispPrimed)
        {
            dispDisableLo = tgtDisableLo;
            dispDisableHi = tgtDisableHi;
            disableDispPrimed = true;
        }
        else
        {
            dispDisableLo += (tgtDisableLo - dispDisableLo) * (1.0f / 1.4f);
            dispDisableHi += (tgtDisableHi - dispDisableHi) * (1.0f / 1.4f);
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
        const DisableDragHandle highlightHandle = currentHighlightedDisableHandle();
        disableLoHighlightAmt += (((highlightHandle == DisableDragHandle::low) ? 1.0f : 0.0f) - disableLoHighlightAmt)
                     * smoothRateThird;
        disableHiHighlightAmt += (((highlightHandle == DisableDragHandle::high) ? 1.0f : 0.0f) - disableHiHighlightAmt)
                     * smoothRateThird;
        disableLoHighlightAmt = juce::jlimit(0.0f, 1.0f, disableLoHighlightAmt);
        disableHiHighlightAmt = juce::jlimit(0.0f, 1.0f, disableHiHighlightAmt);
        disableLoPressAmt += (((disableDragging == DisableDragHandle::low) ? 1.0f : 0.0f) - disableLoPressAmt)
                * smoothRateThird;
        disableHiPressAmt += (((disableDragging == DisableDragHandle::high) ? 1.0f : 0.0f) - disableHiPressAmt)
                * smoothRateThird;
        disableLoPressAmt = juce::jlimit(0.0f, 1.0f, disableLoPressAmt);
        disableHiPressAmt = juce::jlimit(0.0f, 1.0f, disableHiPressAmt);
        coordOverlayAlpha += (coordOverlayTargetAlpha - coordOverlayAlpha) * smoothRateThird;
        coordOverlayAlpha = juce::jlimit(0.0f, 1.0f, coordOverlayAlpha);
        disableOverlayAlpha += (disableOverlayTargetAlpha - disableOverlayAlpha) * smoothRateThird;
        disableOverlayAlpha = juce::jlimit(0.0f, 1.0f, disableOverlayAlpha);
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
    void drawInwardGlow(juce::Graphics& g, float edgeX, float towardX, float plotY, float H,
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
        juce::ColourGradient grad(colour.withAlpha(intensity), x0, plotY,
                                   colour.withAlpha(0.0f), x1, plotY, false);
        g.setGradientFill(grad);
        g.fillRect(juce::Rectangle<float> (juce::Point<float> (std::min(x0, x1), plotY),
                                            juce::Point<float> (std::max(x0, x1), plotY + H)));
    }
    juce::Rectangle<float> getPlotBounds() const
    {
        auto bounds = getLocalBounds().toFloat();
        if(disableUiEnabled)
            bounds = bounds.withTrimmedBottom(disableControlGutterHeight);
        return bounds.reduced(0.5f, 0.5f);
    }
    juce::Rectangle<float> getDisableControlBounds() const
    {
        if(! disableUiEnabled)
            return {};
        const auto plot = getPlotBounds();
        return juce::Rectangle<float> (plot.getX(), plot.getBottom(), plot.getWidth(), disableControlGutterHeight);
    }
    float getDisableFreqLoHz() const
    {
        return disableFreqLoParam != nullptr
                 ? disableFreqLoParam->convertFrom0to1 (disableFreqLoParam->getValue()) : fMin;
    }
    float getDisableFreqHiRawHz() const
    {
        return disableFreqHiParam != nullptr
                 ? disableFreqHiParam->convertFrom0to1 (disableFreqHiParam->getValue()) : fMax;
    }
    float dispTargetDisableFreqHiHz() const
    {
        return juce::jmax(getDisableFreqLoHz(), getDisableFreqHiRawHz());
    }
    float dispDisableFreqLoHz() const { return disableDispPrimed ? dispDisableLo : getDisableFreqLoHz(); }
    float dispDisableFreqHiHz() const { return disableDispPrimed ? dispDisableHi : dispTargetDisableFreqHiHz(); }
    void setDisableFreqLoHz(float hz)
    {
        setParam(disableFreqLoParam, hz);
    }
    void setDisableFreqHiHz(float hz)
    {
        setParam(disableFreqHiParam, hz);
    }
    bool isDisableLoActive() const
    {
        return disableActiveLoParam == nullptr || disableActiveLoParam->getValue() >= 0.5f;
    }
    bool isDisableHiActive() const
    {
        return disableActiveHiParam == nullptr || disableActiveHiParam->getValue() >= 0.5f;
    }
    void setDisableLoActive(bool active)
    {
        setToggleParam(disableActiveLoParam, active);
    }
    void setDisableHiActive(bool active)
    {
        setToggleParam(disableActiveHiParam, active);
    }
    void toggleDisableLoActive() { setDisableLoActive(! isDisableLoActive()); }
    void toggleDisableHiActive() { setDisableHiActive(! isDisableHiActive()); }
    struct DisableHandleGeometry
    {
        juce::Path path;
        float apexX = 0.0f;
        float baseL = 0.0f;
        float baseR = 0.0f;
        float yTop = 0.0f;
        float yBottom = 0.0f;
    };
    struct DisableBaseLayout
    {
        float lowL = 0.0f;
        float lowR = 0.0f;
        float hiL = 0.0f;
        float hiR = 0.0f;
    };
    static void shiftIntervalInside(float& left, float& right, float minX, float maxX) noexcept
    {
        const float width = right - left;
        if(width >= maxX - minX)
        {
            left = minX;
            right = maxX;
            return;
        }
        if(left < minX)
        {
            const float delta = minX - left;
            left += delta;
            right += delta;
        }
        if(right > maxX)
        {
            const float delta = right - maxX;
            left -= delta;
            right -= delta;
        }
    }
    DisableBaseLayout computeDisableBaseLayout(juce::Rectangle<float> plot,
                                                juce::Rectangle<float> control) const
    {
        DisableBaseLayout layout;
        const float loX = plot.getX() + freqToX(dispDisableFreqLoHz(), plot.getWidth());
        const float hiX = plot.getX() + freqToX(dispDisableFreqHiHz(), plot.getWidth());
        const float yTop = control.getY() + 1.0f;
        const float yBottom = control.getBottom() - 1.0f;
        const float height = juce::jmax(1.0f, yBottom - yTop);
        const float baseWidth = height * disableHandleHeightRatio;
        const float halfWidth = 0.5f * baseWidth;
        const float minGap = 1.0f;
        layout.lowL = loX - halfWidth;
        layout.lowR = loX + halfWidth;
        layout.hiL = hiX - halfWidth;
        layout.hiR = hiX + halfWidth;
        shiftIntervalInside(layout.lowL, layout.lowR, plot.getX(), plot.getRight());
        shiftIntervalInside(layout.hiL, layout.hiR, plot.getX(), plot.getRight());
        const float overlap = (layout.lowR + minGap) - layout.hiL;
        if(overlap > 0.0f)
        {
            const float lowRoom = layout.lowL - plot.getX();
            const float hiRoom = plot.getRight() - layout.hiR;
            const float moveLow = juce::jmin(lowRoom, overlap * 0.5f);
            const float moveHi = juce::jmin(hiRoom, overlap - moveLow);
            layout.lowL -= moveLow;
            layout.lowR -= moveLow;
            layout.hiL += moveHi;
            layout.hiR += moveHi;
            const float remaining = (layout.lowR + minGap) - layout.hiL;
            if(remaining > 0.0f)
            {
                const float shrinkLow = juce::jmin(remaining * 0.5f, juce::jmax(0.0f, layout.lowR - layout.lowL - minGap));
                const float shrinkHi = juce::jmin(remaining - shrinkLow, juce::jmax(0.0f, layout.hiR - layout.hiL - minGap));
                layout.lowR -= shrinkLow;
                layout.hiL += shrinkHi;
            }
        }
        layout.lowR = juce::jmax(layout.lowL, layout.lowR);
        layout.hiR = juce::jmax(layout.hiL, layout.hiR);
        return layout;
    }
    DisableHandleGeometry makeDisableHandleGeometry(bool lowHandle, juce::Rectangle<float> plot,
                                                     juce::Rectangle<float> control) const
    {
        DisableHandleGeometry geo;
        if(! disableUiEnabled || control.isEmpty())
            return geo;
        const float loX = plot.getX() + freqToX(dispDisableFreqLoHz(), plot.getWidth());
        const float hiX = plot.getX() + freqToX(dispDisableFreqHiHz(), plot.getWidth());
        const auto layout = computeDisableBaseLayout(plot, control);
        const float yTop = control.getY() + 1.0f;
        const float yBottom = control.getBottom() - 1.0f;
        const float apexX = lowHandle ? loX : hiX;
        const float baseL = lowHandle ? layout.lowL : layout.hiL;
        const float baseR = lowHandle ? layout.lowR : layout.hiR;
        geo.apexX = apexX;
        geo.baseL = baseL;
        geo.baseR = baseR;
        geo.yTop = yTop;
        geo.yBottom = yBottom;
        juce::Path triangle;
        triangle.startNewSubPath(apexX, yTop);
        triangle.lineTo(baseL, yBottom);
        triangle.lineTo(baseR, yBottom);
        triangle.closeSubPath();
        geo.path = triangle.createPathWithRoundedCorners(disableHandleCornerRadius);
        return geo;
    }
    static float pointToSegmentDistanceSq(juce::Point<float> p, juce::Point<float> a,
                                           juce::Point<float> b) noexcept
    {
        const juce::Point<float> ab = b - a;
        const float lenSq = ab.x * ab.x + ab.y * ab.y;
        if(lenSq <= 1.0e-6f)
            return p.getDistanceSquaredFrom(a);
        const juce::Point<float> ap = p - a;
        const float t = juce::jlimit(0.0f, 1.0f, (ap.x * ab.x + ap.y * ab.y) / lenSq);
        const juce::Point<float> closest = a + ab * t;
        return p.getDistanceSquaredFrom(closest);
    }
    static float distanceSqToHandle(juce::Point<float> p, const DisableHandleGeometry& geo) noexcept
    {
        if(geo.path.contains(p))
            return 0.0f;
        const juce::Point<float> apex(geo.apexX, geo.yTop);
        const juce::Point<float> left(geo.baseL, geo.yBottom);
        const juce::Point<float> right(geo.baseR, geo.yBottom);
        return juce::jmin(pointToSegmentDistanceSq(p, apex, left),
                           pointToSegmentDistanceSq(p, apex, right),
                           pointToSegmentDistanceSq(p, left, right));
    }
    DisableDragHandle closestDisableHandle(juce::Point<float> p, juce::Rectangle<float> plot,
                                            juce::Rectangle<float> control) const
    {
        const auto lo = makeDisableHandleGeometry(true, plot, control);
        const auto hi = makeDisableHandleGeometry(false, plot, control);
        return distanceSqToHandle(p, lo) <= distanceSqToHandle(p, hi) ? DisableDragHandle::low
                                                                        : DisableDragHandle::high;
    }
    void drawDashedLine(juce::Graphics& g, juce::Rectangle<float> plot, float x, juce::Colour c) const
    {
        juce::Path p;
        p.startNewSubPath(x, plot.getY());
        p.lineTo(x, plot.getBottom());
        juce::Path dashed;
        const float dashes[] = { 4.0f, 4.0f };
        juce::PathStrokeType(1.0f).createDashedStroke(dashed, p, dashes, 2);
        g.setColour(c);
        g.fillPath(dashed);
    }
    void drawDisableHandles(juce::Graphics& g, juce::Rectangle<float> plot,
                             juce::Rectangle<float> control) const
    {
        if(! disableUiEnabled || control.isEmpty())
            return;
        for(const bool lowHandle : { true, false })
        {
            const auto geo = makeDisableHandleGeometry(lowHandle, plot, control);
            const float highlight = lowHandle ? disableLoHighlightAmt : disableHiHighlightAmt;
            const float pressed = lowHandle ? disableLoPressAmt : disableHiPressAmt;
            g.setColour(disableHandleFillColour(lowHandle, highlight, pressed).withAlpha(0.92f));
            g.fillPath(geo.path);
            g.setColour(disableHandleStrokeColour(lowHandle, highlight, pressed).withAlpha(0.95f));
            g.strokePath(geo.path, juce::PathStrokeType(1.0f));
        }
    }
    juce::Colour disableHandleFillColour(bool lowHandle, float highlight, float pressedAmt) const
    {
        const bool active = lowHandle ? isDisableLoActive() : isDisableHiActive();
        const auto enabledIdle = juce::Colour(0xff2d6f72);
        const auto enabledHot = juce::Colour(0xff6ecdd0);
        const auto idle = active ? enabledIdle : greyscaleColour(enabledIdle);
        const auto hot = active ? enabledHot : greyscaleColour(enabledHot);
        auto colour = idle.interpolatedWith(hot, juce::jlimit(0.0f, 1.0f, highlight));
        return colour.interpolatedWith(colour.darker(0.35f), juce::jlimit(0.0f, 1.0f, pressedAmt));
    }
    juce::Colour disableHandleStrokeColour(bool lowHandle, float highlight, float pressedAmt) const
    {
        const bool active = lowHandle ? isDisableLoActive() : isDisableHiActive();
        const auto enabledIdle = juce::Colour(0xff45aeb1);
        const auto enabledHot = juce::Colour(0xff9fe3e5);
        const auto idle = active ? enabledIdle : greyscaleColour(enabledIdle);
        const auto hot = active ? enabledHot : greyscaleColour(enabledHot);
        auto colour = idle.interpolatedWith(hot, juce::jlimit(0.0f, 1.0f, highlight));
        return colour.interpolatedWith(colour.darker(0.25f), juce::jlimit(0.0f, 1.0f, pressedAmt));
    }
    static juce::Colour greyscaleColour(juce::Colour c)
    {
        return juce::Colour::greyLevel(c.getPerceivedBrightness()).withAlpha(c.getFloatAlpha());
    }
    DisableDragHandle currentHighlightedDisableHandle() const
    {
        if(! disableUiEnabled)
            return DisableDragHandle::none;
        if(disableDragging != DisableDragHandle::none)
            return disableDragging;
        if(hoverZone != HoverZone::disableRow)
            return DisableDragHandle::none;
        const auto plot = getPlotBounds();
        const auto control = getDisableControlBounds();
        if(control.isEmpty())
            return DisableDragHandle::none;
        return closestDisableHandle(lastMousePos, plot, control);
    }
    void drawDisableReadouts(juce::Graphics& g, juce::Rectangle<float> plot) const
    {
        const float hoverAlpha = disableOverlayAlpha;
        const float forcedAlpha = (disableDragging != DisableDragHandle::none || (typing && isDisableSelection())) ? 1.0f : 0.0f;
        const float a = juce::jmax(hoverAlpha, forcedAlpha);
        if(a <= 0.001f)
            return;
        const auto chipCol = juce::Colour(0xff111116).withAlpha(0.78f);
        const auto borderCol = juce::Colour(0xff4e4e58).withAlpha(0.82f);
        const auto textCol = juce::Colour(0xffd8d8e0);
        const int chipW = 104;
        const int chipH = 18;
        const int pad = 6;
        const int y = juce::roundToInt(plot.getBottom()) - chipH - pad;
        auto drawChip = [&] (juce::Rectangle<int> r, const juce::String& text, juce::Justification just)
        {
            g.setColour(chipCol.withAlpha(chipCol.getFloatAlpha() * a));
            g.fillRoundedRectangle(r.toFloat(), 4.0f);
            g.setColour(borderCol.withAlpha(borderCol.getFloatAlpha() * a));
            g.drawRoundedRectangle(r.toFloat(), 4.0f, 1.0f);
            g.setColour(textCol.withAlpha(a));
            g.setFont(KnobLookAndFeel::courier(11.0f));
            g.drawText(text, r.reduced(6, 1), just, false);
        };
        const juce::String loText = typing && selected == Handle::disableLo
                          ? entryBuffer + "_"
                          : juce::String(getDisableFreqLoHz(), 1) + " Hz";
        const juce::String hiText = typing && selected == Handle::disableHi
                          ? entryBuffer + "_"
                          : juce::String(dispTargetDisableFreqHiHz(), 1) + " Hz";
        drawChip({ juce::roundToInt(plot.getX()) + pad, y, chipW, chipH },
              loText,
                  juce::Justification::centredLeft);
        drawChip({ juce::roundToInt(plot.getRight()) - chipW - pad, y, chipW, chipH },
              hiText,
                  juce::Justification::centredRight);
    }
    juce::String entryLabelForSelection() const
    {
        switch(selected)
        {
            case Handle::center: return "centre ";
            case Handle::spread: return "spread ";
            case Handle::disableLo: return "disable lo ";
            case Handle::disableHi: return "disable hi ";
            case Handle::none: break;
        }
        return {};
    }
    bool isDetectionSelection() const noexcept
    {
        return selected == Handle::center || selected == Handle::spread;
    }
    bool isDisableSelection() const noexcept
    {
        return selected == Handle::disableLo || selected == Handle::disableHi;
    }
    void updateHoverZone(HoverZone newZone)
    {
        if(hoverZone == newZone)
            return;
        hoverZone = newZone;
        ++coordOverlayToken;
        ++disableOverlayToken;
        mouseOverPlot = (newZone == HoverZone::plot);
        mouseOverDisableRow = (newZone == HoverZone::disableRow);
        coordOverlayTargetAlpha = mouseOverPlot ? coordOverlayTargetAlpha : 0.0f;
        disableOverlayTargetAlpha = mouseOverDisableRow ? disableOverlayTargetAlpha : 0.0f;
        if(mouseOverPlot)
            scheduleCoordOverlayShow();
        else
            coordOverlayTargetAlpha = 0.0f;
        if(mouseOverDisableRow)
            scheduleDisableOverlayShow();
        else
            disableOverlayTargetAlpha = 0.0f;
        repaint();
    }
    void selectEntryTarget(Handle handle)
    {
        selected = handle;
        typing = false;
        entryBuffer.clear();
        repaint();
    }
    juce::RangedAudioParameter* paramForSelection() const
    {
        switch(selected)
        {
            case Handle::center: return centerParam;
            case Handle::spread: return spreadParam;
            case Handle::disableLo: return disableFreqLoParam;
            case Handle::disableHi: return disableFreqHiParam;
            case Handle::none: break;
        }
        return nullptr;
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
    static void setToggleParam(juce::RangedAudioParameter* p, bool enabled)
    {
        if(p == nullptr) return;
        p->setValueNotifyingHost(enabled ? 1.0f : 0.0f);
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
            setParam(paramForSelection(), value);
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
            if(! safe->mouseOverPlot)
                return;
            safe->coordOverlayTargetAlpha = 1.0f;
            safe->repaint();
        });
    }
    void scheduleDisableOverlayShow()
    {
        const int token = ++disableOverlayToken;
        juce::Timer::callAfterDelay(250, [safe = juce::Component::SafePointer<SpectrumDisplay> (this), token]
        {
            if(safe == nullptr)
                return;
            if(token != safe->disableOverlayToken)
                return;
            if(! safe->mouseOverDisableRow)
                return;
            safe->disableOverlayTargetAlpha = 1.0f;
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
    juce::RangedAudioParameter* disableFreqLoParam = nullptr;
    juce::RangedAudioParameter* disableFreqHiParam = nullptr;
    juce::RangedAudioParameter* disableActiveLoParam = nullptr;
    juce::RangedAudioParameter* disableActiveHiParam = nullptr;
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
    DisableDragHandle disableDragging = DisableDragHandle::none;
    juce::Point<float> anchorPos;
    float anchorCenter = 640.0f;
    float anchorSpread = 2.5f;
    float disableDragAnchorX = 0.0f;
    float disableLoAnchorX = 0.0f;
    float disableHiAnchorX = 0.0f;
    float dispCenter = 640.0f;
    float dispSpread = 2.5f;
    bool dispPrimed = false;
    float dispDisableLo = fMin;
    float dispDisableHi = 20000.0f;
    bool disableDispPrimed = false;
    float dragFillAmt = 0.0f;
    float thresholdBlendAmt = 0.0f;
    bool thresholdBlendPrimed = false;
    float bypassVisualAmt = 0.0f;
    bool bypassVisualPrimed = false;
    bool mouseOverPlot = false;
    bool mouseOverDisableRow = false;
    HoverZone hoverZone = HoverZone::none;
    juce::Point<float> lastMousePos;
    float coordOverlayAlpha = 0.0f;
    float coordOverlayTargetAlpha = 0.0f;
    int coordOverlayToken = 0;
    float disableOverlayAlpha = 0.0f;
    float disableOverlayTargetAlpha = 0.0f;
    int disableOverlayToken = 0;
    float disableLoHighlightAmt = 0.0f;
    float disableHiHighlightAmt = 0.0f;
    float disableLoPressAmt = 0.0f;
    float disableHiPressAmt = 0.0f;
    bool disableUiEnabled = false;
    bool currentPressDragged = false;
    juce::Point<float> pressStartPos;
    double pressStartTimeMs = 0.0;
    bool fineControlDrag = false;
    bool pendingDoubleClick = false;
    bool pendingDoubleClickOnDisableHandle = false;
    double lastClickTimeMs = 0.0;
    juce::Point<float> lastClickPos;
    static constexpr double customDoubleClickWindowMs = 320.0;
    static constexpr float fineControlScale = 0.1f;
    static constexpr float customDoubleClickDistancePx = 16.0f;
    static constexpr float customDoubleClickTriangleDistancePx = 18.0f;
    static constexpr float dragStartGracePx = 2.0f;
    static constexpr float dragStartTriangleGracePx = 2.0f;
    Handle selected = Handle::none;
    bool typing = false;
    juce::String entryBuffer;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumDisplay)
};