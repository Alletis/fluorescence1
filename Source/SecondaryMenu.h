#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "SmoothColour.h"
#include <cmath>
#include <functional>
#include <atomic>
#include <memory>
#ifndef ALLETIS_FLUORESCENCE_GPU_BACKDROP_BLIT_PROBE
 #define ALLETIS_FLUORESCENCE_GPU_BACKDROP_BLIT_PROBE 1
#endif
#ifndef ALLETIS_FLUORESCENCE_EXPERIMENTAL_OPENGL_UI
 #define ALLETIS_FLUORESCENCE_EXPERIMENTAL_OPENGL_UI 0
#endif
#ifndef ALLETIS_GPU_BACKDROP_BLUR_ENABLED
 #define ALLETIS_GPU_BACKDROP_BLUR_ENABLED 1
#endif
#ifndef ALLETIS_GPU_BACKDROP_IMMEDIATE_OPEN
 #define ALLETIS_GPU_BACKDROP_IMMEDIATE_OPEN 1
#endif
#if ALLETIS_FLUORESCENCE_GPU_BACKDROP_BLIT_PROBE && ALLETIS_FLUORESCENCE_EXPERIMENTAL_OPENGL_UI
 #include <juce_opengl/juce_opengl.h>
 #define ALLETIS_GPU_BACKDROP_PROBE_ACTIVE 1
#else
 #define ALLETIS_GPU_BACKDROP_PROBE_ACTIVE 0
#endif
#if ALLETIS_GPU_BACKDROP_PROBE_ACTIVE
class AlletisGpuBackdropProbeTexture final : public juce::ReferenceCountedObject
{
public:
    AlletisGpuBackdropProbeTexture()
    {
        juce::gl::glGenTextures(1, &texture);
        juce::gl::glGenTextures(1, &intermediateTexture);
    }
    ~AlletisGpuBackdropProbeTexture() override
    {
        if(texture != 0)
            juce::gl::glDeleteTextures(1, &texture);
        if(intermediateTexture != 0)
            juce::gl::glDeleteTextures(1, &intermediateTexture);
    }
    GLuint texture = 0;
    GLuint intermediateTexture = 0;
    int width = 0, height = 0;
    int intermediateWidth = 0, intermediateHeight = 0;
};
#endif
class MenuDotsButton : public juce::Button,
                       private juce::Timer
{
public:
    MenuDotsButton() : juce::Button("secondaryMenuA")
    {
        setWantsKeyboardFocus(false);
        startTimerHz(60);
    }
    ~MenuDotsButton() override { stopTimer(); }
    void setActive(bool a) { active = a; }
private:
    void paintButton(juce::Graphics& g, bool, bool) override
    {
        const auto b = getLocalBounds().toFloat().reduced(1.5f);
        const float corner = juce::jmin(12.0f, b.getWidth() * 0.28f);
        g.setColour(bg.get());
        g.fillRoundedRectangle(b, corner);
        g.setColour(border.get());
        g.drawRoundedRectangle(b, corner, 1.5f);
        const float r = juce::jmax(1.7f, b.getWidth() * 0.052f);
        const float gap = r * 3.2f;
        const float cx = b.getCentreX();
        const float cy = b.getCentreY();
        g.setColour(dot.get());
        for(int i = -1; i <= 1; ++i)
            g.fillEllipse(cx + (float) i * gap - r, cy - r, 2.0f * r, 2.0f * r);
    }
    void timerCallback() override
    {
        const bool over = isOver(), down = isDown();
        const bool on = active;
        const juce::Colour bgT = on ? (down ? juce::Colour(0xff1b2a2d)
                                      : over ? juce::Colour(0xff31474b)
                                             : juce::Colour(0xff263436))
                                    : (down ? juce::Colour(0xff1f1f26)
                                      : over ? juce::Colour(0xff31313c)
                                             : juce::Colour(0xff26262e));
        const juce::Colour borderT = down ? juce::Colour(0xff307a7c)
                                   : over ? juce::Colour(0xff6ecdd0)
                                   : on ? juce::Colour(0xff45aeb1)
                                          : juce::Colour(0xff3a3a46);
        const juce::Colour dotT = down ? juce::Colour(0xff7fc9cc)
                                   : over ? juce::Colour(0xff9fe3e5)
                                   : on ? juce::Colour(0xff9fe3e5)
                                          : juce::Colour(0xff8a8a92);
        if(! primed) { bg.set(bgT); border.set(borderT); dot.set(dotT); primed = true; repaint(); return; }
        bool moving = false;
        moving |= bg.approach(bgT, rate);
        moving |= border.approach(borderT, rate);
        moving |= dot.approach(dotT, rate);
        if(moving) repaint();
    }
    SmoothColour bg, border, dot;
    bool active = false, primed = false;
    const float rate = 1.0f / 3.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MenuDotsButton)
};
class RoundToggle : public juce::Button,
                    private juce::Timer
{
public:
    RoundToggle() : juce::Button("roundToggle")
    {
        setWantsKeyboardFocus(false);
        setClickingTogglesState(true);
        startTimerHz(60);
    }
    ~RoundToggle() override { stopTimer(); }
private:
    void paintButton(juce::Graphics& g, bool, bool) override
    {
        auto b = getLocalBounds().toFloat().reduced(1.0f);
        const float d = juce::jmin(b.getWidth(), b.getHeight());
        const auto circle = juce::Rectangle<float> (d, d).withCentre(b.getCentre());
        g.setColour(fill.get());
        g.fillEllipse(circle);
    }
    void timerCallback() override
    {
        const bool over = isOver(), down = isDown(), on = getToggleState();
        const juce::Colour fillT = on ? (down ? juce::Colour(0xff307a7c)
                                       : over ? juce::Colour(0xff6ecdd0)
                                              : juce::Colour(0xff45aeb1))
                                      : (down ? juce::Colour(0xff5a5a64)
                                       : over ? juce::Colour(0xffb8b8c0)
                                              : juce::Colour(0xff8a8a92));
        if(! primed) { fill.set(fillT); primed = true; repaint(); return; }
        if(fill.approach(fillT, rate)) repaint();
    }
    SmoothColour fill;
    bool primed = false;
    const float rate = 1.0f / 3.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RoundToggle)
};
class FlyoutPanel : public juce::Component,
                    private juce::Timer
{
public:
    FlyoutPanel()
    {
        setVisible(false);
        setAlpha(0.0f);
        setOpaque(! ALLETIS_GPU_BACKDROP_PROBE_ACTIVE);
        setInterceptsMouseClicks(true, true);
        startTimerHz(60);
    }
    ~FlyoutPanel() override { stopTimer(); }
    std::function<void()> onClose;
    std::function<void(bool)> onPinnedChanged;
    bool isOpen() const noexcept { return open; }
    bool isPinned() const noexcept { return pinned; }
    void setPinned(bool p)
    {
        if(p != pinned) { pinned = p; repaint(); }
    }
    bool hitTestPin(juce::Point<float> localPos) const { return pinHitArea().contains(localPos); }
    void prewarmBackdrop(bool forceRefresh = false)
    {
#if ALLETIS_GPU_BACKDROP_PROBE_ACTIVE
        juce::ignoreUnused(forceRefresh);
#else
        if(forceRefresh || ! blurredBackdrop.isValid())
            refreshBlurredBackdrop();
#endif
    }
    void show()
    {
        if(open) return;
        open = true;
    #if ALLETIS_GPU_BACKDROP_PROBE_ACTIVE
        backdropReady.store(false, std::memory_order_release);
        backdropCapturePending.store(true, std::memory_order_release);
  #if ALLETIS_GPU_BACKDROP_IMMEDIATE_OPEN
        waitingForBackdrop = false;
        anim = 0.0f;
        animStart = anim;
        animEnd = anim;
        animInProgress = false;
  #else
        waitingForBackdrop = true;
        anim = captureWarmupAlpha;
        animStart = anim;
        animEnd = anim;
        animInProgress = false;
        applyAnim();
  #endif
    #else
        beginAnimation(1.0f);
    #endif
        setVisible(true);
        toFront(false);
#if ALLETIS_GPU_BACKDROP_PROBE_ACTIVE && ALLETIS_GPU_BACKDROP_IMMEDIATE_OPEN
        applyAnim();
        beginAnimation(1.0f);
#endif
        prewarmBackdrop(true);
        repaintBackdropRegion();
    }
    void close()
    {
        if(! open) return;
        open = false;
#if ALLETIS_GPU_BACKDROP_PROBE_ACTIVE
        waitingForBackdrop = false;
#endif
        beginAnimation(0.0f);
        repaintBackdropRegion();
        if(onClose) onClose();
    }
    void toggle() { open ? close() : show(); }
    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(0.75f);
        juce::Path clipPath;
        clipPath.addRoundedRectangle(b, 12.0f);
#if ALLETIS_GPU_BACKDROP_PROBE_ACTIVE
        if(juce::OpenGLContext::getCurrentContext() != nullptr)
        {
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(clipPath);
            paintOpenGLBackdropCopy(g);
        }
        else
        {
            g.setColour(juce::Colour(0xff17171c));
            g.fillRoundedRectangle(b, 12.0f);
        }
  #if ALLETIS_GPU_BACKDROP_IMMEDIATE_OPEN
        g.setColour(juce::Colour(0xff17171c).withAlpha(0.5f * anim));
  #else
        g.setColour(juce::Colour(0xff17171c).withAlpha(0.5f));
  #endif
        g.fillRoundedRectangle(b, 12.0f);
#else
        g.fillAll(juce::Colour(0xff17171c));
        if(originalBackdrop.isValid())
            g.drawImageWithin(originalBackdrop, 0, 0, getWidth(), getHeight(),
                              juce::RectanglePlacement::stretchToFit);
        if(blurredBackdrop.isValid())
        {
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(clipPath);
            g.drawImageWithin(blurredBackdrop, 0, 0, getWidth(), getHeight(),
                              juce::RectanglePlacement::stretchToFit);
        }
        g.setColour(juce::Colour(0xff17171c).withAlpha(0.5f));
        g.fillRoundedRectangle(b, 12.0f);
#endif
#if ALLETIS_GPU_BACKDROP_PROBE_ACTIVE && ALLETIS_GPU_BACKDROP_IMMEDIATE_OPEN
        g.setColour(juce::Colour(0xff34343e).withMultipliedAlpha(anim));
#else
        g.setColour(juce::Colour(0xff34343e));
#endif
        g.drawRoundedRectangle(b, 12.0f, 1.0f);
        drawPin(g);
    }
#if ALLETIS_GPU_BACKDROP_PROBE_ACTIVE
    void paintOpenGLBackdropCopy(juce::Graphics& g)
    {
#if ALLETIS_GPU_BACKDROP_BLUR_ENABLED
        static const juce::String horizontalShaderText = R"GLSL(
            uniform sampler2D sceneCopy;
            uniform vec2 sceneSize;
            uniform float blurSigmaPx;
            void main()
            {
                vec2 uv = vec2(pixelPos.x / sceneSize.x,
                               1.0 - pixelPos.y / sceneSize.y);
                vec2 texel = vec2(1.0) / sceneSize;
                vec2 uvMin = 0.5 / sceneSize;
                vec2 uvMax = vec2(1.0) - uvMin;
                vec4 colour = vec4(0.0);
                float weightSum = 0.0;
                float sigma2 = blurSigmaPx * blurSigmaPx;
                for(int x = -6; x <= 6; ++x)
                {
                    float offsetPx = float(x);
                    float weight = exp(-(offsetPx * offsetPx) / (2.0 * sigma2));
                    vec2 sampleUV = clamp(uv + vec2(offsetPx * texel.x, 0.0),
                                          uvMin, uvMax);
                    colour += texture2D(sceneCopy, sampleUV) * weight;
                    weightSum += weight;
                }
                gl_FragColor = (colour / weightSum) * pixelAlpha;
            }
        )GLSL";
        static const juce::String verticalShaderText = R"GLSL(
            uniform sampler2D horizontalCopy;
            uniform sampler2D sceneCopy;
            uniform vec2 sceneSize;
            uniform float blurSigmaPx;
            uniform float backdropOpacity;
            void main()
            {
                vec2 uv = vec2(pixelPos.x / sceneSize.x,
                               1.0 - pixelPos.y / sceneSize.y);
                vec2 texel = vec2(1.0) / sceneSize;
                vec2 uvMin = 0.5 / sceneSize;
                vec2 uvMax = vec2(1.0) - uvMin;
                vec4 colour = vec4(0.0);
                float weightSum = 0.0;
                float sigma2 = blurSigmaPx * blurSigmaPx;
                for(int y = -6; y <= 6; ++y)
                {
                    float offsetPx = float(y);
                    float weight = exp(-(offsetPx * offsetPx) / (2.0 * sigma2));
                    vec2 sampleUV = clamp(uv + vec2(0.0, offsetPx * texel.y),
                                          uvMin, uvMax);
                    colour += texture2D(horizontalCopy, sampleUV) * weight;
                    weightSum += weight;
                }
                vec4 original = texture2D(sceneCopy, clamp(uv, uvMin, uvMax));
                vec4 blurred = colour / weightSum;
                gl_FragColor = mix(original, blurred, backdropOpacity) * pixelAlpha;
            }
        )GLSL";
        constexpr float blurSigmaLogicalPx = 3.0f;
        const auto* topLevel = getTopLevelComponent();
        const int topLevelWidth = juce::jmax(1, topLevel != nullptr ? topLevel->getWidth() : getWidth());
        const float componentScale = topLevel != nullptr && getWidth() > 0
            ? (float) topLevel->getLocalArea(this, getLocalBounds()).getWidth() / (float) getWidth()
            : 1.0f;
#else
        static const juce::String shaderText = R"GLSL(
            uniform sampler2D sceneCopy;
            uniform vec2 sceneSize;
            void main()
            {
                vec2 uv = vec2(pixelPos.x / sceneSize.x,
                               1.0 - pixelPos.y / sceneSize.y);
                gl_FragColor = texture2D(sceneCopy,
                                         clamp(uv, vec2(0.0), vec2(1.0))) * pixelAlpha;
            }
        )GLSL";
#endif
        if(gpuHorizontalShader == nullptr)
            gpuHorizontalShader = std::make_unique<juce::OpenGLGraphicsContextCustomShader>(horizontalShaderText);
        if(gpuVerticalShader == nullptr)
            gpuVerticalShader = std::make_unique<juce::OpenGLGraphicsContextCustomShader>(verticalShaderText);
#if ALLETIS_GPU_BACKDROP_BLUR_ENABLED
        gpuHorizontalShader->onShaderActivated = [this, topLevelWidth, componentScale, blurSigmaLogicalPx]
            (juce::OpenGLShaderProgram& program)
#else
        gpuCopyShader->onShaderActivated = [this] (juce::OpenGLShaderProgram& program)
#endif
        {
            auto* context = juce::OpenGLContext::getCurrentContext();
            if(context == nullptr)
                return;
            GLint viewport[4]{};
            juce::gl::glGetIntegerv(juce::gl::GL_VIEWPORT, viewport);
            const int width = viewport[2], height = viewport[3];
            if(width < 1 || height < 1)
                return;
            constexpr const char* cacheName = "AlletisFlyoutGpuBlitProbeTexture";
            auto* cache = static_cast<AlletisGpuBackdropProbeTexture*> (
                context->getAssociatedObject(cacheName));
            if(cache == nullptr)
            {
                cache = new AlletisGpuBackdropProbeTexture();
                context->setAssociatedObject(cacheName, cache);
            }
            GLint previousUnit = juce::gl::GL_TEXTURE0;
            juce::gl::glGetIntegerv(juce::gl::GL_ACTIVE_TEXTURE, &previousUnit);
            context->extensions.glActiveTexture(juce::gl::GL_TEXTURE3);
            juce::gl::glBindTexture(juce::gl::GL_TEXTURE_2D, cache->texture);
            const bool textureResized = cache->width != width || cache->height != height;
            if(textureResized)
            {
                juce::gl::glTexParameteri(juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MIN_FILTER,
                                          juce::gl::GL_LINEAR);
                juce::gl::glTexParameteri(juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MAG_FILTER,
                                          juce::gl::GL_LINEAR);
                juce::gl::glTexParameteri(juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_WRAP_S,
                                          juce::gl::GL_CLAMP_TO_EDGE);
                juce::gl::glTexParameteri(juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_WRAP_T,
                                          juce::gl::GL_CLAMP_TO_EDGE);
                juce::gl::glTexImage2D(juce::gl::GL_TEXTURE_2D, 0, juce::gl::GL_RGBA,
                                       width, height, 0, juce::gl::GL_RGBA,
                                       juce::gl::GL_UNSIGNED_BYTE, nullptr);
                cache->width = width;
                cache->height = height;
            }
            const bool captureRequested =
                backdropCapturePending.exchange(false, std::memory_order_acq_rel);
            if(textureResized || captureRequested)
            {
                GLint previousReadBuffer = juce::gl::GL_BACK;
                GLint framebuffer = 0;
                juce::gl::glGetIntegerv(juce::gl::GL_READ_BUFFER, &previousReadBuffer);
                juce::gl::glGetIntegerv(juce::gl::GL_FRAMEBUFFER_BINDING, &framebuffer);
                juce::gl::glReadBuffer(framebuffer != 0 ? juce::gl::GL_COLOR_ATTACHMENT0
                                                        : juce::gl::GL_BACK);
                juce::gl::glCopyTexSubImage2D(juce::gl::GL_TEXTURE_2D, 0, 0, 0,
                                              viewport[0], viewport[1], width, height);
                juce::gl::glReadBuffer((GLenum) previousReadBuffer);
                backdropReady.store(true, std::memory_order_release);
            }
            juce::OpenGLShaderProgram::Uniform sceneCopy(program, "sceneCopy");
            juce::OpenGLShaderProgram::Uniform sceneSize(program, "sceneSize");
            sceneCopy.set(3);
            sceneSize.set((GLfloat) width, (GLfloat) height);
#if ALLETIS_GPU_BACKDROP_BLUR_ENABLED
            juce::OpenGLShaderProgram::Uniform blurSigmaPx(program, "blurSigmaPx");
            const float displayScale = (float) width / (float) topLevelWidth;
            const float physicalScale = juce::jmax(0.25f, componentScale * displayScale);
            blurSigmaPx.set((GLfloat) juce::jmax(0.5f,
                blurSigmaLogicalPx * physicalScale));
#endif
            context->extensions.glActiveTexture((GLenum) previousUnit);
        };
#if ALLETIS_GPU_BACKDROP_BLUR_ENABLED
        gpuHorizontalShader->fillRect(g.getInternalContext(), getLocalBounds());
        gpuVerticalShader->onShaderActivated = [this, topLevelWidth, componentScale, blurSigmaLogicalPx]
            (juce::OpenGLShaderProgram& program)
        {
            auto* context = juce::OpenGLContext::getCurrentContext();
            if(context == nullptr)
                return;
            GLint viewport[4]{};
            juce::gl::glGetIntegerv(juce::gl::GL_VIEWPORT, viewport);
            const int width = viewport[2], height = viewport[3];
            if(width < 1 || height < 1)
                return;
            constexpr const char* cacheName = "AlletisFlyoutGpuBlitProbeTexture";
            auto* cache = static_cast<AlletisGpuBackdropProbeTexture*> (
                context->getAssociatedObject(cacheName));
            if(cache == nullptr)
                return;
            GLint previousUnit = juce::gl::GL_TEXTURE0;
            GLint previousReadBuffer = juce::gl::GL_BACK;
            GLint framebuffer = 0;
            juce::gl::glGetIntegerv(juce::gl::GL_ACTIVE_TEXTURE, &previousUnit);
            juce::gl::glGetIntegerv(juce::gl::GL_READ_BUFFER, &previousReadBuffer);
            juce::gl::glGetIntegerv(juce::gl::GL_FRAMEBUFFER_BINDING, &framebuffer);
            context->extensions.glActiveTexture(juce::gl::GL_TEXTURE3);
            juce::gl::glBindTexture(juce::gl::GL_TEXTURE_2D, cache->intermediateTexture);
            juce::gl::glTexParameteri(juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MIN_FILTER,
                                      juce::gl::GL_LINEAR);
            juce::gl::glTexParameteri(juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MAG_FILTER,
                                      juce::gl::GL_LINEAR);
            juce::gl::glTexParameteri(juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_WRAP_S,
                                      juce::gl::GL_CLAMP_TO_EDGE);
            juce::gl::glTexParameteri(juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_WRAP_T,
                                      juce::gl::GL_CLAMP_TO_EDGE);
            if(cache->intermediateWidth != width || cache->intermediateHeight != height)
            {
                juce::gl::glTexImage2D(juce::gl::GL_TEXTURE_2D, 0, juce::gl::GL_RGBA,
                                       width, height, 0, juce::gl::GL_RGBA,
                                       juce::gl::GL_UNSIGNED_BYTE, nullptr);
                cache->intermediateWidth = width;
                cache->intermediateHeight = height;
            }
            juce::gl::glCopyTexSubImage2D(juce::gl::GL_TEXTURE_2D, 0, 0, 0,
                                          viewport[0], viewport[1], width, height);
            context->extensions.glActiveTexture(juce::gl::GL_TEXTURE4);
            juce::gl::glBindTexture(juce::gl::GL_TEXTURE_2D, cache->texture);
            juce::OpenGLShaderProgram::Uniform horizontalCopy(program, "horizontalCopy");
            juce::OpenGLShaderProgram::Uniform sceneCopy(program, "sceneCopy");
            juce::OpenGLShaderProgram::Uniform sceneSize(program, "sceneSize");
            juce::OpenGLShaderProgram::Uniform blurSigmaPx(program, "blurSigmaPx");
            juce::OpenGLShaderProgram::Uniform backdropOpacity(program, "backdropOpacity");
            horizontalCopy.set(3);
            sceneCopy.set(4);
            sceneSize.set((GLfloat) width, (GLfloat) height);
            const float displayScale = (float) width / (float) topLevelWidth;
            const float physicalScale = juce::jmax(0.25f, componentScale * displayScale);
            blurSigmaPx.set((GLfloat) juce::jmax(0.5f,
                blurSigmaLogicalPx * physicalScale));
            backdropOpacity.set((GLfloat) juce::jlimit(0.0f, 1.0f, anim));
            juce::gl::glReadBuffer((GLenum) previousReadBuffer);
            context->extensions.glActiveTexture((GLenum) previousUnit);
        };
        gpuVerticalShader->fillRect(g.getInternalContext(), getLocalBounds());
#else
        gpuCopyShader->fillRect(g.getInternalContext(), getLocalBounds());
#endif
    }
#endif
    void drawPin(juce::Graphics& g)
    {
        const auto area = pinHitArea();
        const float s = juce::jmin(area.getWidth(), area.getHeight());
        const float cx = area.getCentreX();
        const float cy = area.getCentreY();
        const float tilt = 0.38f;
        const float dx = -std::sin(tilt);
        const float dy = std::cos(tilt);
        const float headR = s * 0.155f;
        const juce::Point<float> headC(cx - dx * s * 0.13f, cy - dy * s * 0.13f);
        const juce::Point<float> tip(cx + dx * s * 0.36f, cy + dy * s * 0.36f);
        const juce::Point<float> start(headC.x + dx * headR * 0.5f,
                                        headC.y + dy * headR * 0.5f);
#if ALLETIS_GPU_BACKDROP_PROBE_ACTIVE && ALLETIS_GPU_BACKDROP_IMMEDIATE_OPEN
        g.setColour(pinS.get().withMultipliedAlpha(anim));
#else
        g.setColour(pinS.get());
#endif
        g.fillEllipse(headC.x - headR, headC.y - headR, headR * 2.0f, headR * 2.0f);
        juce::Path shaft;
        shaft.startNewSubPath(start);
        shaft.lineTo(tip);
        g.strokePath(shaft, juce::PathStrokeType(s * 0.09f, juce::PathStrokeType::curved,
                                                              juce::PathStrokeType::rounded));
    }
    void moved() override { invalidateBackdrop(); }
    void resized() override { invalidateBackdrop(); }
    void mouseDown(const juce::MouseEvent& e) override
    {
        if(pinHitArea().contains(e.position))
        {
            pinArmed = true;
            pinHover = true;
            updatePinColour();
            repaint();
        }
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if(! pinArmed) return;
        const bool over = pinHitArea().contains(e.position);
        if(over != pinHover) { pinHover = over; updatePinColour(); repaint(); }
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        const bool commit = pinArmed && pinHitArea().contains(e.position);
        pinArmed = false;
        if(commit)
        {
            pinned = ! pinned;
            if(onPinnedChanged) onPinnedChanged(pinned);
        }
        pinHover = pinHitArea().contains(e.position);
        updatePinColour();
        repaint();
    }
    void mouseMove(const juce::MouseEvent& e) override
    {
        const bool h = pinHitArea().contains(e.position);
        if(h != pinHover) { pinHover = h; updatePinColour(); repaint(); }
    }
    void mouseEnter(const juce::MouseEvent& e) override { mouseMove(e); }
    void mouseExit(const juce::MouseEvent&) override
    {
        if(pinHover && ! pinArmed) { pinHover = false; updatePinColour(); repaint(); }
    }
private:
    juce::Rectangle<float> pinHitArea() const { return { 5.0f, 5.0f, 24.0f, 24.0f }; }
    juce::Colour pinTarget() const
    {
        const bool pressed = pinArmed && pinHover;
        if(pinned)
            return pressed ? juce::Colour(0xff307a7c)
                 : pinHover ? juce::Colour(0xff6ecdd0)
                            : juce::Colour(0xff45aeb1);
        return pressed ? juce::Colour(0xff5a5a64)
             : pinHover ? juce::Colour(0xffb8b8c0)
                        : juce::Colour(0xff8a8a92);
    }
    void updatePinColour()
    {
        const auto t = pinTarget();
        if(! pinPrimed) { pinS.set(t); pinPrimed = true; return; }
        if(pinS.approach(t, 1.0f / 3.0f) && open)
            repaint();
    }
    static void boxBlur1DHorizontal(const juce::Image& src, juce::Image& dst, int radius)
    {
        const int w = src.getWidth();
        const int h = src.getHeight();
        juce::Image::BitmapData srcData(src, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData dstData(dst, juce::Image::BitmapData::writeOnly);
        const int windowSize = radius * 2 + 1;
        for(int y = 0; y < h; ++y)
        {
            const auto* srcLine = srcData.getLinePointer(y);
            auto* dstLine = dstData.getLinePointer(y);
            int sumA = 0, sumR = 0, sumG = 0, sumB = 0;
            for(int k = -radius; k <= radius; ++k)
            {
                const int sx = juce::jlimit(0, w - 1, k);
                const auto* px = reinterpret_cast<const juce::PixelARGB*> (srcLine + sx * srcData.pixelStride);
                sumA += (int) px->getAlpha();
                sumR += (int) px->getRed();
                sumG += (int) px->getGreen();
                sumB += (int) px->getBlue();
            }
            for(int x = 0; x < w; ++x)
            {
                auto* out = reinterpret_cast<juce::PixelARGB*> (dstLine + x * dstData.pixelStride);
                out->setARGB((juce::uint8) (sumA / windowSize),
                              (juce::uint8) (sumR / windowSize),
                              (juce::uint8) (sumG / windowSize),
                              (juce::uint8) (sumB / windowSize));
                if(x == w - 1)
                    continue;
                const int sxOut = juce::jlimit(0, w - 1, x - radius);
                const int sxIn = juce::jlimit(0, w - 1, x + radius + 1);
                const auto* outPx = reinterpret_cast<const juce::PixelARGB*> (srcLine + sxOut * srcData.pixelStride);
                const auto* inPx = reinterpret_cast<const juce::PixelARGB*> (srcLine + sxIn * srcData.pixelStride);
                sumA += (int) inPx->getAlpha() - (int) outPx->getAlpha();
                sumR += (int) inPx->getRed() - (int) outPx->getRed();
                sumG += (int) inPx->getGreen() - (int) outPx->getGreen();
                sumB += (int) inPx->getBlue() - (int) outPx->getBlue();
            }
        }
    }
    static void boxBlur1DVertical(const juce::Image& src, juce::Image& dst, int radius)
    {
        const int w = src.getWidth();
        const int h = src.getHeight();
        juce::Image::BitmapData srcData(src, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData dstData(dst, juce::Image::BitmapData::writeOnly);
        const int windowSize = radius * 2 + 1;
        for(int x = 0; x < w; ++x)
        {
            int sumA = 0, sumR = 0, sumG = 0, sumB = 0;
            for(int k = -radius; k <= radius; ++k)
            {
                const int sy = juce::jlimit(0, h - 1, k);
                const auto* px = reinterpret_cast<const juce::PixelARGB*> (
                    srcData.getLinePointer(sy) + x * srcData.pixelStride);
                sumA += (int) px->getAlpha();
                sumR += (int) px->getRed();
                sumG += (int) px->getGreen();
                sumB += (int) px->getBlue();
            }
            for(int y = 0; y < h; ++y)
            {
                auto* out = reinterpret_cast<juce::PixelARGB*> (
                    dstData.getLinePointer(y) + x * dstData.pixelStride);
                out->setARGB((juce::uint8) (sumA / windowSize),
                              (juce::uint8) (sumR / windowSize),
                              (juce::uint8) (sumG / windowSize),
                              (juce::uint8) (sumB / windowSize));
                if(y == h - 1)
                    continue;
                const int syOut = juce::jlimit(0, h - 1, y - radius);
                const int syIn = juce::jlimit(0, h - 1, y + radius + 1);
                const auto* outPx = reinterpret_cast<const juce::PixelARGB*> (
                    srcData.getLinePointer(syOut) + x * srcData.pixelStride);
                const auto* inPx = reinterpret_cast<const juce::PixelARGB*> (
                    srcData.getLinePointer(syIn) + x * srcData.pixelStride);
                sumA += (int) inPx->getAlpha() - (int) outPx->getAlpha();
                sumR += (int) inPx->getRed() - (int) outPx->getRed();
                sumG += (int) inPx->getGreen() - (int) outPx->getGreen();
                sumB += (int) inPx->getBlue() - (int) outPx->getBlue();
            }
        }
    }
    void fastBoxBlur(juce::Image& img, int radius, int passes)
    {
        if(! img.isValid() || radius <= 0 || passes <= 0)
            return;
        const bool scratchNeedsRebuilding = ! blurScratch.isValid()
                                         || blurScratch.getWidth() != img.getWidth()
                                         || blurScratch.getHeight() != img.getHeight();
        if(scratchNeedsRebuilding)
        {
            blurScratch = juce::Image(juce::Image::ARGB,
                                       img.getWidth(),
                                       img.getHeight(),
                                       true);
        }
        for(int i = 0; i < passes; ++i)
        {
            boxBlur1DHorizontal(img, blurScratch, radius);
            boxBlur1DVertical(blurScratch, img, radius);
        }
    }
    void refreshBlurredBackdrop()
    {
        auto* parent = getParentComponent();
        if(parent == nullptr)
            return;
        const auto areaInParent = getBounds();
        if(areaInParent.isEmpty())
            return;
        const float oldAlpha = getAlpha();
        setAlpha(0.0f);
        auto full = parent->createComponentSnapshot(areaInParent,
                                                    true,
                                                    1.0f,
                                                    juce::SoftwareImageType {});
        setAlpha(oldAlpha);
        if(! full.isValid())
            return;
        const int lowW = juce::jmax(1, juce::roundToInt(
            (float) full.getWidth() / (float) blurDownsampleFactor));
        const int lowH = juce::jmax(1, juce::roundToInt(
            (float) full.getHeight() / (float) blurDownsampleFactor));
        juce::Image low(juce::Image::ARGB, lowW, lowH, true);
        {
            juce::Graphics g(low);
            g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
            g.drawImage(full, low.getBounds().toFloat());
        }
        originalBackdrop = std::move(full);
        fastBoxBlur(low, blurRadiusLowRes, blurPasses);
        blurredBackdrop = std::move(low);
    }
    void timerCallback() override
    {
        updatePinColour();
#if ALLETIS_GPU_BACKDROP_PROBE_ACTIVE && ! ALLETIS_GPU_BACKDROP_IMMEDIATE_OPEN
        if(waitingForBackdrop)
        {
            if(! backdropReady.load(std::memory_order_acquire))
            {
                repaintBackdropRegion();
                return;
            }
            waitingForBackdrop = false;
            beginAnimation(1.0f);
        }
#endif
#if ALLETIS_GPU_BACKDROP_PROBE_ACTIVE
        if(isShowing() && (open || animInProgress))
        {
            backdropCapturePending.store(true, std::memory_order_release);
            repaintBackdropRegion();
        }
#else
        if(isShowing() && (open || animInProgress))
        {
            refreshBlurredBackdrop();
            repaint();
        }
#endif
        const float target = open ? 1.0f : 0.0f;
        if(target != animEnd)
            beginAnimation(target);
        if(animInProgress)
        {
            const double elapsedMs = juce::Time::getMillisecondCounterHiRes() - animStartMs;
            animPhase = juce::jlimit(0.0f, 1.0f,
                                     (float) (elapsedMs / juce::jmax(1.0, animDurationMs)));
            const float eased = 0.5f - 0.5f
                * std::cos(animPhase * juce::MathConstants<float>::pi);
            anim = animStart + (animEnd - animStart) * eased;
            applyAnim();
            if(animPhase >= 1.0f)
            {
                anim = animEnd;
                applyAnim();
                animInProgress = false;
                if(! open && isVisible())
                {
                    setVisible(false);
                    originalBackdrop = {};
                    blurredBackdrop = {};
                    repaintBackdropRegion();
                }
            }
        }
    }
    void beginAnimation(float target)
    {
        animStart = anim;
        animEnd = target;
        animPhase = 0.0f;
        animStartMs = juce::Time::getMillisecondCounterHiRes();
        animDurationMs = transitionDurationMs
            * (double) juce::jmax(0.05f, std::abs(animEnd - animStart));
        animInProgress = true;
    }
    void invalidateBackdrop()
    {
        originalBackdrop = {};
        blurredBackdrop = {};
#if ALLETIS_GPU_BACKDROP_PROBE_ACTIVE
        backdropCapturePending.store(true, std::memory_order_release);
#endif
    }
    void repaintBackdropRegion()
    {
        if(auto* parent = getParentComponent())
            parent->repaint(getBounds());
        else
            repaint();
    }
    void applyAnim()
    {
#if ALLETIS_GPU_BACKDROP_PROBE_ACTIVE && ALLETIS_GPU_BACKDROP_IMMEDIATE_OPEN
        if(getAlpha() != 1.0f)
            setAlpha(1.0f);
        for(int i = 0; i < getNumChildComponents(); ++i)
            if(auto* child = getChildComponent(i))
                if(child->getAlpha() != anim)
                    child->setAlpha(anim);
        repaint();
#else
        setAlpha(anim);
#endif
    }
    bool open = false;
    float anim = 0.0f;
    float animStart = 0.0f;
    float animEnd = 0.0f;
    float animPhase = 1.0f;
    double animStartMs = 0.0;
    double animDurationMs = 1.0;
    bool animInProgress = false;
    static constexpr double transitionDurationMs = 180.0;
#if ALLETIS_GPU_BACKDROP_PROBE_ACTIVE
    static constexpr float captureWarmupAlpha = 0.002f;
#endif
    static constexpr int blurDownsampleFactor = 4;
    static constexpr int blurRadiusLowRes = 2;
    static constexpr int blurPasses = 3;
    juce::Image originalBackdrop;
    juce::Image blurredBackdrop;
    juce::Image blurScratch;
#if ALLETIS_GPU_BACKDROP_PROBE_ACTIVE
#if ALLETIS_GPU_BACKDROP_BLUR_ENABLED
    std::unique_ptr<juce::OpenGLGraphicsContextCustomShader> gpuHorizontalShader;
    std::unique_ptr<juce::OpenGLGraphicsContextCustomShader> gpuVerticalShader;
#else
    std::unique_ptr<juce::OpenGLGraphicsContextCustomShader> gpuCopyShader;
#endif
    std::atomic<bool> backdropCapturePending { true };
    std::atomic<bool> backdropReady { false };
    bool waitingForBackdrop = false;
#endif
    bool pinned = false;
    bool pinHover = false;
    bool pinArmed = false;
    bool pinPrimed = false;
    SmoothColour pinS;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FlyoutPanel)
};