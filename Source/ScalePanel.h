#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "KnobLookAndFeel.h"
#include "ScaleLibrary.h"
#include "SmoothTextButton.h"
#include "KeyButton.h"
class ScalePanel : public juce::Component,
                   private juce::Timer
{
public:
    explicit ScalePanel(juce::AudioProcessorValueTreeState& state)
        : apvts(state)
    {
        for(int i = 0; i < 12; ++i)
            note[i] = apvts.getParameter("note" + juce::String(i));
        midiParam = apvts.getParameter("targetMode");
        rootBtn.setMagentaAccent(true);
        scaleBtn.setMagentaAccent(true);
        fillClearBtn.setMagentaAccent(true);
        rootBtn.setIndex(0);
        rootBtn.onValueChange = [this] (int) { onKeyChanged(); };
        rootBtn.onMenuRequest = [this] { showKeyMenu(); };
        addAndMakeVisible(rootBtn);
        scaleBtn.setButtonText("Scale...");
        scaleBtn.onClick = [this] { showScaleMenu(); };
        addAndMakeVisible(scaleBtn);
        fillClearBtn.onClick = [this] { fillOrClear(); };
        addAndMakeVisible(fillClearBtn);
        styleLabel(rootLbl, "KEY");
        styleLabel(scaleLbl, "SCALE");
        restoreUiState();
        startTimerHz(30);
    }
    ~ScalePanel() override { stopTimer(); }
    void resized() override
    {
        auto r = getLocalBounds();
        const int labelH = 13, rowH = 24, gap = 4;
        rootLbl.setBounds(r.removeFromTop(labelH));
        rootBtn.setBounds(r.removeFromTop(rowH));
        r.removeFromTop(gap);
        scaleLbl.setBounds(r.removeFromTop(labelH));
        scaleBtn.setBounds(r.removeFromTop(rowH));
        r.removeFromTop(gap + 2);
        fillClearBtn.setBounds(r.removeFromTop(rowH));
    }
private:
    struct MagentaPopupLookAndFeel : KnobLookAndFeel
    {
        MagentaPopupLookAndFeel()
        {
            setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff1a141d));
            setColour(juce::PopupMenu::textColourId, juce::Colour(0xffebe4ee));
            setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff562b58));
            setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
            setColour(juce::PopupMenu::headerTextColourId, juce::Colour(0xffffbbff));
        }
    };
    void showScaleMenu()
    {
        if(midiOn()) return;
        auto menu = ScaleLibrary::buildMenu(currentScaleId);
        menu.setLookAndFeel(&popupLnf);
        menu.showMenuAsync(juce::PopupMenu::Options()
                                .withTargetComponent(&scaleBtn)
                                .withMinimumWidth(scaleBtn.getWidth())
                                .withStandardItemHeight(26),
            [this] (int result)
            {
                if(result > 0 && result <= (int) ScaleLibrary::scales().size())
                {
                    currentScaleId = result;
                    scaleBtn.setButtonText(ScaleLibrary::scales()[(size_t) (result - 1)].name);
                    presetScaleSelected = true;
                    scaleBtn.setActive(true);
                    applyScale();
                    saveUiState();
                }
            });
    }
    void showKeyMenu()
    {
        if(midiOn()) return;
        juce::PopupMenu menu;
        for(int i = 0; i < 12; ++i)
            menu.addItem(i + 1, KeyButton::noteName(i), true, i == rootBtn.getIndex());
        menu.setLookAndFeel(&popupLnf);
        menu.showMenuAsync(juce::PopupMenu::Options()
                                .withTargetComponent(&rootBtn)
                                .withMinimumWidth(rootBtn.getWidth())
                                .withStandardItemHeight(26),
            [this] (int result)
            {
                if(result > 0)
                {
                    rootBtn.setIndex(result - 1);
                    onKeyChanged();
                }
            });
    }
    void styleLabel(juce::Label& l, const juce::String& t)
    {
        l.setText(t, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centredLeft);
        l.setColour(juce::Label::textColourId, juce::Colour(0xffb0b0b8));
        l.setFont(KnobLookAndFeel::courier(KnobLookAndFeel::uiFont));
        l.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(l);
    }
    bool midiOn() const { return midiParam != nullptr
                              && juce::roundToInt(midiParam->getValue() * 2.0f) == 1; }
    juce::uint16 currentMask() const
    {
        juce::uint16 m = 0;
        for(int i = 0; i < 12; ++i)
            if(note[i] != nullptr && note[i]->getValue() >= 0.5f)
                m |= (juce::uint16) (1 << i);
        return m;
    }
    bool anyNoteOn() const { return currentMask() != 0; }
    void setNotes(juce::uint16 mask)
    {
        for(int i = 0; i < 12; ++i)
            if(note[i] != nullptr)
                note[i]->setValueNotifyingHost((mask & (1 << i)) ? 1.0f : 0.0f);
        expectedMask = mask;
        writeSettleFrames = 4;
    }
    void onKeyChanged()
    {
        if(presetScaleSelected) applyScale();
        saveUiState();
    }
    void applyScale()
    {
        if(midiOn() || currentScaleId <= 0) return;
        const int root = rootBtn.getIndex();
        const auto base = ScaleLibrary::scales()[(size_t) (currentScaleId - 1)].mask;
        setNotes(ScaleLibrary::rcirc(base, root));
    }
    void fillOrClear()
    {
        if(midiOn()) return;
        setNotes(anyNoteOn() ? 0x000 : 0xFFF);
        clearPreset();
    }
    void clearPreset()
    {
        if(! presetScaleSelected) return;
        presetScaleSelected = false;
        scaleBtn.setButtonText("Scale...");
        scaleBtn.setActive(false);
        saveUiState();
    }
    void saveUiState()
    {
        apvts.state.setProperty("uiScaleRoot", rootBtn.getIndex(), nullptr);
        apvts.state.setProperty("uiScaleId", presetScaleSelected ? currentScaleId : 0, nullptr);
    }
    void restoreUiState()
    {
        const int savedRoot = (int) apvts.state.getProperty("uiScaleRoot", 0);
        const int savedId = (int) apvts.state.getProperty("uiScaleId", 0);
        rootBtn.setIndex(savedRoot);
        if(savedId > 0 && savedId <= (int) ScaleLibrary::scales().size())
        {
            const auto base = ScaleLibrary::scales()[(size_t) (savedId - 1)].mask;
            const auto wanted = ScaleLibrary::rcirc(base, savedRoot);
            if(currentMask() == wanted)
            {
                currentScaleId = savedId;
                presetScaleSelected = true;
                scaleBtn.setButtonText(ScaleLibrary::scales()[(size_t) (savedId - 1)].name);
                scaleBtn.setActive(true);
                expectedMask = wanted;
                lastSeenMask = wanted;
                writeSettleFrames = 4;
            }
            else
            {
                currentScaleId = 0;
                presetScaleSelected = false;
                apvts.state.setProperty("uiScaleId", 0, nullptr);
            }
        }
    }
    void timerCallback() override
    {
        const bool enabled = ! midiOn();
        rootBtn.setEnabled(enabled);
        scaleBtn.setEnabled(enabled);
        fillClearBtn.setEnabled(enabled);
        fillClearBtn.setButtonText(anyNoteOn() ? "CLEAR" : "FULL");
        if(enabled && presetScaleSelected)
        {
            const juce::uint16 nowMask = currentMask();
            if(writeSettleFrames > 0)
                --writeSettleFrames;
            else if(nowMask != expectedMask && nowMask == lastSeenMask)
                clearPreset();
            lastSeenMask = nowMask;
        }
    }
    juce::AudioProcessorValueTreeState& apvts;
    juce::RangedAudioParameter* note[12] { };
    juce::RangedAudioParameter* midiParam = nullptr;
    KeyButton rootBtn;
    SmoothTextButton scaleBtn, fillClearBtn;
    juce::Label rootLbl, scaleLbl;
    MagentaPopupLookAndFeel popupLnf;
    int currentScaleId = 0;
    bool presetScaleSelected = false;
    juce::uint16 expectedMask = 0;
    juce::uint16 lastSeenMask = 0;
    int writeSettleFrames = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScalePanel)
};