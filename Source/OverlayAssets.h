#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_cryptography/juce_cryptography.h>
#include "OverlayRendering.h"
#include "Preferences.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <functional>
#include <memory>
#include <limits>
#include <vector>
class FluorescenceOverlayAssets final : private juce::AsyncUpdater,
                                        private juce::Timer
{
private:
    class SlotWorker final : public juce::Thread
    {
    public:
        SlotWorker(FluorescenceOverlayAssets& manager, size_t slotIndex)
            : juce::Thread(slotIndex == 0 ? "Fluorescence background overlay"
                                          : "Fluorescence spectrum overlay"),
              owner(manager),
              index(slotIndex)
        {
        }
        void run() override { owner.runSlot(index, *this); }
    private:
        FluorescenceOverlayAssets& owner;
        size_t index;
    };
public:
    enum class Slot { background = 0, spectrum = 1 };
    using BoundMode = OverlayRendering::BoundMode;
    struct Settings
    {
        juce::File imageFile;
        bool enabled = false;
        float brightness = 0.6f;
        float blur = 0.0f;
        float resize = 1.0f;
        float translateX = 0.0f;
        float translateY = 0.0f;
        BoundMode boundMode = BoundMode::none;
    };
    struct Snapshot
    {
        Settings settings;
        juce::Image blurredImage;
        juce::Image previousBlurredImage;
        bool validImage = false;
        float loadMultiplier = 0.0f;
        float blurTransition = 1.0f;
    };
    std::function<void()> onChanged;
    std::function<void(Slot)> onImageNotFound;
    FluorescenceOverlayAssets()
    {
        loadPreferences();
        savePreferences();
        for(size_t i = 0; i < workers.size(); ++i)
            workers[i] = std::make_unique<SlotWorker>(*this, i);
    }
    ~FluorescenceOverlayAssets() override
    {
        assetLoadLifetime->store(false, std::memory_order_release);
        assetLoadPool.removeAllJobs(true, -1);
        stopTimer();
        for(size_t i = 0; i < workers.size(); ++i)
        {
            workers[i]->signalThreadShouldExit();
            wakes[i].signal();
        }
        for(auto& worker : workers)
            worker->stopThread(-1);
        cancelPendingUpdate();
    }
    void startLoading()
    {
        if(loadingActive.exchange(true, std::memory_order_acq_rel))
            return;
        const bool firstStart = ! workers[0]->isThreadRunning();
        if(firstStart)
        {
            const juce::ScopedLock guard(lock);
            for(auto& entry : entries)
            {
                entry.startupLoadPending = true;
                entry.startupFramesPresented = 0;
            }
        }
        for(size_t i = 0; i < workers.size(); ++i)
        {
            if(! workers[i]->isThreadRunning())
                workers[i]->startThread(juce::Thread::Priority::low);
            wakes[i].signal();
        }
    }
    void suspendLoading()
    {
        if(! loadingActive.exchange(false, std::memory_order_acq_rel)) return;
        for(auto& wake : wakes) wake.signal();
        stopTimer();
        cancelPendingUpdate();
        const juce::ScopedLock guard(lock);
        for(auto& entry : entries)
        {
            entry.fading = false;
            entry.blurFading = false;
            entry.loadMultiplier = entry.sourceImage.isValid() ? 1.0f : 0.0f;
            entry.blurTransition = 1.0f;
            for(auto& render : entry.renders) render.previousImage = {};
        }
    }
    Settings getSettings(Slot slot) const
    {
        const juce::ScopedLock guard(lock);
        return entries[index(slot)].settings;
    }
    void noteStartupFrame(Slot slot)
    {
        const juce::ScopedLock guard(lock);
        auto& entry = entries[index(slot)];
        if(entry.startupLoadPending && ! entry.sourceImage.isValid())
            entry.startupFramesPresented = juce::jmin(2, entry.startupFramesPresented + 1);
    }
    Snapshot getSnapshot(Slot slot)
    {
        const juce::ScopedLock guard(lock);
        const auto& entry = entries[index(slot)];
        return { entry.settings, entry.sourceImage, {}, entry.sourceImage.isValid(),
             entry.loadMultiplier, entry.blurTransition };
    }
    Snapshot getSnapshot(Slot slot, int width, int height)
    {
        width = juce::jmax(1, width);
        height = juce::jmax(1, height);
        juce::Image rendered;
        Settings settings;
        float multiplier = 0.0f;
        bool requestAdded = false;
        {
            const juce::ScopedLock guard(lock);
            auto& entry = entries[index(slot)];
            settings = entry.settings;
            multiplier = entry.loadMultiplier;
            for(auto& render : entry.renders)
            {
                if(render.width == width && render.height == height)
                {
                    rendered = render.image;
                    return { settings, rendered, render.previousImage,
                             rendered.isValid(), multiplier, entry.blurTransition };
                }
            }
            entry.renders.push_back({ width, height, {}, {}, true });
            requestAdded = true;
        }
        if(requestAdded)
            wakes[index(slot)].signal();
        return { settings, rendered, {}, false, multiplier, 1.0f };
    }
    bool setImageFile(Slot slot, const juce::File& file, bool notify = true)
    {
        const auto image = juce::ImageFileFormat::loadFrom(file);
        if(! image.isValid()) return false;
        const auto localFile = imageFileFor(slot);
        if(! writePng(image, localFile)) return false;
        invalidateRenderCache(slot);
        {
            const juce::ScopedLock guard(lock);
            auto& entry = entries[index(slot)];
            entry.settings.imageFile = localFile;
            entry.sourceImage = {};
            entry.renders.clear();
            entry.loadMultiplier = 0.0f;
            entry.fading = false;
            entry.startupLoadPending = false;
            entry.forceReload = true;
        }
        savePreferences();
        wakes[index(slot)].signal();
        if(notify && onChanged) onChanged();
        return true;
    }
    bool loadImageAsync(Slot slot, const juce::File& file,
                        std::function<void(juce::Result)> completion)
    {
        const auto operation = slot == Slot::background
            ? FileOperation::backgroundImage : FileOperation::spectrumImage;
        if(! beginFileOperation(operation))
            return false;
        const auto lifetime = assetLoadLifetime;
        assetLoadPool.addJob([this, lifetime, slot, file,
                              completion = std::move(completion)]() mutable
        {
            juce::Result result = setImageFile(slot, file, false)
                ? juce::Result::ok()
                : juce::Result::fail("Could not decode or install image");
            juce::MessageManager::callAsync(
                [this, lifetime, slot, result = std::move(result),
                 completion = std::move(completion)]() mutable
                {
                    if(! lifetime->load(std::memory_order_acquire))
                        return;
                    endFileOperation();
                    if(result.wasOk() && onChanged)
                        onChanged();
                    if(completion)
                        completion(std::move(result));
                });
        });
        return true;
    }
    bool isImageLoadActive(Slot slot) const noexcept
    {
        const auto operation = fileOperation.load(std::memory_order_acquire);
        return operation == (slot == Slot::background
            ? FileOperation::backgroundImage : FileOperation::spectrumImage);
    }
    bool isAnyImageLoadActive() const noexcept
    {
        return isImageLoadActive(Slot::background)
            || isImageLoadActive(Slot::spectrum);
    }
    bool isFileOperationActive() const noexcept
    {
        return fileOperation.load(std::memory_order_acquire) != FileOperation::none;
    }
    juce::Result saveSkin(const juce::File& destination) const
    {
        auto root = std::make_unique<juce::XmlElement>("FluorescenceSkin");
        root->setAttribute("version", 1);
        std::array<Settings, 2> copy;
        {
            const juce::ScopedLock guard(lock);
            for(size_t i = 0; i < copy.size(); ++i) copy[i] = entries[i].settings;
        }
        for(size_t i = 0; i < copy.size(); ++i)
        {
            auto* overlay = root->createNewChildElement("Overlay");
            overlay->setAttribute("slot", i == 0 ? "background" : "spectrum");
            overlay->setAttribute("enabled", copy[i].enabled);
            overlay->setAttribute("brightness", (double) copy[i].brightness);
            overlay->setAttribute("blur", (double) copy[i].blur);
            overlay->setAttribute("resize", (double) copy[i].resize);
            overlay->setAttribute("translateX", (double) copy[i].translateX);
            overlay->setAttribute("translateY", (double) copy[i].translateY);
            overlay->setAttribute("boundMode", (int) copy[i].boundMode);
            juce::MemoryBlock png;
            if(imageFileFor(static_cast<Slot>(i)).loadFileAsData(png))
                overlay->createNewChildElement("Image")->addTextElement(
                    juce::Base64::toBase64(png.getData(), png.getSize()));
        }
        const auto xml = root->toString();
        juce::ZipFile::Builder zip;
        zip.addEntry(std::make_unique<juce::MemoryInputStream>(
                         xml.toRawUTF8(), xml.getNumBytesAsUTF8(), true),
                     9, "skin.xml", juce::Time::getCurrentTime());
        destination.deleteFile();
        juce::FileOutputStream output(destination);
        if(! output.openedOk() || ! zip.writeToStream(output, nullptr))
            return juce::Result::fail("Could not write skin file");
        output.flush();
        return juce::Result::ok();
    }
    juce::Result loadSkin(const juce::File& source, bool notify = true)
    {
        juce::ZipFile zip(source);
        const int xmlIndex = zip.getIndexOfFileName("skin.xml");
        if(xmlIndex < 0) return juce::Result::fail("skin.xml is missing");
        std::unique_ptr<juce::InputStream> stream(zip.createStreamForEntry(xmlIndex));
        if(stream == nullptr) return juce::Result::fail("Could not read skin.xml");
        const auto xml = juce::XmlDocument::parse(stream->readEntireStreamAsString());
        if(xml == nullptr || ! xml->hasTagName("FluorescenceSkin"))
            return juce::Result::fail("Invalid skin XML");
        std::array<Settings, 2> loaded;
        std::array<juce::Image, 2> images;
        for(auto* overlay : xml->getChildIterator())
        {
            if(! overlay->hasTagName("Overlay")) continue;
            const int i = overlay->getStringAttribute("slot") == "background" ? 0
                        : overlay->getStringAttribute("slot") == "spectrum" ? 1 : -1;
            if(i < 0) continue;
            auto& settings = loaded[(size_t) i];
            settings.enabled = overlay->getBoolAttribute("enabled");
            settings.brightness = juce::jlimit(0.0f, 1.0f,
                (float) overlay->getDoubleAttribute("brightness", 0.6));
            settings.blur = juce::jlimit(0.0f, 1.0f,
                (float) overlay->getDoubleAttribute("blur"));
            const double resize = overlay->getDoubleAttribute("resize", 1.0);
            settings.resize = std::isfinite(resize) && resize > 0.0
                ? (float) juce::jmin(resize, (double) std::numeric_limits<float>::max()) : 1.0f;
            const double translateX = overlay->getDoubleAttribute("translateX");
            const double translateY = overlay->getDoubleAttribute("translateY");
            settings.translateX = std::isfinite(translateX)
                ? (float) juce::jlimit(-(double) std::numeric_limits<float>::max(),
                                      (double) std::numeric_limits<float>::max(), translateX)
                : 0.0f;
            settings.translateY = std::isfinite(translateY)
                ? (float) juce::jlimit(-(double) std::numeric_limits<float>::max(),
                                      (double) std::numeric_limits<float>::max(), translateY)
                : 0.0f;
            settings.boundMode = static_cast<BoundMode>(juce::jlimit(
                0, 3, overlay->getIntAttribute("boundMode")));
            if(auto* imageXml = overlay->getChildByName("Image"))
            {
                juce::MemoryOutputStream decoded;
                if(juce::Base64::convertFromBase64(decoded, imageXml->getAllSubText()))
                    images[(size_t) i] = juce::ImageFileFormat::loadFrom(
                        decoded.getData(), decoded.getDataSize());
            }
        }
        if(! images[0].isValid() || ! images[1].isValid())
            return juce::Result::fail("Skin must contain background and spectrum PNG images");
        for(size_t i = 0; i < images.size(); ++i)
            if(images[i].isValid()
                && ! writePng(images[i], imageFileFor(static_cast<Slot>(i))))
                return juce::Result::fail("Could not install skin image");
        for(size_t i = 0; i < images.size(); ++i)
            invalidateRenderCache(static_cast<Slot>(i));
        {
            const juce::ScopedLock guard(lock);
            for(size_t i = 0; i < entries.size(); ++i)
            {
                auto& entry = entries[i];
                loaded[i].imageFile = imageFileFor(static_cast<Slot>(i));
                entry.settings = loaded[i];
                entry.committedBlur = loaded[i].blur;
                entry.sourceImage = {};
                entry.renders.clear();
                entry.loadMultiplier = 0.0f;
                entry.fading = false;
                entry.forceReload = true;
            }
        }
        savePreferences();
        for(auto& wake : wakes) wake.signal();
        if(notify && onChanged) onChanged();
        return juce::Result::ok();
    }
    bool loadSkinAsync(const juce::File& source,
                       std::function<void(juce::Result)> completion)
    {
        if(! beginFileOperation(FileOperation::skinLoad))
            return false;
        const auto lifetime = assetLoadLifetime;
        assetLoadPool.addJob([this, lifetime, source, completion = std::move(completion)]() mutable
        {
            auto result = loadSkin(source, false);
            juce::MessageManager::callAsync(
                [this, lifetime, result = std::move(result),
                 completion = std::move(completion)]() mutable
                {
                    if(! lifetime->load(std::memory_order_acquire))
                        return;
                    endFileOperation();
                    if(result.wasOk() && onChanged)
                        onChanged();
                    if(completion)
                        completion(std::move(result));
                });
        });
        return true;
    }
    bool isSkinLoadActive() const noexcept
    {
        return fileOperation.load(std::memory_order_acquire) == FileOperation::skinLoad;
    }
    bool saveSkinAsync(const juce::File& destination,
                       std::function<void(juce::Result)> completion)
    {
        if(! beginFileOperation(FileOperation::skinSave))
            return false;
        const auto lifetime = assetLoadLifetime;
        assetLoadPool.addJob([this, lifetime, destination,
                              completion = std::move(completion)]() mutable
        {
            auto result = saveSkin(destination);
            juce::MessageManager::callAsync(
                [this, lifetime, result = std::move(result),
                 completion = std::move(completion)]() mutable
                {
                    if(! lifetime->load(std::memory_order_acquire))
                        return;
                    endFileOperation();
                    if(completion)
                        completion(std::move(result));
                });
        });
        return true;
    }
    bool isSkinSaveActive() const noexcept
    {
        return fileOperation.load(std::memory_order_acquire) == FileOperation::skinSave;
    }
    void setEnabled(Slot slot, bool enabled)
    {
        {
            const juce::ScopedLock guard(lock);
            entries[index(slot)].settings.enabled = enabled;
        }
        savePreferences();
        if(onChanged) onChanged();
    }
    void setBrightness(Slot slot, float value, bool persist = false)
    {
        {
            const juce::ScopedLock guard(lock);
            entries[index(slot)].settings.brightness = juce::jlimit(0.0f, 1.0f, value);
        }
        if(persist) savePreferences();
        if(onChanged) onChanged();
    }
    void setResize(Slot slot, float value, bool persist = false)
    {
        if(! std::isfinite(value) || value <= 0.0f) return;
        bool changed = false;
        {
            const juce::ScopedLock guard(lock);
            auto& current = entries[index(slot)].settings.resize;
            changed = current != value;
            current = value;
        }
        if(persist) savePreferences();
        if(changed && onChanged) onChanged();
    }
    void setTranslation(Slot slot, float x, float y, bool persist = false)
    {
        if(! std::isfinite(x) || ! std::isfinite(y)) return;
        bool changed = false;
        {
            const juce::ScopedLock guard(lock);
            auto& settings = entries[index(slot)].settings;
            changed = settings.translateX != x || settings.translateY != y;
            settings.translateX = x;
            settings.translateY = y;
        }
        if(persist) savePreferences();
        if(changed && onChanged) onChanged();
    }
    void setBoundMode(Slot slot, BoundMode mode)
    {
        {
            const juce::ScopedLock guard(lock);
            auto& current = entries[index(slot)].settings.boundMode;
            if(current == mode) return;
            current = mode;
        }
        savePreferences();
        if(onChanged) onChanged();
    }
    void setBlur(Slot slot, float value)
    {
        bool changed = false;
        {
            const juce::ScopedLock guard(lock);
            auto& current = entries[index(slot)].settings.blur;
            const float next = juce::jlimit(0.0f, 1.0f, value);
            changed = current != next;
            current = next;
        }
        if(changed && onChanged) onChanged();
    }
    static juce::Image renderBlurredCover(const juce::Image& source,
                                          int width, int height, float blur)
    {
        return gaussianBlur(OverlayRendering::makeCoverImage(source, width, height), blur);
    }
    void commitBlur(Slot slot)
    {
        invalidateRenderCache(slot);
        {
            const juce::ScopedLock guard(lock);
            auto& entry = entries[index(slot)];
            entry.committedBlur = entry.settings.blur;
            entry.blurEditPending = true;
            for(auto& render : entry.renders)
                render.pending = true;
        }
        savePreferences();
        wakes[index(slot)].signal();
    }
    void requestReload(Slot slot)
    {
        {
            const juce::ScopedLock guard(lock);
            entries[index(slot)].forceReload = true;
        }
        wakes[index(slot)].signal();
    }
    void savePreferences()
    {
        std::array<Settings, 2> copy;
        {
            const juce::ScopedLock guard(lock);
            for(int i = 0; i < 2; ++i)
                copy[(size_t) i] = entries[(size_t) i].settings;
        }
        if(FluorescencePreferences::settingsDirectory().createDirectory().failed())
            return;
        juce::PropertiesFile file(FluorescencePreferences::settingsFile(),
                                  FluorescencePreferences::options());
        if(! file.isValidFile())
            return;
        for(int i = 0; i < 2; ++i)
        {
            const auto stem = i == 0 ? juce::String("overlayBackground")
                                     : juce::String("overlaySpectrum");
            file.removeValue(stem + "Path");
            file.setValue(stem + "Enabled", copy[(size_t) i].enabled);
            file.setValue(stem + "Brightness", (double) copy[(size_t) i].brightness);
            file.setValue(stem + "Blur", (double) copy[(size_t) i].blur);
            file.setValue(stem + "Resize", (double) copy[(size_t) i].resize);
            file.setValue(stem + "TranslateX", (double) copy[(size_t) i].translateX);
            file.setValue(stem + "TranslateY", (double) copy[(size_t) i].translateY);
            file.setValue(stem + "BoundMode", (int) copy[(size_t) i].boundMode);
        }
        file.saveIfNeeded();
        const auto hash = contentHash(FluorescencePreferences::settingsFile());
        {
            const juce::ScopedLock guard(lock);
            settingsHash = hash;
        }
    }
private:
    enum class FileOperation
    {
        none,
        skinLoad,
        skinSave,
        backgroundImage,
        spectrumImage
    };
    bool beginFileOperation(FileOperation operation) noexcept
    {
        auto expected = FileOperation::none;
        return fileOperation.compare_exchange_strong(
            expected, operation, std::memory_order_acq_rel);
    }
    void endFileOperation() noexcept
    {
        fileOperation.store(FileOperation::none, std::memory_order_release);
    }
    static constexpr double fadeDurationMs = 200.0;
    static constexpr double startupFadeDurationMs = 120.0;
    struct Stamp
    {
        juce::String path;
        juce::int64 size = -1;
        juce::int64 modifiedMs = -1;
        bool operator==(const Stamp& rhs) const noexcept
        {
            return path == rhs.path && size == rhs.size
                   && modifiedMs == rhs.modifiedMs;
        }
    };
    struct Entry
    {
        struct Render
        {
            int width = 0;
            int height = 0;
            juce::Image image;
            juce::Image previousImage;
            bool pending = false;
        };
        Settings settings;
        juce::Image sourceImage;
        std::vector<Render> renders;
        bool forceReload = true;
        bool fadePending = false;
        bool fading = false;
        bool blurEditPending = false;
        bool blurFadePending = false;
        bool blurFading = false;
        bool imageNotFoundPending = false;
        float committedBlur = 0.0f;
        float loadMultiplier = 0.0f;
        float blurTransition = 1.0f;
        double fadeStartedAtMs = 0.0;
        double blurFadeStartedAtMs = 0.0;
        double activeFadeDurationMs = fadeDurationMs;
        bool startupLoadPending = false;
        int startupFramesPresented = 0;
    };
    static size_t index(Slot slot) noexcept { return static_cast<size_t>(slot); }
    static juce::File imageFileFor(Slot slot)
    {
        return FluorescencePreferences::settingsDirectory().getChildFile(
            slot == Slot::background ? "background.png" : "spectrum.png");
    }
    static bool writePng(const juce::Image& image, const juce::File& destination)
    {
        if(! image.isValid()
            || FluorescencePreferences::settingsDirectory().createDirectory().failed())
            return false;
        const auto temporary = destination.getSiblingFile(destination.getFileName() + ".tmp");
        temporary.deleteFile();
        {
            juce::FileOutputStream output(temporary);
            juce::PNGImageFormat png;
            if(! output.openedOk() || ! png.writeImageToStream(image, output)) return false;
            output.flush();
        }
        return temporary.moveFileTo(destination);
    }
    static juce::File renderCacheFile(Slot slot)
    {
        return FluorescencePreferences::settingsDirectory().getChildFile(
            slot == Slot::background ? "tempb.png" : "temps.png");
    }
    static juce::File renderCacheMetadataFile(Slot slot)
    {
        return FluorescencePreferences::settingsDirectory().getChildFile(
            slot == Slot::background ? "tempb.xml" : "temps.xml");
    }
    static void invalidateRenderCache(Slot slot)
    {
        renderCacheFile(slot).deleteFile();
        renderCacheMetadataFile(slot).deleteFile();
    }
    static juce::Image loadRenderCache(Slot slot, const juce::String& sourceHash,
                                       float blur, int width, int height)
    {
        juce::PropertiesFile metadata(renderCacheMetadataFile(slot),
                                      FluorescencePreferences::options());
        if(! metadata.isValidFile()
            || metadata.getValue("sourceHash") != sourceHash
            || metadata.getIntValue("width") != width
            || metadata.getIntValue("height") != height
            || std::abs((float) metadata.getDoubleValue("blur") - blur) > 0.000001f)
            return {};
        return juce::ImageFileFormat::loadFrom(renderCacheFile(slot));
    }
    static void saveRenderCache(Slot slot, const juce::Image& image,
                                const juce::String& sourceHash,
                                float blur, int width, int height)
    {
        if(! writePng(image, renderCacheFile(slot))) return;
        juce::PropertiesFile metadata(renderCacheMetadataFile(slot),
                                      FluorescencePreferences::options());
        if(! metadata.isValidFile()) return;
        metadata.setValue("sourceHash", sourceHash);
        metadata.setValue("blur", (double) blur);
        metadata.setValue("width", width);
        metadata.setValue("height", height);
        metadata.saveIfNeeded();
    }
    static juce::String contentHash(const juce::File& file)
    {
        return file.existsAsFile() ? juce::MD5(file).toHexString() : juce::String();
    }
    static bool readPreferences(std::array<Settings, 2>& result)
    {
        juce::PropertiesFile file(FluorescencePreferences::settingsFile(),
                                  FluorescencePreferences::options());
        if(! file.isValidFile()) return false;
        for(int i = 0; i < 2; ++i)
        {
            const auto stem = i == 0 ? juce::String("overlayBackground")
                                     : juce::String("overlaySpectrum");
            const auto localFile = imageFileFor(static_cast<Slot>(i));
            const auto interruptedWrite = localFile.getSiblingFile(
                localFile.getFileName() + ".tmp");
            if(! localFile.existsAsFile() && interruptedWrite.existsAsFile())
                interruptedWrite.moveFileTo(localFile);
            const auto legacyPath = file.getValue(stem + "Path");
            if(! localFile.existsAsFile() && legacyPath.isNotEmpty())
            {
                const auto legacyImage = juce::ImageFileFormat::loadFrom(juce::File(legacyPath));
                if(legacyImage.isValid()) writePng(legacyImage, localFile);
            }
            result[(size_t) i].imageFile = localFile;
            result[(size_t) i].enabled = file.getBoolValue(stem + "Enabled", false);
            result[(size_t) i].brightness = juce::jlimit(0.0f, 1.0f,
                (float) file.getDoubleValue(stem + "Brightness", 0.6));
            result[(size_t) i].blur = juce::jlimit(0.0f, 1.0f,
                (float) file.getDoubleValue(stem + "Blur", 0.0));
            const double scale = file.getDoubleValue(stem + "Resize", 1.0);
            result[(size_t) i].resize = std::isfinite(scale) && scale > 0.0
                ? (float) juce::jmin(scale, (double) std::numeric_limits<float>::max()) : 1.0f;
            if(result[(size_t) i].resize == 0.0f)
                result[(size_t) i].resize = std::numeric_limits<float>::denorm_min();
            const double tx = file.getDoubleValue(stem + "TranslateX", 0.0);
            const double ty = file.getDoubleValue(stem + "TranslateY", 0.0);
            result[(size_t) i].translateX = std::isfinite(tx)
                ? (float) juce::jlimit(-(double) std::numeric_limits<float>::max(),
                                      (double) std::numeric_limits<float>::max(), tx) : 0.0f;
            result[(size_t) i].translateY = std::isfinite(ty)
                ? (float) juce::jlimit(-(double) std::numeric_limits<float>::max(),
                                      (double) std::numeric_limits<float>::max(), ty) : 0.0f;
            result[(size_t) i].boundMode = static_cast<BoundMode>(juce::jlimit(
                0, 3, file.getIntValue(stem + "BoundMode", 0)));
        }
        return true;
    }
    void loadPreferences()
    {
        std::array<Settings, 2> loaded;
        if(! readPreferences(loaded))
            return;
        const juce::ScopedLock guard(lock);
        for(size_t i = 0; i < entries.size(); ++i)
        {
            auto& e = entries[i];
            e.settings = loaded[i];
            e.committedBlur = e.settings.blur;
            e.forceReload = true;
        }
    }
    void reloadPreferencesIfChanged()
    {
        const auto hash = contentHash(FluorescencePreferences::settingsFile());
        {
            const juce::ScopedLock guard(lock);
            if(hash == settingsHash)
                return;
        }
        std::array<Settings, 2> loaded;
        if(! readPreferences(loaded))
        {
            const juce::ScopedLock guard(lock);
            settingsHash = hash;
            return;
        }
        std::array<bool, 2> reload {};
        bool changed = false;
        {
            const juce::ScopedLock guard(lock);
            settingsHash = hash;
            for(size_t i = 0; i < entries.size(); ++i)
            {
                auto& entry = entries[i];
                const bool sourceChanged = entry.settings.imageFile != loaded[i].imageFile
                                        || entry.settings.blur != loaded[i].blur;
                const bool transformChanged = entry.settings.resize != loaded[i].resize
                                           || entry.settings.translateX != loaded[i].translateX
                                           || entry.settings.translateY != loaded[i].translateY
                                           || entry.settings.boundMode != loaded[i].boundMode;
                const bool slotChanged = sourceChanged
                                      || entry.settings.enabled != loaded[i].enabled
                                      || entry.settings.brightness != loaded[i].brightness
                                      || transformChanged;
                if(! slotChanged)
                    continue;
                entry.settings = loaded[i];
                if(sourceChanged)
                {
                    entry.committedBlur = loaded[i].blur;
                    entry.sourceImage = {};
                    entry.renders.clear();
                    entry.loadMultiplier = 0.0f;
                    entry.fading = false;
                    entry.forceReload = true;
                    reload[i] = true;
                }
                changed = true;
            }
        }
        for(size_t i = 0; i < reload.size(); ++i)
            if(reload[i])
                wakes[i].signal();
        if(changed)
            triggerAsyncUpdate();
    }
    static Stamp stampFor(const Settings& s)
    {
        Stamp result;
        result.path = s.imageFile.getFullPathName();
        if(s.imageFile.existsAsFile())
        {
            result.size = s.imageFile.getSize();
            result.modifiedMs = s.imageFile.getLastModificationTime().toMilliseconds();
        }
        return result;
    }
    static juce::Image gaussianBlur(const juce::Image& src, float normalizedRadius)
    {
        if(! src.isValid() || normalizedRadius <= 0.0001f)
            return src;
        const float sigma = normalizedRadius
                          * (float) juce::jmin(src.getWidth(), src.getHeight()) / 45.0f;
        if(sigma < 0.15f) return src;
        const int radius = juce::jlimit(1, 36, (int) std::ceil(3.0f * sigma));
        const int w = src.getWidth(), h = src.getHeight();
        const size_t n = (size_t) w * (size_t) h;
        struct Pixel { float r, g, b, a; };
        std::vector<Pixel> horizontal(n), pixels(n);
        std::vector<float> kernel((size_t) radius * 2 + 1);
        float kernelSum = 0.0f;
        for(int k = -radius; k <= radius; ++k)
        {
            const float v = std::exp(-0.5f * (float) (k * k) / (sigma * sigma));
            kernel[(size_t) (k + radius)] = v;
            kernelSum += v;
        }
        for(auto& value : kernel) value /= kernelSum;
        juce::Image::BitmapData input(src, juce::Image::BitmapData::readOnly);
        for(int y = 0; y < h; ++y)
            for(int x = 0; x < w; ++x)
            {
                const auto c = input.getPixelColour(x, y);
                const float a = c.getFloatAlpha();
                pixels[(size_t) y * (size_t) w + (size_t) x] =
                    { c.getFloatRed() * a, c.getFloatGreen() * a,
                      c.getFloatBlue() * a, a };
            }
        for(int y = 0; y < h; ++y)
            for(int x = 0; x < w; ++x)
            {
                Pixel sum {};
                for(int k = -radius; k <= radius; ++k)
                {
                    const int sample = juce::jlimit(0, w - 1, x + k);
                    const auto& p = pixels[(size_t) y * (size_t) w + (size_t) sample];
                    const float v = kernel[(size_t) (k + radius)];
                    sum.r += p.r * v; sum.g += p.g * v;
                    sum.b += p.b * v; sum.a += p.a * v;
                }
                horizontal[(size_t) y * (size_t) w + (size_t) x] = sum;
            }
        juce::Image output(juce::Image::ARGB, w, h, true);
        juce::Image::BitmapData out(output, juce::Image::BitmapData::writeOnly);
        for(int y = 0; y < h; ++y)
            for(int x = 0; x < w; ++x)
            {
                Pixel sum {};
                for(int k = -radius; k <= radius; ++k)
                {
                    const int sample = juce::jlimit(0, h - 1, y + k);
                    const auto& p = horizontal[(size_t) sample * (size_t) w + (size_t) x];
                    const float v = kernel[(size_t) (k + radius)];
                    sum.r += p.r * v; sum.g += p.g * v;
                    sum.b += p.b * v; sum.a += p.a * v;
                }
                const float invA = sum.a > 0.00001f ? 1.0f / sum.a : 0.0f;
                out.setPixelColour(x, y, juce::Colour::fromFloatRGBA(
                    juce::jlimit(0.0f, 1.0f, sum.r * invA),
                    juce::jlimit(0.0f, 1.0f, sum.g * invA),
                    juce::jlimit(0.0f, 1.0f, sum.b * invA),
                    juce::jlimit(0.0f, 1.0f, sum.a)));
            }
        return output;
    }
    static juce::Image loadSourceImage(const Settings& s)
    {
        if(! s.imageFile.existsAsFile()) return {};
        const juce::String ext = s.imageFile.getFileExtension().toLowerCase();
        if(ext != ".png" && ext != ".jpg" && ext != ".jpeg") return {};
        auto image = juce::ImageFileFormat::loadFrom(s.imageFile);
        if(! image.isValid()) return {};
        const int cap = 2048;
        const int longest = juce::jmax(image.getWidth(), image.getHeight());
        if(longest > cap)
        {
            const float factor = (float) cap / (float) longest;
            image = image.rescaled(juce::jmax(1, juce::roundToInt(image.getWidth() * factor)),
                                   juce::jmax(1, juce::roundToInt(image.getHeight() * factor)),
                                   juce::Graphics::highResamplingQuality);
        }
        return image;
    }
    void runSlot(size_t slotIndex, SlotWorker& worker)
    {
        Stamp lastStamp;
        while(! worker.threadShouldExit())
        {
            if(! loadingActive.load(std::memory_order_acquire))
            {
                wakes[slotIndex].wait(-1);
                continue;
            }
            if(slotIndex == 0)
                reloadPreferencesIfChanged();
            Settings desired;
            bool force = false;
            {
                const juce::ScopedLock guard(lock);
                auto& entry = entries[slotIndex];
                desired = entry.settings;
                desired.blur = entry.committedBlur;
                force = entry.forceReload;
                entry.forceReload = false;
            }
            const auto currentStamp = stampFor(desired);
            if(force || ! (currentStamp == lastStamp))
            {
                const bool imageMissing = ! desired.imageFile.existsAsFile();
                const auto decoded = loadSourceImage(desired);
                lastStamp = currentStamp;
                bool published = false;
                {
                    const juce::ScopedLock guard(lock);
                    auto& entry = entries[slotIndex];
                    if(entry.settings.imageFile == desired.imageFile
                        && entry.committedBlur == desired.blur)
                    {
                        entry.sourceImage = decoded;
                        entry.renders.clear();
                       #if ALLETIS_FLUORESCENCE_EXPERIMENTAL_OPENGL_UI
                        const bool fadeLateStartupImage = decoded.isValid()
                            && entry.startupLoadPending
                            && entry.startupFramesPresented >= 2;
                        entry.loadMultiplier = decoded.isValid() && ! fadeLateStartupImage
                            ? 1.0f : 0.0f;
                        entry.fadePending = fadeLateStartupImage;
                        if(! fadeLateStartupImage)
                            entry.startupLoadPending = false;
                       #else
                        entry.loadMultiplier = 0.0f;
                        entry.fadePending = false;
                       #endif
                        entry.fading = false;
                        entry.imageNotFoundPending = desired.enabled && imageMissing;
                        published = true;
                    }
                    else
                    {
                        entry.forceReload = true;
                    }
                }
                if(published)
                    triggerAsyncUpdate();
            }
                 while(! worker.threadShouldExit()
                     && loadingActive.load(std::memory_order_acquire))
            {
                juce::Image source;
                Settings renderSettings;
                int renderWidth = 0;
                int renderHeight = 0;
                {
                    const juce::ScopedLock guard(lock);
                    auto& entry = entries[slotIndex];
                    auto pending = std::find_if(
                        entry.renders.begin(), entry.renders.end(),
                        [] (const Entry::Render& render) { return render.pending; });
                    if(pending == entry.renders.end() || ! entry.sourceImage.isValid())
                        break;
                    pending->pending = false;
                    renderWidth = pending->width;
                    renderHeight = pending->height;
                    source = entry.sourceImage;
                    renderSettings = entry.settings;
                    renderSettings.blur = entry.committedBlur;
                }
                const auto slot = static_cast<Slot>(slotIndex);
                const auto sourceHash = contentHash(renderSettings.imageFile);
                auto rendered = loadRenderCache(
                    slot, sourceHash, renderSettings.blur, renderWidth, renderHeight);
                if(! rendered.isValid())
                {
                    rendered = OverlayRendering::makeCoverImage(
                        source, renderWidth, renderHeight);
                    rendered = gaussianBlur(rendered, renderSettings.blur);
                    if(rendered.isValid())
                        saveRenderCache(slot, rendered, sourceHash,
                                        renderSettings.blur, renderWidth, renderHeight);
                }
                bool renderPublished = false;
                {
                    const juce::ScopedLock guard(lock);
                    auto& entry = entries[slotIndex];
                    if(entry.sourceImage == source
                        && entry.committedBlur == renderSettings.blur)
                    {
                        auto target = std::find_if(
                            entry.renders.begin(), entry.renders.end(),
                            [renderWidth, renderHeight] (const Entry::Render& render)
                            {
                                return render.width == renderWidth
                                    && render.height == renderHeight;
                            });
                        if(target != entry.renders.end())
                        {
                            const bool replacingImage = target->image.isValid();
                            if(entry.blurEditPending && replacingImage && rendered.isValid()
                                && slot != Slot::spectrum)
                            {
                                target->previousImage = target->image;
                                entry.blurFadePending = true;
                            }
                            else if(slot == Slot::spectrum)
                            {
                                target->previousImage = {};
                                entry.blurFadePending = false;
                                entry.blurFading = false;
                                entry.blurTransition = 1.0f;
                            }
                            target->image = rendered;
                            if(! replacingImage
                                && ! entry.fadePending && ! entry.fading)
                                entry.fadePending = rendered.isValid();
                            renderPublished = true;
                        }
                    }
                }
                if(renderPublished)
                    triggerAsyncUpdate();
            }
            {
                const juce::ScopedLock guard(lock);
                entries[slotIndex].blurEditPending = false;
            }
            wakes[slotIndex].wait(2000);
        }
    }
    void handleAsyncUpdate() override
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        bool shouldAnimate = false;
        std::array<bool, 2> imageNotFound {};
        bool settingsChanged = false;
        {
            const juce::ScopedLock guard(lock);
            for(size_t i = 0; i < entries.size(); ++i)
            {
                auto& entry = entries[i];
                if(entry.imageNotFoundPending)
                {
                    entry.imageNotFoundPending = false;
                    entry.settings.enabled = false;
                    imageNotFound[i] = true;
                    settingsChanged = true;
                }
                if(! entry.fadePending)
                {
                    if(entry.blurFadePending)
                    {
                        entry.blurFadePending = false;
                        entry.blurTransition = 0.0f;
                        entry.blurFadeStartedAtMs = now;
                        entry.blurFading = true;
                        shouldAnimate = true;
                    }
                    continue;
                }
                entry.fadePending = false;
                entry.fadeStartedAtMs = now;
                entry.activeFadeDurationMs = entry.startupLoadPending
                    ? startupFadeDurationMs : fadeDurationMs;
                entry.startupLoadPending = false;
                entry.loadMultiplier = 0.0f;
                entry.fading = entry.sourceImage.isValid() || std::any_of(
                    entry.renders.begin(), entry.renders.end(),
                    [] (const Entry::Render& render) { return render.image.isValid(); });
                shouldAnimate = shouldAnimate || entry.fading;
            }
        }
        if(settingsChanged)
            savePreferences();
        if(shouldAnimate && ! isTimerRunning())
            startTimerHz(60);
        for(size_t i = 0; i < imageNotFound.size(); ++i)
            if(imageNotFound[i] && onImageNotFound)
                onImageNotFound(static_cast<Slot>(i));
        if(onChanged) onChanged();
    }
    void timerCallback() override
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        bool anyFading = false;
        {
            const juce::ScopedLock guard(lock);
            for(auto& entry : entries)
            {
                if(entry.fading)
                {
                    const float phase = juce::jlimit(
                        0.0f, 1.0f,
                        (float) ((now - entry.fadeStartedAtMs) / entry.activeFadeDurationMs));
                    entry.loadMultiplier = 0.5f - 0.5f
                        * std::cos(phase * juce::MathConstants<float>::pi);
                    entry.fading = phase < 1.0f;
                }
                if(entry.blurFading)
                {
                    const float phase = juce::jlimit(
                        0.0f, 1.0f, (float) ((now - entry.blurFadeStartedAtMs) / fadeDurationMs));
                    entry.blurTransition = 0.5f - 0.5f
                        * std::cos(phase * juce::MathConstants<float>::pi);
                    entry.blurFading = phase < 1.0f;
                    if(! entry.blurFading)
                        for(auto& render : entry.renders)
                            render.previousImage = {};
                }
                anyFading = anyFading || entry.fading || entry.blurFading;
            }
        }
        if(! anyFading)
            stopTimer();
        if(onChanged) onChanged();
    }
    mutable juce::CriticalSection lock;
    juce::ThreadPool assetLoadPool { 1, 0, juce::Thread::Priority::low };
    std::atomic<FileOperation> fileOperation { FileOperation::none };
    std::shared_ptr<std::atomic<bool>> assetLoadLifetime =
        std::make_shared<std::atomic<bool>>(true);
    std::array<Entry, 2> entries;
    juce::String settingsHash;
    std::array<juce::WaitableEvent, 2> wakes;
    std::array<std::unique_ptr<SlotWorker>, 2> workers;
    std::atomic<bool> loadingActive { false };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FluorescenceOverlayAssets)
};