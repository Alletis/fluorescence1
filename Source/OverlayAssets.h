#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_cryptography/juce_cryptography.h>
#include "OverlayRendering.h"
#include "Preferences.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <memory>
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
    struct Settings
    {
        juce::File imageFile;
        bool enabled = false;
        float brightness = 0.6f;
        float blur = 0.0f;
    };
    struct Snapshot
    {
        Settings settings;
        juce::Image blurredImage;
        bool validImage = false;
        float loadMultiplier = 0.0f;
    };
    std::function<void()> onChanged;
    std::function<void(Slot)> onImageNotFound;
    FluorescenceOverlayAssets()
    {
        loadPreferences();
        for(size_t i = 0; i < workers.size(); ++i)
            workers[i] = std::make_unique<SlotWorker>(*this, i);
    }
    ~FluorescenceOverlayAssets() override
    {
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
        if(workers[0]->isThreadRunning())
            return;
        for(size_t i = 0; i < workers.size(); ++i)
        {
            workers[i]->startThread();
            wakes[i].signal();
        }
    }
    Settings getSettings(Slot slot) const
    {
        const juce::ScopedLock guard(lock);
        return entries[index(slot)].settings;
    }
    Snapshot getSnapshot(Slot slot)
    {
        const juce::ScopedLock guard(lock);
        const auto& entry = entries[index(slot)];
        return { entry.settings, entry.sourceImage, entry.sourceImage.isValid(),
                 entry.loadMultiplier };
    }
    Snapshot getSnapshot(Slot slot, int width, int height)
    {
        width = juce::jmax(1, width);
        height = juce::jmax(1, height);
        juce::Image rendered;
        Settings settings;
        float multiplier = 0.0f;
        bool sourceValid = false;
        bool requestAdded = false;
        {
            const juce::ScopedLock guard(lock);
            auto& entry = entries[index(slot)];
            settings = entry.settings;
            multiplier = entry.loadMultiplier;
            sourceValid = entry.sourceImage.isValid();
            for(auto& render : entry.renders)
            {
                if(render.width == width && render.height == height)
                {
                    rendered = render.image;
                    return { settings, rendered, rendered.isValid(), multiplier };
                }
            }
            if(sourceValid)
            {
                entry.renders.push_back({ width, height, {}, true });
                requestAdded = true;
            }
        }
        if(requestAdded)
            wakes[index(slot)].signal();
        return { settings, rendered, false, multiplier };
    }
    void setImageFile(Slot slot, const juce::File& file)
    {
        {
            const juce::ScopedLock guard(lock);
            auto& entry = entries[index(slot)];
            if(entry.settings.imageFile == file) return;
            entry.settings.imageFile = file;
            entry.sourceImage = {};
            entry.renders.clear();
            entry.loadMultiplier = 0.0f;
            entry.fading = false;
            entry.forceReload = true;
        }
        savePreferences();
        wakes[index(slot)].signal();
        if(onChanged) onChanged();
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
    void setBlur(Slot slot, float value)
    {
        const juce::ScopedLock guard(lock);
        entries[index(slot)].settings.blur = juce::jlimit(0.0f, 1.0f, value);
    }
    void commitBlur(Slot slot)
    {
        {
            const juce::ScopedLock guard(lock);
            auto& entry = entries[index(slot)];
            entry.committedBlur = entry.settings.blur;
            entry.forceReload = true;
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
            file.setValue(stem + "Path", copy[(size_t) i].imageFile.getFullPathName());
            file.setValue(stem + "Enabled", copy[(size_t) i].enabled);
            file.setValue(stem + "Brightness", (double) copy[(size_t) i].brightness);
            file.setValue(stem + "Blur", (double) copy[(size_t) i].blur);
        }
        file.saveIfNeeded();
        const auto hash = contentHash(FluorescencePreferences::settingsFile());
        {
            const juce::ScopedLock guard(lock);
            settingsHash = hash;
        }
    }
private:
    struct Stamp
    {
        juce::String path;
        juce::int64 size = -1;
        juce::int64 modifiedMs = -1;
        juce::String contentHash;
        float committedBlur = -1.0f;
        bool operator==(const Stamp& rhs) const noexcept
        {
            return path == rhs.path && size == rhs.size
                   && modifiedMs == rhs.modifiedMs
                   && contentHash == rhs.contentHash
                   && committedBlur == rhs.committedBlur;
        }
    };
    struct Entry
    {
        struct Render
        {
            int width = 0;
            int height = 0;
            juce::Image image;
            bool pending = false;
        };
        Settings settings;
        juce::Image sourceImage;
        std::vector<Render> renders;
        bool forceReload = true;
        bool fadePending = false;
        bool fading = false;
        bool imageNotFoundPending = false;
        float committedBlur = 0.0f;
        float loadMultiplier = 0.0f;
        double fadeStartedAtMs = 0.0;
    };
    static size_t index(Slot slot) noexcept { return static_cast<size_t>(slot); }
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
            const auto path = file.getValue(stem + "Path");
            result[(size_t) i].imageFile = path.isNotEmpty() ? juce::File(path) : juce::File();
            result[(size_t) i].enabled = file.getBoolValue(stem + "Enabled", false);
            result[(size_t) i].brightness = juce::jlimit(0.0f, 1.0f,
                (float) file.getDoubleValue(stem + "Brightness", 0.6));
            result[(size_t) i].blur = juce::jlimit(0.0f, 1.0f,
                (float) file.getDoubleValue(stem + "Blur", 0.0));
        }
        return true;
    }
    void loadPreferences()
    {
        std::array<Settings, 2> loaded;
        if(! readPreferences(loaded))
            return;
        const auto hash = contentHash(FluorescencePreferences::settingsFile());
        const juce::ScopedLock guard(lock);
        settingsHash = hash;
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
                const bool slotChanged = sourceChanged
                                      || entry.settings.enabled != loaded[i].enabled
                                      || entry.settings.brightness != loaded[i].brightness;
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
        result.committedBlur = s.blur;
        if(s.imageFile.existsAsFile())
        {
            result.size = s.imageFile.getSize();
            result.modifiedMs = s.imageFile.getLastModificationTime().toMilliseconds();
            result.contentHash = contentHash(s.imageFile);
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
                        entry.loadMultiplier = 0.0f;
                        entry.fadePending = false;
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
            while(! worker.threadShouldExit())
            {
                juce::Image source;
                float blur = 0.0f;
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
                    blur = entry.committedBlur;
                }
                auto rendered = OverlayRendering::makeCoverImage(
                    source, renderWidth, renderHeight);
                rendered = gaussianBlur(rendered, blur);
                bool renderPublished = false;
                {
                    const juce::ScopedLock guard(lock);
                    auto& entry = entries[slotIndex];
                    if(entry.sourceImage == source && entry.committedBlur == blur)
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
                            target->image = rendered;
                            if(! entry.fadePending && ! entry.fading
                                && entry.loadMultiplier <= 0.0001f)
                                entry.fadePending = rendered.isValid();
                            renderPublished = true;
                        }
                    }
                }
                if(renderPublished)
                    triggerAsyncUpdate();
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
                    continue;
                entry.fadePending = false;
                entry.loadMultiplier = 0.0f;
                entry.fadeStartedAtMs = now;
                entry.fading = std::any_of(
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
        constexpr double fadeDurationMs = 200.0;
        const double now = juce::Time::getMillisecondCounterHiRes();
        bool anyFading = false;
        {
            const juce::ScopedLock guard(lock);
            for(auto& entry : entries)
            {
                if(! entry.fading)
                    continue;
                const float phase = juce::jlimit(
                    0.0f, 1.0f, (float) ((now - entry.fadeStartedAtMs) / fadeDurationMs));
                entry.loadMultiplier = 0.5f - 0.5f
                    * std::cos(phase * juce::MathConstants<float>::pi);
                entry.fading = phase < 1.0f;
                anyFading = anyFading || entry.fading;
            }
        }
        if(! anyFading)
            stopTimer();
        if(onChanged) onChanged();
    }
    mutable juce::CriticalSection lock;
    std::array<Entry, 2> entries;
    juce::String settingsHash;
    std::array<juce::WaitableEvent, 2> wakes;
    std::array<std::unique_ptr<SlotWorker>, 2> workers;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FluorescenceOverlayAssets)
};