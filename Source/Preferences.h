#pragma once
#include <juce_data_structures/juce_data_structures.h>
#include <cmath>
namespace FluorescencePreferences
{
    inline juce::File settingsDirectory()
    {
       #if JUCE_WINDOWS
        const auto base = juce::File::getSpecialLocation(juce::File::windowsLocalAppData);
       #elif JUCE_MAC
        const auto base = juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                              .getChildFile("Library/Application Support");
       #else
        const auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
       #endif
        return base.getChildFile("Alletis").getChildFile("Fluorescence");
    }
    inline juce::File settingsFile()
    {
        return settingsDirectory().getChildFile("settings.xml");
    }
    inline juce::PropertiesFile::Options options()
    {
        juce::PropertiesFile::Options result;
        result.applicationName = "Fluorescence";
        result.filenameSuffix = ".xml";
        result.storageFormat = juce::PropertiesFile::storeAsXML;
        result.millisecondsBeforeSaving = -1;
        return result;
    }
    inline float loadUiZoom(float fallback = 1.0f)
    {
        juce::PropertiesFile settings(settingsFile(), options());
        if(! settings.isValidFile())
            return fallback;
        const auto saved = settings.getValue("uiZoom");
        if(saved.isEmpty()) return fallback;
        const double value = saved.getDoubleValue();
        if(! std::isfinite(value) || value <= 0.0)
            return fallback;
        const float next = (float) value;
        return std::isfinite(next) ? next : fallback;
    }
    inline bool saveUiZoom(float zoom)
    {
        if(! std::isfinite(zoom) || zoom <= 0.0f)
            return false;
        if(settingsDirectory().createDirectory().failed())
            return false;
        juce::PropertiesFile settings(settingsFile(), options());
        if(! settings.isValidFile())
            return false;
        settings.setValue("uiZoom", (double) zoom);
        return settings.saveIfNeeded();
    }
    inline juce::File lastPresetDirectory()
    {
        juce::PropertiesFile settings(settingsFile(), options());
        if(settings.isValidFile())
        {
            const auto saved = settings.getValue("lastPresetDirectory");
            if(saved.isNotEmpty())
            {
                const juce::File folder(saved);
                if(folder.isDirectory())
                    return folder;
            }
        }
        return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    }
    inline bool rememberPresetDirectory(const juce::File& folder)
    {
        if(! folder.isDirectory())
            return false;
        if(settingsDirectory().createDirectory().failed())
            return false;
        juce::PropertiesFile settings(settingsFile(), options());
        if(! settings.isValidFile())
            return false;
        settings.setValue("lastPresetDirectory", folder.getFullPathName());
        return settings.saveIfNeeded();
    }
    inline juce::File lastDirectory(const juce::String& key,
                                    const juce::File& fallback)
    {
        juce::PropertiesFile settings(settingsFile(), options());
        if(settings.isValidFile())
        {
            const juce::File saved(settings.getValue(key));
            if(saved.isDirectory()) return saved;
        }
        return fallback;
    }
    inline bool rememberDirectory(const juce::String& key, const juce::File& folder)
    {
        if(! folder.isDirectory()
            || settingsDirectory().createDirectory().failed())
            return false;
        juce::PropertiesFile settings(settingsFile(), options());
        if(! settings.isValidFile()) return false;
        settings.setValue(key, folder.getFullPathName());
        return settings.saveIfNeeded();
    }
    inline juce::File lastOverlayImageDirectory()
    {
        return lastDirectory("lastOverlayImageDirectory",
            juce::File::getSpecialLocation(juce::File::userPicturesDirectory));
    }
    inline bool rememberOverlayImageDirectory(const juce::File& folder)
    {
        return rememberDirectory("lastOverlayImageDirectory", folder);
    }
    inline juce::File lastSkinDirectory()
    {
        return lastDirectory("lastSkinDirectory",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory));
    }
    inline bool rememberSkinDirectory(const juce::File& folder)
    {
        return rememberDirectory("lastSkinDirectory", folder);
    }
}