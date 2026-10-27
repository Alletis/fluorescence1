#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "OverlayAssets.h"
#include "SmoothTextButton.h"
#include "SmoothColour.h"
#include "KnobLookAndFeel.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <limits>
class OverlayTextKnob final : public juce::Component,
                              private juce::Timer
{
public:
    enum class Kind { bright, blur, resize, translate, bound };
    using BoundMode = OverlayRendering::BoundMode;
    std::function<void(double, double, bool)> onValueChanged;
    std::function<void(BoundMode)> onModeChanged;
    explicit OverlayTextKnob(Kind type = Kind::bright) : kind(type)
    {
        setWantsKeyboardFocus(true);
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        if(kind == Kind::translate)
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        else if(kind == Kind::bound)
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        input.setMultiLine(false);
        input.setJustification(juce::Justification::centred);
        input.setFont(KnobLookAndFeel::courier(11.0f));
        input.setSelectAllWhenFocused(true);
        input.onReturnKey = [this] { commitEntry(); };
        input.onEscapeKey = [this] { cancelEntry(); };
        input.onFocusLost = [this] { cancelEntry(); };
        addChildComponent(input);
        startTimerHz(60);
    }
    ~OverlayTextKnob() override { stopTimer(); }
    void setMagentaAccent(bool magenta) { isMagenta = magenta; repaint(); }
    bool isInteracting() const noexcept
    {
        return mouseHeld || dragging || input.isVisible() || popupOpen;
    }
    void setScalar(double next)
    {
        if(std::isfinite(next)) { first = next; repaint(); }
    }
    void setXY(double x, double y)
    {
        if(std::isfinite(x) && std::isfinite(y))
        {
            first = x;
            second = y;
            repaint();
        }
    }
    void setBound(BoundMode next) { mode = next; repaint(); }
    void paint(juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat().reduced(0.5f);
        const bool showValue = hovered || dragging;
        g.setColour(fillColour.get());
        g.fillRoundedRectangle(b, 3.0f);
        g.setColour(outlineColour.get());
        g.drawRoundedRectangle(b, 3.0f, 1.0f);
        if(! input.isVisible())
        {
            g.setColour(textColour.get());
            g.setFont(KnobLookAndFeel::courier(juce::jmin(12.0f, (float) getHeight() * 0.58f)));
            g.drawFittedText(showValue ? valueString() : titleString(),
                             getLocalBounds().reduced(4, 0),
                             juce::Justification::centred, 1);
        }
    }
    void resized() override { input.setBounds(getLocalBounds().reduced(2)); }
    void mouseEnter(const juce::MouseEvent&) override { hovered = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { hovered = false; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override
    {
        if(! isEnabled() || ! e.mods.isLeftButtonDown()) return;
        grabKeyboardFocus();
        mouseHeld = true;
        if(kind == Kind::bound) return;
        dragging = true;
        fineDrag = e.mods.isCtrlDown();
        dragHasChanged = false;
        dragOrigin = e.position;
        dragInitialA = first;
        dragInitialB = second;
        repaint();
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if(! dragging) return;
        const auto delta = e.position - dragOrigin;
        const double sensitivity = fineDrag ? 0.1 : 1.0;
        if(kind == Kind::translate)
            updateTranslation(delta, sensitivity, e.mods.isShiftDown());
        else if(kind == Kind::resize)
        {
            const double logScale = std::log(juce::jmax(dragInitialA,
                (double) std::numeric_limits<float>::denorm_min()))
                + ((double) delta.x - (double) delta.y) * sensitivity / 160.0;
            const double minLog = std::log((double) std::numeric_limits<float>::denorm_min());
            const double maxLog = std::log((double) std::numeric_limits<float>::max());
            first = juce::jmin((double) std::numeric_limits<float>::max(),
                std::exp(juce::jlimit(minLog, maxLog, logScale)));
        }
        else
        {
            first = juce::jlimit(0.0, 1.0,
                dragInitialA
                    + ((double) delta.x - (double) delta.y) * sensitivity / 160.0);
        }
        dragHasChanged = true;
        notify(false);
        repaint();
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        hovered = getLocalBounds().toFloat().contains(e.position);
        mouseHeld = false;
        if(dragging)
        {
            if(kind == Kind::translate && dragHasChanged)
            {
                const auto delta = e.position - dragOrigin;
                updateTranslation(delta, fineDrag ? 0.1 : 1.0, e.mods.isShiftDown());
            }
            dragging = false;
            if(dragHasChanged)
                notify(true);
        }
        else if(kind == Kind::bound && ! e.mods.isCtrlDown()
                 && ! e.mouseWasDraggedSinceMouseDown())
            showBoundMenu();
        repaint();
    }
    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        if(! e.mods.isLeftButtonDown()) return;
        dragging = false;
        switch(kind)
        {
            case Kind::bright: first = 0.6; break;
            case Kind::blur: first = 0.0; break;
            case Kind::resize: first = 1.0; break;
            case Kind::translate: first = second = 0.0; break;
            case Kind::bound:
                mode = BoundMode::clamp;
                if(onModeChanged) onModeChanged(mode);
                repaint();
                return;
        }
        notify(true);
        repaint();
    }
    bool keyPressed(const juce::KeyPress& key) override
    {
        if(key.getKeyCode() == juce::KeyPress::returnKey
            || key.getKeyCode() == juce::KeyPress::F2Key)
        {
            if(kind == Kind::bound) showBoundMenu();
            else beginEntry();
            return true;
        }
        if(kind == Kind::bound) return false;
        const auto c = key.getTextCharacter();
        if((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.'
            || (kind == Kind::translate && (c == 'x' || c == 'X' || c == 'y' || c == 'Y')))
        {
            beginEntry();
            input.setText(juce::String::charToString(c), false);
            input.moveCaretToEnd();
            return true;
        }
        return false;
    }
private:
    void updateTranslation(juce::Point<float> delta, double sensitivity, bool lockAxis)
    {
        const double normalX = saturatedFloat(
            dragInitialA + (double) delta.x * sensitivity);
        const double normalY = saturatedFloat(
            dragInitialB + (double) delta.y * sensitivity);
        if(! lockAxis)
        {
            first = normalX;
            second = normalY;
            return;
        }
        if(std::abs(normalX - dragInitialA) < std::abs(normalY - dragInitialB))
        {
            first = dragInitialA;
            second = normalY;
        }
        else
        {
            first = normalX;
            second = dragInitialB;
        }
    }
    static double saturatedFloat(double value)
    {
        const double max = (double) std::numeric_limits<float>::max();
        return juce::jlimit(-max, max, value);
    }
    juce::String titleString() const
    {
        switch(kind)
        {
            case Kind::bright: return "BRIGHT";
            case Kind::blur: return "BLUR";
            case Kind::resize: return "RESIZE";
            case Kind::translate: return "TRANSLATE";
            case Kind::bound: return "BOUND";
        }
        return {};
    }
    juce::String valueString() const
    {
        switch(kind)
        {
            case Kind::bright:
            case Kind::blur: return juce::String(first, 2);
            case Kind::resize:
                return(first >= 10000.0 || first < 0.001
                    ? juce::String::formatted("%.3g", first)
                    : juce::String(first, 3)) + "x";
            case Kind::translate:
                return "X:" + compactNumber(first) + " Y:" + compactNumber(second);
            case Kind::bound:
                switch(mode)
                {
                    case BoundMode::none: return "NONE";
                    case BoundMode::repeat: return "REPEAT";
                    case BoundMode::mirror: return "MIRROR";
                    case BoundMode::clamp: return "CLAMP";
                    default: return "CLAMP";
                }
        }
        return {};
    }
    static juce::String compactNumber(double value)
    {
        if(std::abs(value) >= 1000000.0
            || (std::abs(value) < 0.01 && value != 0.0))
            return juce::String::formatted("%.3g", value);
        if(std::abs(value - std::round(value)) < 0.01)
            return juce::String((juce::int64) std::llround(value));
        return juce::String(value, 1);
    }
    void notify(bool persist)
    {
        if(onValueChanged) onValueChanged(first, second, persist);
    }
    static bool parseNumber(juce::String text, double& value)
    {
        const auto utf8 = text.trim().toStdString();
        if(utf8.empty()) return false;
        char* end = nullptr;
        const double parsed = std::strtod(utf8.c_str(), &end);
        if(end == utf8.c_str() || *end != '\0' || ! std::isfinite(parsed)) return false;
        value = parsed;
        return true;
    }
    void beginEntry()
    {
        if(input.isVisible()) return;
        input.setText(kind == Kind::translate
            ? juce::String(first, 2) + ", " + juce::String(second, 2)
            : juce::String(first, kind == Kind::resize ? 5 : 2), false);
        input.setVisible(true);
        input.toFront(false);
        input.grabKeyboardFocus();
        input.selectAll();
        repaint();
    }
    void commitEntry()
    {
        const auto text = input.getText().trim();
        double a = first, b = second;
        bool valid = false;
        bool setFirst = false, setSecond = false;
        if(kind == Kind::translate)
        {
            if(text.startsWithIgnoreCase("x="))
            {
                valid = parseNumber(text.substring(2), a);
                setFirst = valid;
            }
            else if(text.startsWithIgnoreCase("y="))
            {
                valid = parseNumber(text.substring(2), b);
                setSecond = valid;
            }
            else
            {
                const int comma = text.indexOfChar(',');
                if(comma > 0)
                {
                valid = parseNumber(text.substring(0, comma), a)
                     && parseNumber(text.substring(comma + 1), b);
                    setFirst = setSecond = valid;
                }
            }
        }
        else valid = parseNumber(text, a);
        if(valid)
        {
            if(kind == Kind::translate)
            {
                if(setFirst) first = saturatedFloat(a);
                if(setSecond) second = saturatedFloat(b);
            }
            else if(kind == Kind::resize)
            {
                valid = a > 0.0;
                if(valid)
                    first = juce::jmax((double) std::numeric_limits<float>::denorm_min(),
                                       juce::jmin(a, (double) std::numeric_limits<float>::max()));
            }
            else first = juce::jlimit(0.0, 1.0, a);
        }
        input.setVisible(false);
        if(valid) notify(true);
        repaint();
    }
    void cancelEntry()
    {
        if(! input.isVisible()) return;
        input.setVisible(false);
        repaint();
    }
    void showBoundMenu()
    {
        if(kind != Kind::bound || popupOpen) return;
        popupOpen = true;
        const auto safe = juce::Component::SafePointer<OverlayTextKnob>(this);
        juce::MessageManager::callAsync([safe]
        {
            if(safe == nullptr) return;
            if(! safe->isShowing() || ! safe->isEnabled())
            {
                safe->popupOpen = false;
                return;
            }
            juce::PopupMenu menu;
            const std::array<BoundMode, 4> choices {
                BoundMode::none, BoundMode::repeat, BoundMode::mirror, BoundMode::clamp
            };
            const std::array<const char*, 4> names { "NONE", "REPEAT", "MIRROR", "CLAMP" };
            for(int i = 0; i < 4; ++i)
                menu.addItem(i + 1, names[(size_t) i], true,
                             safe->mode == choices[(size_t) i]);
            menu.setLookAndFeel(&safe->getLookAndFeel());
            menu.showMenuAsync(juce::PopupMenu::Options()
                                   .withTargetComponent(safe)
                                   .withMinimumWidth(safe->getWidth())
                                   .withStandardItemHeight(26),
                               [safe, choices](int selected)
            {
                if(safe == nullptr) return;
                safe->popupOpen = false;
                if(selected < 1 || selected > 4) return;
                safe->setBound(choices[(size_t) (selected - 1)]);
                if(safe->onModeChanged) safe->onModeChanged(safe->mode);
            });
        });
    }
    void timerCallback() override
    {
        const bool highlighted = hovered || dragging || input.isVisible() || popupOpen;
        const auto accent = juce::Colour(isMagenta ? 0xffd391d8 : 0xff45aeb1);
        const auto fillTarget = juce::Colour(highlighted ? 0xff23232d : 0xff17171d);
        const auto outlineTarget = highlighted ? accent.withAlpha(0.85f)
                                                : juce::Colour(0xff383842);
        const auto textTarget = highlighted ? juce::Colour(0xfff0f0f5)
                                             : juce::Colour(0xffb0b0b8);
        if(! coloursPrimed)
        {
            fillColour.set(fillTarget);
            outlineColour.set(outlineTarget);
            textColour.set(textTarget);
            coloursPrimed = true;
            repaint();
            return;
        }
        bool moving = false;
        constexpr float rate = 1.0f / 3.0f;
        moving = fillColour.approach(fillTarget, rate) || moving;
        moving = outlineColour.approach(outlineTarget, rate) || moving;
        moving = textColour.approach(textTarget, rate) || moving;
        if(moving) repaint();
    }
    Kind kind;
    bool isMagenta = false;
    bool hovered = false;
    bool mouseHeld = false;
    bool popupOpen = false;
    bool dragging = false;
    bool fineDrag = false;
    bool dragHasChanged = false;
    double first = 0.0, second = 0.0;
    double dragInitialA = 0.0, dragInitialB = 0.0;
    juce::Point<float> dragOrigin;
    BoundMode mode = BoundMode::clamp;
    juce::TextEditor input;
    SmoothColour fillColour, outlineColour, textColour;
    bool coloursPrimed = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OverlayTextKnob)
};
class OverlaySettingsPage final : public juce::Component
{
public:
    using Slot = FluorescenceOverlayAssets::Slot;
    using BoundMode = FluorescenceOverlayAssets::BoundMode;
    std::function<void(Slot)> onLoadImage;
    std::function<void(Slot)> onImageUnavailable;
    std::function<void()> onLoadSkin;
    std::function<void()> onSaveSkin;
    int offset = 26;
    OverlaySettingsPage()
    {
        loadSkin.setButtonText("LOAD SKIN");
        saveSkin.setButtonText("SAVE SKIN");
        loadSkin.onClick = [this] { if(onLoadSkin) onLoadSkin(); };
        saveSkin.onClick = [this] { if(onSaveSkin) onSaveSkin(); };
        addAndMakeVisible(loadSkin);
        addAndMakeVisible(saveSkin);
        for(int i = 0; i < 2; ++i)
        {
            auto& r = rows[(size_t) i];
            const auto slot = static_cast<Slot>(i);
            const bool magenta = (i == 1);
            r.toggle.setButtonText("OVERLAY OFF");
            r.toggle.setMagentaAccent(magenta);
            r.load.setButtonText("LOAD");
            r.load.setMagentaAccent(magenta);
            addAndMakeVisible(r.toggle);
            addAndMakeVisible(r.load);
            for(auto* knob : { &r.bright, &r.blur, &r.resize, &r.translate, &r.bound })
            {
                knob->setMagentaAccent(magenta);
                addAndMakeVisible(*knob);
            }
            r.toggle.onClick = [this, slot]
            {
                if(assets == nullptr) return;
                const bool next = ! assets->getSettings(slot).enabled;
                if(next)
                {
                    const auto snap = assets->getSnapshot(slot);
                    if(! snap.settings.imageFile.existsAsFile() || ! snap.validImage)
                    {
                        if(onImageUnavailable) onImageUnavailable(slot);
                        updateToggle(slot);
                        return;
                    }
                }
                assets->setEnabled(slot, next);
                updateToggle(slot);
            };
            r.load.onClick = [this, slot] { if(onLoadImage) onLoadImage(slot); };
            r.bright.onValueChanged = [this, slot](double v, double, bool commit)
            {
                ++editRevision;
                if(assets) assets->setBrightness(slot, (float) v, commit);
            };
            r.blur.onValueChanged = [this, slot](double v, double, bool commit)
            {
                ++editRevision;
                if(! assets) return;
                assets->setBlur(slot, (float) v);
                if(commit) assets->commitBlur(slot);
            };
            r.resize.onValueChanged = [this, slot](double v, double, bool commit)
            {
                ++editRevision;
                if(assets) assets->setResize(slot, (float) v, commit);
            };
            r.translate.onValueChanged = [this, slot](double x, double y, bool commit)
            {
                ++editRevision;
                if(assets) assets->setTranslation(slot, (float) x, (float) y, commit);
            };
            r.bound.onModeChanged = [this, slot](BoundMode mode)
            {
                ++editRevision;
                if(assets) assets->setBoundMode(slot, mode);
            };
        }
    }
    void bind(FluorescenceOverlayAssets& manager)
    {
        assets = &manager;
        refresh();
    }
    void refresh()
    {
        if(assets == nullptr) return;
        for(int i = 0; i < 2; ++i)
        {
            const auto slot = static_cast<Slot>(i);
            const auto settings = assets->getSettings(slot);
            auto& r = rows[(size_t) i];
            updateToggle(slot);
            if(! r.bright.isInteracting()) r.bright.setScalar(settings.brightness);
            if(! r.blur.isInteracting()) r.blur.setScalar(settings.blur);
            if(! r.resize.isInteracting()) r.resize.setScalar(settings.resize);
            if(! r.translate.isInteracting())
                r.translate.setXY(settings.translateX, settings.translateY);
            if(! r.bound.isInteracting()) r.bound.setBound(settings.boundMode);
        }
    }
    void setSkinLoadInProgress(bool loading)
    {
        skinLoading = loading;
        updateLoadButtonStates();
    }
    void setSkinSaveInProgress(bool saving)
    {
        skinSaving = saving;
        updateLoadButtonStates();
    }
    void setImageLoadInProgress(Slot slot, bool loading)
    {
        imageLoading[(size_t) static_cast<int>(slot)] = loading;
        updateLoadButtonStates();
    }
    bool isPreviewingOverlay() const noexcept
    {
        for(const auto& row : rows)
            for(const auto* knob : { &row.bright, &row.blur, &row.resize,
                                      &row.translate, &row.bound })
                if(knob->isInteracting())
                    return true;
        return false;
    }
    juce::uint64 getEditRevision() const noexcept { return editRevision; }
    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(0xff6ecdd0));
        g.setFont(KnobLookAndFeel::courier(KnobLookAndFeel::uiFont));
        g.drawText("IMAGE OVERLAYS", 5, 0, juce::jmax(1, getWidth() / 3), offset,
               juce::Justification::centredLeft, false);
        g.setFont(KnobLookAndFeel::courier(10.0f));
        const int rowHeight = juce::jmax(1, (getHeight() - offset) / 2);
        constexpr int labelH = 11;
        constexpr int labelGap = 1;
        constexpr int controlGap = 4;
        const int controlH = juce::jlimit(
            14, 28, (rowHeight - labelH - labelGap - controlGap) / 2);
        const int stackH = labelH + labelGap + controlH * 2 + controlGap;
        for(int i = 0; i < 2; ++i)
        {
            const int top = offset + i * rowHeight;
            const int stackY = top + juce::jmax(0, (rowHeight - stackH) / 2);
            g.setColour(juce::Colour(i == 0 ? 0xff7fc7ca : 0xffefa7f3));
            g.drawText(i == 0 ? "BACKGROUND" : "SPECTRUM",
                       5, stackY, getWidth() - 10, labelH,
                       juce::Justification::centredLeft);
            if(i == 0)
            {
                g.setColour(juce::Colour(0xff34343e));
                g.drawHorizontalLine(top + rowHeight - 1, 4.0f, (float) getWidth() - 4.0f);
            }
        }
    }
    void resized() override
    {
        constexpr int headerGap = 3;
        const int headerButtonW = juce::jmax(54, (getWidth() - 10) / 4);
        saveSkin.setBounds(getWidth() - 5 - headerButtonW, 3,
                   headerButtonW, juce::jmax(14, offset - 6));
        loadSkin.setBounds(saveSkin.getX() - headerGap - headerButtonW, 3,
                   headerButtonW, juce::jmax(14, offset - 6));
        const int rowHeight = juce::jmax(1, (getHeight() - offset) / 2);
        const int rightW = juce::jmax(54, getWidth() / 3);
        constexpr int gap = 3;
        constexpr int labelH = 11;
        constexpr int labelGap = 1;
        constexpr int rowGap = 4;
        const int controlH = juce::jlimit(
            14, 28, (rowHeight - labelH - labelGap - rowGap) / 2);
        const int stackH = labelH + labelGap + controlH * 2 + rowGap;
        constexpr std::array<float, 5> widths { 0.15f, 0.14f, 0.18f, 0.34f, 0.19f };
        const int availableW = juce::jmax(5, getWidth() - 10 - 4 * gap);
        for(int i = 0; i < 2; ++i)
        {
            auto& r = rows[(size_t) i];
            const int y = offset + i * rowHeight;
            const int stackY = y + juce::jmax(0, (rowHeight - stackH) / 2);
            const int upperControlsY = stackY + labelH + labelGap;
            const int lowerControlsY = upperControlsY + controlH + rowGap;
            r.toggle.setBounds(5, upperControlsY,
                               getWidth() - rightW - 16, controlH);
            r.load.setBounds(getWidth() - rightW - 5,
                             upperControlsY, rightW, controlH);
            const std::array<OverlayTextKnob*, 5> controls {
                &r.bright, &r.blur, &r.resize, &r.translate, &r.bound
            };
            int x = 5;
            int remaining = availableW;
            for(int k = 0; k < 5; ++k)
            {
                const int width = k == 4 ? juce::jmax(1, remaining)
                    : juce::jmax(1, juce::roundToInt((float) availableW * widths[(size_t) k]));
                controls[(size_t) k]->setBounds(x, lowerControlsY, width, controlH);
                x += width + gap;
                remaining -= width;
            }
        }
    }
private:
    struct Row
    {
        SmoothTextButton toggle, load;
        OverlayTextKnob bright { OverlayTextKnob::Kind::bright };
        OverlayTextKnob blur { OverlayTextKnob::Kind::blur };
        OverlayTextKnob resize { OverlayTextKnob::Kind::resize };
        OverlayTextKnob translate { OverlayTextKnob::Kind::translate };
        OverlayTextKnob bound { OverlayTextKnob::Kind::bound };
    };
    void updateToggle(Slot slot)
    {
        if(assets == nullptr) return;
        auto& button = rows[(size_t) static_cast<int>(slot)].toggle;
        const bool enabled = assets->getSettings(slot).enabled;
        button.setButtonText(enabled ? "OVERLAY ON" : "OVERLAY OFF");
        button.setActive(enabled);
    }
    void updateLoadButtonStates()
    {
        const bool anyImageLoading = imageLoading[0] || imageLoading[1];
        const bool anyFileOperation = skinLoading || skinSaving || anyImageLoading;
        loadSkin.setEnabled(! anyFileOperation);
        loadSkin.setButtonText(skinLoading ? "LOADING..." : "LOAD SKIN");
        saveSkin.setEnabled(! anyFileOperation);
        saveSkin.setButtonText(skinSaving ? "SAVING..." : "SAVE SKIN");
        for(size_t i = 0; i < rows.size(); ++i)
        {
            rows[i].load.setEnabled(! anyFileOperation);
            rows[i].load.setButtonText(imageLoading[i] ? "LOADING..." : "LOAD");
        }
    }
    FluorescenceOverlayAssets* assets = nullptr;
    SmoothTextButton loadSkin, saveSkin;
    std::array<Row, 2> rows;
    std::array<bool, 2> imageLoading {};
    bool skinLoading = false;
    bool skinSaving = false;
    juce::uint64 editRevision = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OverlaySettingsPage)
};