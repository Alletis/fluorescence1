#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
namespace ApvtsPresetIO
{
    inline constexpr const char* extension = ".apvts";
    inline const juce::Identifier idKey { "id" };
    inline const juce::Identifier valueKey { "value" };
    inline const juce::Identifier soundProperties[] =
    {
        "uiScaleRoot", "uiScaleId", "uiLastValidScaleId"
    };
    inline bool parseFiniteNumber(const juce::var& input, double& result)
    {
        if(! (input.isString() || input.isInt() || input.isInt64()
               || input.isDouble()))
            return false;
        const auto text = input.toString().trim();
        const int length = text.length();
        if(length == 0 || length > 128)
            return false;
        int i = 0;
        if(text[i] == '+' || text[i] == '-')
            ++i;
        bool anyDigits = false;
        while(i < length && text[i] >= '0' && text[i] <= '9')
        {
            anyDigits = true;
            ++i;
        }
        if(i < length && text[i] == '.')
        {
            ++i;
            while(i < length && text[i] >= '0' && text[i] <= '9')
            {
                anyDigits = true;
                ++i;
            }
        }
        if(! anyDigits)
            return false;
        if(i < length && (text[i] == 'e' || text[i] == 'E'))
        {
            ++i;
            if(i < length && (text[i] == '+' || text[i] == '-'))
                ++i;
            const int exponentStart = i;
            while(i < length && text[i] >= '0' && text[i] <= '9')
                ++i;
            if(i == exponentStart)
                return false;
        }
        if(i != length)
            return false;
        result = text.getDoubleValue();
        return std::isfinite(result);
    }
    inline juce::Result validate(const juce::ValueTree& incoming,
                                 const juce::ValueTree& current,
                                 juce::AudioProcessorValueTreeState& apvts)
    {
        if(! incoming.isValid() || ! current.isValid()
            || ! incoming.hasType(current.getType()))
            return juce::Result::fail("Not a compatible APVTS state tree.");
        if(incoming.getNumChildren() != current.getNumChildren())
            return juce::Result::fail("Parameter count does not match this plugin.");
        juce::StringArray seen;
        for(int i = 0; i < incoming.getNumChildren(); ++i)
        {
            auto node = incoming.getChild(i);
            if(! node.hasProperty(idKey) || ! node.hasProperty(valueKey))
                return juce::Result::fail("A parameter entry is incomplete.");
            const auto id = node.getProperty(idKey).toString();
            if(id.isEmpty() || seen.contains(id))
                return juce::Result::fail("Duplicate or empty parameter ID.");
            seen.add(id);
            const auto expected = current.getChildWithProperty(idKey, id);
            if(! expected.isValid() || node.getType() != expected.getType()
                || apvts.getParameter(id) == nullptr)
                return juce::Result::fail("Unknown or incompatible parameter: " + id);
            double n = 0.0;
            if(! parseFiniteNumber(node.getProperty(valueKey), n))
                return juce::Result::fail("Invalid numeric parameter value: " + id);
            const auto range = apvts.getParameterRange(id);
            if(n < (double) range.start - 0.0001 || n > (double) range.end + 0.0001)
                return juce::Result::fail("Out-of-range parameter: " + id);
        }
        for(const auto& property : soundProperties)
        {
            if(! incoming.hasProperty(property))
                continue;
            double n = 0.0;
            if(! parseFiniteNumber(incoming.getProperty(property), n))
                return juce::Result::fail("Invalid scale metadata: " + property.toString());
            if(n < 0.0 || n > (property.toString() == "uiScaleRoot" ? 11.0 : 100000.0)
                || std::floor(n) != n)
                return juce::Result::fail("Out-of-range scale metadata: " + property.toString());
        }
        return juce::Result::ok();
    }
    inline juce::Result save(juce::AudioProcessorValueTreeState& apvts,
                             const juce::File& destination)
    {
        if(destination.getFileExtension().compareIgnoreCase(extension) != 0)
            return juce::Result::fail("Preset filename must end in .apvts.");
        auto state = apvts.copyState();
        for(const auto* name : { "uiZoom", "uiSecondaryOpen",
                                  "uiSecondaryPinned", "uiHintOpen" })
            state.removeProperty(juce::Identifier(name), nullptr);
        auto xml = state.createXml();
        if(xml == nullptr)
            return juce::Result::fail("Could not serialize preset state.");
        if(! destination.replaceWithText(xml->toString()))
            return juce::Result::fail("Could not write preset file: " + destination.getFullPathName());
        return juce::Result::ok();
    }
    inline juce::Result load(juce::AudioProcessorValueTreeState& apvts,
                             const juce::File& source)
    {
        if(! source.existsAsFile())
            return juce::Result::fail("Preset file does not exist.");
        if(source.getFileExtension().compareIgnoreCase(extension) != 0)
            return juce::Result::fail("Please select a .apvts preset.");
        if(source.getSize() <= 0 || source.getSize() > 2 * 1024 * 1024)
            return juce::Result::fail("Preset is empty or unexpectedly large.");
        const auto text = source.loadFileAsString();
        if(text.containsIgnoreCase("<!DOCTYPE") || text.containsIgnoreCase("<!ENTITY"))
            return juce::Result::fail("XML DTDs and entities are not allowed in presets.");
        auto xml = juce::XmlDocument::parse(text);
        if(xml == nullptr)
            return juce::Result::fail("Preset is not valid XML.");
        auto incoming = juce::ValueTree::fromXml(*xml);
        auto current = apvts.copyState();
        const auto check = validate(incoming, current, apvts);
        if(check.failed())
            return check;
        auto merged = current.createCopy();
        for(int i = 0; i < incoming.getNumChildren(); ++i)
        {
            const auto src = incoming.getChild(i);
            auto dst = merged.getChildWithProperty(idKey, src.getProperty(idKey));
            double n = 0.0;
            if(! parseFiniteNumber(src.getProperty(valueKey), n))
                return juce::Result::fail("Unexpected invalid parameter during merge.");
            dst.setProperty(valueKey, n, nullptr);
        }
        for(const auto& property : soundProperties)
        {
            if(incoming.hasProperty(property))
            {
                double n = 0.0;
                if(! parseFiniteNumber(incoming.getProperty(property), n))
                    return juce::Result::fail("Unexpected invalid scale metadata during merge.");
                merged.setProperty(property, (int) n, nullptr);
            }
            else
                merged.removeProperty(property, nullptr);
        }
        apvts.replaceState(merged);
        return juce::Result::ok();
    }
}