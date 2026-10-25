#pragma once
#include <juce_data_structures/juce_data_structures.h>
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
}