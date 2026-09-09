#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <initializer_list>
namespace ScaleLibrary
{
    struct Scale { const char* name; juce::uint16 mask; };
    inline juce::uint16 maskOf(std::initializer_list<int> semis)
    {
        juce::uint16 m = 0;
        for(int n : semis) m |= (juce::uint16) (1 << (((n % 12) + 12) % 12));
        return m;
    }
    inline const std::vector<Scale>& scales()
    {
        static const std::vector<Scale> s = []
        {
            auto m = [] (std::initializer_list<int> iv) { return maskOf(iv); };
            return std::vector<Scale>
            {
                { "Major", m({ 0,2,4,5,7,9,11 }) },
                { "Dorian", m({ 0,2,3,5,7,9,10 }) },
                { "Phrygian", m({ 0,1,3,5,7,8,10 }) },
                { "Lydian", m({ 0,2,4,6,7,9,11 }) },
                { "Mixolydian", m({ 0,2,4,5,7,9,10 }) },
                { "Minor", m({ 0,2,3,5,7,8,10 }) },
                { "Locrian", m({ 0,1,3,5,6,8,10 }) },
                { "Major Pentatonic", m({ 0,2,4,7,9 }) },
                { "Minor Pentatonic", m({ 0,3,5,7,10 }) },
                { "Minor Blues", m({ 0,3,5,6,7,10 }) },
                { "Melodic Minor", m({ 0,2,3,5,7,9,11 }) },
                { "Lydian Augmented", m({ 0,2,4,6,8,9,11 }) },
                { "Lydian Dominant", m({ 0,2,4,6,7,9,10 }) },
                { "Super Locrian", m({ 0,1,3,4,6,8,10 }) },
                { "Harmonic Minor", m({ 0,2,3,5,7,8,11 }) },
                { "Harmonic Major", m({ 0,2,4,5,7,8,11 }) },
                { "Dorian #4", m({ 0,2,3,6,7,9,10 }) },
                { "Phrygian Dominant",m({ 0,1,4,5,7,8,10 }) },
                { "Whole Tone", m({ 0,2,4,6,8,10 }) },
                { "Half-whole Dim.", m({ 0,1,3,4,6,7,9,10 }) },
                { "Whole-half Dim.", m({ 0,2,3,5,6,8,9,11 }) },
                { "Messiaen 3", m({ 0,2,3,4,6,7,8,10,11 }) },
                { "Messiaen 4", m({ 0,1,2,5,6,7,8,11 }) },
                { "Messiaen 5", m({ 0,1,5,6,7,11 }) },
                { "Messiaen 6", m({ 0,2,4,5,6,8,10,11 }) },
                { "Messiaen 7", m({ 0,1,2,3,5,6,7,8,9,11 }) },
                { "8-Tone Spanish", m({ 0,1,3,4,5,6,8,10 }) },
                { "Bhairav", m({ 0,1,4,5,7,8,11 }) },
                { "Hungarian Minor", m({ 0,2,3,6,7,8,11 }) },
                { "Hirajoshi", m({ 0,2,3,7,8 }) },
                { "In-Sen", m({ 0,1,5,7,10 }) },
                { "Iwato", m({ 0,1,5,6,10 }) },
                { "Kumoi", m({ 0,2,3,7,9 }) },
                { "Pelog Selisir", m({ 0,1,3,7,8 }) },
                { "Pelog Tembung", m({ 0,1,5,7,10 }) },
                { "Major Triad", m({ 0,4,7 }) },
                { "Minor Triad", m({ 0,3,7 }) },
                { "Diminished", m({ 0,3,6 }) },
                { "Augmented", m({ 0,4,8 }) },
                { "Sus2", m({ 0,2,7 }) },
                { "Sus4", m({ 0,5,7 }) },
                { "Major 7", m({ 0,4,7,11 }) },
                { "Dominant 7", m({ 0,4,7,10 }) },
                { "Minor 7", m({ 0,3,7,10 }) },
                { "Min7 b5", m({ 0,3,6,10 }) },
                { "Dim 7", m({ 0,3,6,9 }) },
                { "MinMaj 7", m({ 0,3,7,11 }) },
                { "Major 6", m({ 0,4,7,9 }) },
                { "Minor 6", m({ 0,3,7,9 }) },
                { "Major 11", m({ 0,2,4,5,7,11 }) },
                { "Dominant 11", m({ 0,2,4,5,7,10 }) },
                { "Minor 11", m({ 0,2,3,5,7,10 }) },
            };
        }();
        return s;
    }
    inline juce::uint16 rcirc(juce::uint16 pattern, int root)
    {
        root = ((root % 12) + 12) % 12;
        if(root == 0) return(juce::uint16) (pattern & 0x0FFF);
        return(juce::uint16) (((pattern << root) | (pattern >> (12 - root))) & 0x0FFF);
    }
    inline void addGroup(juce::PopupMenu& m, std::initializer_list<int> idxs, int currentId)
    {
        for(int i : idxs)
            m.addItem(i + 1, scales()[(size_t) i].name, true, (i + 1) == currentId);
    }
    inline juce::PopupMenu buildMenu(int currentId)
    {
        juce::PopupMenu root;
        {
            juce::PopupMenu scales;
            { juce::PopupMenu g; addGroup(g, { 0,1,2,3,4,5,6 }, currentId); scales.addSubMenu("Diatonic Modes", g); }
            { juce::PopupMenu g; addGroup(g, { 7,8,9 }, currentId); scales.addSubMenu("Pentatonic & Blues", g); }
            { juce::PopupMenu g; addGroup(g, { 10,11,12,13 }, currentId); scales.addSubMenu("Melodic Minor", g); }
            { juce::PopupMenu g; addGroup(g, { 14,15,16,17 }, currentId); scales.addSubMenu("Harmonic", g); }
            {
                juce::PopupMenu g; addGroup(g, { 18,19,20 }, currentId);
                juce::PopupMenu mess; addGroup(mess, { 21,22,23,24,25 }, currentId);
                g.addSubMenu("Messiaen", mess);
                scales.addSubMenu("Symmetric", g);
            }
            {
                juce::PopupMenu g; addGroup(g, { 26,27,28 }, currentId);
                juce::PopupMenu jp; addGroup(jp, { 29,30,31,32 }, currentId); g.addSubMenu("Japanese", jp);
                juce::PopupMenu id; addGroup(id, { 33,34 }, currentId); g.addSubMenu("Indonesian", id);
                scales.addSubMenu("World / Exotic", g);
            }
            root.addSubMenu("Scales", scales);
        }
        {
            juce::PopupMenu tri; addGroup(tri, { 35,36,37,38,39,40 }, currentId);
            juce::PopupMenu sev; addGroup(sev, { 41,42,43,44,45,46,47,48 }, currentId);
            juce::PopupMenu ele; addGroup(ele, { 49,50,51 }, currentId);
            juce::PopupMenu ch;
            ch.addSubMenu("Triads", tri);
            ch.addSubMenu("Sevenths", sev);
            ch.addSubMenu("Elevenths", ele);
            root.addSubMenu("Chords", ch);
        }
        return root;
    }
}