#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "KnobLookAndFeel.h"
#include "ScaleLibrary.h"
#include "SmoothTextButton.h"
#include "ScaleStepperButton.h"
#include "KeyButton.h"
#include "SmoothToggle.h"
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
        addAndMakeVisible(scaleControls);
        addAndMakeVisible(midiControls);
        rootBtn.setMagentaAccent(true);
        scaleBtn.setMagentaAccent(true);
        fillClearBtn.setMagentaAccent(true);
        rootBtn.setIndex(0);
        rootBtn.onValueChange = [this] (int) { onKeyChanged(); };
        rootBtn.onMenuRequest = [this] { showKeyMenu(); };
        scaleControls.addAndMakeVisible(rootBtn);
        scaleBtn.setButtonText("Scale...");
        scaleBtn.onClick = [this] { showScaleMenu(); };
        scaleBtn.onStep = [this] (int direction) { stepScale(direction); };
        scaleControls.addAndMakeVisible(scaleBtn);
        fillClearBtn.onClick = [this] { fillOrClear(); };
        scaleControls.addAndMakeVisible(fillClearBtn);
        styleLabel(scaleControls, rootLbl, "KEY");
        styleLabel(scaleControls, scaleLbl, "SCALE");
        styleLabel(midiControls, bendLbl, "BEND RANGE");
        bendBtn.setComponentID("midiBendRange");
        bendBtn.setMagentaAccent(true);
        bendBtn.formatValue = [] (int range) { return range == 0 ? juce::String("Auto")
                                                                : juce::String(range) + " st"; };
        midiControls.addAndMakeVisible(bendBtn);
        midiControls.addAndMakeVisible(fullMidiBtn);
        midiControls.addAndMakeVisible(mpeBtn);
        bendBtn.addChildComponent(bendEditor);
        bendEditor.setJustificationType(juce::Justification::centred);
        bendEditor.setFont(KnobLookAndFeel::courier(KnobLookAndFeel::uiFont));
        bendEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff281d30));
        bendEditor.setColour(juce::TextEditor::textColourId, juce::Colour(0xffffd5ff));
        bendEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xffeb8fff));
        bendEditor.setColour(juce::TextEditor::highlightColourId, juce::Colour(0xff562b58));
        bendBtn.onMenuRequest = [this]
        {
            const int range = bendBtn.getIndex();
            bendEditor.setText(range == 0 ? juce::String("Auto") : juce::String(range),
                               juce::dontSendNotification);
            bendEditor.setVisible(true);
            bendEditor.showEditor();
        };
        bendEditor.onEditorHide = [this] { bendEditor.setVisible(false); };
        bendEditor.onTextChange = [this]
        {
            const auto text = bendEditor.getText().trim();
            if(text.equalsIgnoreCase("Auto"))
                bendBtn.setIndex(0, true);
            else if(text.isNotEmpty() && text.containsOnly("0123456789"))
                bendBtn.setIndex(text.substring(0, 3).getIntValue(), true);
        };
        bendAttachment = std::make_unique<juce::ParameterAttachment>(
            *apvts.getParameter("midiBendRange"),
            [this] (float range) { bendBtn.setIndex((int) range); });
        bendBtn.onValueChange = [this] (int range)
        {
            bendAttachment->setValueAsCompleteGesture((float) range);
        };
        bendAttachment->sendInitialUpdate();
        fullMidiAttachment = std::make_unique<ButtonAttachment>(apvts, "fullMidi", fullMidiBtn);
        mpeAttachment = std::make_unique<ButtonAttachment>(apvts, "mpe", mpeBtn);
        restoreUiState();
        targetControlsPhase = midiOn() ? 1.0f : -1.0f;
        updateTargetControls();
        startTimerHz(30);
    }
    ~ScalePanel() override { stopTimer(); }
    void syncFromApvtsState()
    {
        currentScaleId = 0;
        presetScaleSelected = false;
        scaleBtn.setButtonText("Scale...");
        scaleBtn.setActive(false);
        expectedMask = lastSeenMask = currentMask();
        writeSettleFrames = 4;
        restoreUiState();
    }
    void resized() override
    {
        scaleControls.setBounds(getLocalBounds());
        midiControls.setBounds(getLocalBounds());
        auto r = getLocalBounds();
        const int labelH = 13, rowH = 24, gap = 4;
        rootLbl.setBounds(r.removeFromTop(labelH));
        rootBtn.setBounds(r.removeFromTop(rowH));
        r.removeFromTop(gap);
        scaleLbl.setBounds(r.removeFromTop(labelH));
        scaleBtn.setBounds(r.removeFromTop(rowH));
        r.removeFromTop(gap + 2);
        fillClearBtn.setBounds(r.removeFromTop(rowH));
        bendLbl.setBounds(rootLbl.getBounds());
        bendBtn.setBounds(rootBtn.getBounds());
        bendEditor.setBounds(bendBtn.valueBounds().reduced(1));
        fullMidiBtn.setBounds(scaleBtn.getBounds());
        mpeBtn.setBounds(fillClearBtn.getBounds());
    }
private:
    struct MagentaPopupLookAndFeel : KnobLookAndFeel
    {
        MagentaPopupLookAndFeel()
        {
            setColour(juce::PopupMenu::backgroundColourId,
                      juce::Colour(0xff1a141d).withAlpha(0.99f));
            setColour(juce::PopupMenu::textColourId, juce::Colour(0xffebe4ee));
            setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff562b58));
            setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
            setColour(juce::PopupMenu::headerTextColourId, juce::Colour(0xffffbbff));
        }
    };
    void showScaleMenu()
    {
        if(midiOn()) return;
        auto menu = ScaleLibrary::buildMenu(presetScaleSelected ? currentScaleId : 0);
        menu.setLookAndFeel(&popupLnf);
        menu.showMenuAsync(juce::PopupMenu::Options()
                                .withTargetComponent(&scaleBtn)
                                .withMinimumWidth(scaleBtn.getWidth())
                                .withStandardItemHeight(26),
            [safe = juce::Component::SafePointer<ScalePanel>(this)] (int result)
            {
                if(safe != nullptr)
                    safe->selectScale(result);
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
            [safe = juce::Component::SafePointer<ScalePanel>(this)] (int result)
            {
                if(safe != nullptr && result > 0 && result <= 12)
                {
                    safe->rootBtn.setIndex(result - 1);
                    safe->onKeyChanged();
                }
            });
    }
    void styleLabel(juce::Component& parent, juce::Label& l, const juce::String& t)
    {
        l.setText(t, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centredLeft);
        l.setColour(juce::Label::textColourId, juce::Colour(0xffb0b0b8));
        l.setFont(KnobLookAndFeel::courier(KnobLookAndFeel::uiFont));
        l.setInterceptsMouseClicks(false, false);
        parent.addAndMakeVisible(l);
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
    bool isValidScaleId(int id) const
    {
        return id > 0 && id <= (int) ScaleLibrary::scales().size();
    }
    void selectScale(int id)
    {
        if(midiOn() || ! isValidScaleId(id)) return;
        currentScaleId = id;
        lastValidScaleId = id;
        presetScaleSelected = true;
        scaleBtn.setButtonText(ScaleLibrary::scales()[(size_t) (id - 1)].name);
        scaleBtn.setActive(true);
        applyScale();
        saveUiState();
    }
    void stepScale(int direction)
    {
        if(midiOn() || direction == 0) return;
        const int count = (int) ScaleLibrary::scales().size();
        if(count <= 0) return;
        const int base = presetScaleSelected && isValidScaleId(currentScaleId)
                           ? currentScaleId : lastValidScaleId;
        int next;
        if(isValidScaleId(base))
            next = 1 + (base - 1 + (direction > 0 ? 1 : -1) + count) % count;
        else
            next = direction > 0 ? 1 : count;
        selectScale(next);
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
        apvts.state.setProperty("uiLastValidScaleId", lastValidScaleId, nullptr);
    }
    void restoreUiState()
    {
        const int savedRoot = (int) apvts.state.getProperty("uiScaleRoot", 0);
        const int savedId = (int) apvts.state.getProperty("uiScaleId", 0);
        const int storedLast = (int) apvts.state.getProperty("uiLastValidScaleId", 0);
        lastValidScaleId = isValidScaleId(savedId) ? savedId
                         : isValidScaleId(storedLast) ? storedLast : 0;
        rootBtn.setIndex(savedRoot);
        if(isValidScaleId(savedId))
        {
            const auto base = ScaleLibrary::scales()[(size_t) (savedId - 1)].mask;
            const auto wanted = ScaleLibrary::rcirc(base, rootBtn.getIndex());
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
        else if(savedId != 0)
        {
            apvts.state.setProperty("uiScaleId", 0, nullptr);
        }
        if(lastValidScaleId != storedLast)
            apvts.state.setProperty("uiLastValidScaleId", lastValidScaleId, nullptr);
    }
    void timerCallback() override
    {
        const bool enabled = ! midiOn();
        updateTargetControls();
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
    void updateTargetControls()
    {
        const bool showMidi = midiOn();
        if(! showMidi && bendEditor.isBeingEdited())
            bendEditor.hideEditor(false);
        constexpr float step = 0.2f;
        if(showMidi)
            targetControlsPhase = juce::jmin(1.0f, targetControlsPhase + step);
        else
            targetControlsPhase = juce::jmax(-1.0f, targetControlsPhase - step);
        if(std::abs(targetControlsPhase) < 0.001f)
            targetControlsPhase = 0.0f;
        const auto apply = [] (juce::Component& group, float alpha, bool selected)
        {
            const bool visible = alpha > 0.0f;
            group.setAlpha(alpha);
            group.setVisible(visible);
            group.setInterceptsMouseClicks(false, visible && selected);
            group.setEnabled(visible && selected);
        };
        apply(scaleControls, juce::jmax(0.0f, -targetControlsPhase), ! showMidi);
        apply(midiControls, juce::jmax(0.0f, targetControlsPhase), showMidi);
    }
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    juce::AudioProcessorValueTreeState& apvts;
    juce::RangedAudioParameter* note[12] { };
    juce::RangedAudioParameter* midiParam = nullptr;
    juce::Component scaleControls, midiControls;
    KeyButton rootBtn;
    KeyButton bendBtn { 96, false };
    juce::Label bendLbl, bendEditor;
    SmoothToggleButton fullMidiBtn { "FULL MIDI", true };
    SmoothToggleButton mpeBtn { "MPE", true };
    std::unique_ptr<juce::ParameterAttachment> bendAttachment;
    std::unique_ptr<ButtonAttachment> fullMidiAttachment, mpeAttachment;
    float targetControlsPhase = -1.0f;
    ScaleStepperButton scaleBtn;
    SmoothTextButton fillClearBtn;
    juce::Label rootLbl, scaleLbl;
    MagentaPopupLookAndFeel popupLnf;
    int currentScaleId = 0;
    int lastValidScaleId = 0;
    bool presetScaleSelected = false;
    juce::uint16 expectedMask = 0;
    juce::uint16 lastSeenMask = 0;
    int writeSettleFrames = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScalePanel)
};