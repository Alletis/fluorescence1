#pragma once
#if ALLETIS_FLUORESCENCE_EXPERIMENTAL_OPENGL_UI
#include <juce_opengl/juce_opengl.h>
#include "OverlayAssets.h"
#include <cmath>
#include <memory>
class FluorescenceGpuBackground
{
public:
    bool paint(juce::Graphics& g, int canvasW, int canvasH,
               const FluorescenceOverlayAssets::Snapshot& source,
               const juce::Image& logo, juce::Rectangle<int> logoBounds,
               const juce::Image& brand, juce::Rectangle<int> brandBounds,
               int backgroundDarkenWidth, float backgroundScale)
    {
        auto* context = juce::OpenGLContext::getCurrentContext();
        if(context == nullptr || !source.validImage || !source.blurredImage.isValid()
            || !source.settings.enabled || canvasW <= 0 || canvasH <= 0)
            return false;
        auto* cache = static_cast<Resources*>(context->getAssociatedObject(cacheKey));
        if(cache == nullptr)
        {
            cache = new Resources();
            context->setAssociatedObject(cacheKey, cache);
        }
        const auto& raw = source.blurredImage;
        if(cache->rawImage != raw)
        {
            cache->raw.loadImage(flattenToOpaqueRGB(raw));
            cache->rawImage = raw;
            cache->blurSigma = -1.0f;
        }
        if(cache->logoImage != logo)
        {
            if(logo.isValid()) cache->logo.loadImage(flattenToOpaqueRGB(logo));
            cache->logoImage = logo;
        }
        if(cache->brandImage != brand)
        {
            if(brand.isValid()) cache->brand.loadImage(flattenToOpaqueRGB(brand));
            cache->brandImage = brand;
        }
        if(cache->raw.getTextureID() == 0)
            return false;
        const float coverScale = juce::jmax((float)canvasW / (float)raw.getWidth(),
                                             (float)canvasH / (float)raw.getHeight());
        const int coverW = juce::jmax(canvasW, juce::roundToInt((float)raw.getWidth() * coverScale));
        const int coverH = juce::jmax(canvasH, juce::roundToInt((float)raw.getHeight() * coverScale));
        const float sigmaCover = juce::jmax(0.0f, source.settings.blur)
                               * (float)juce::jmin(coverW, coverH) / 45.0f;
        const float sigmaSource = sigmaCover / coverScale;
        GLuint wallpaperID = cache->raw.getTextureID();
        int wallpaperTexW = cache->raw.getWidth(), wallpaperTexH = cache->raw.getHeight();
        if(sigmaSource >= 0.15f)
        {
            if(!updateBlur(*context, *cache, raw.getWidth(), raw.getHeight(), sigmaSource))
                return false;
            wallpaperID = cache->vertical.getTextureID();
            wallpaperTexW = cache->vertical.getWidth();
            wallpaperTexH = cache->vertical.getHeight();
        }
        if(!compositeShader)
            compositeShader = std::make_unique<juce::OpenGLGraphicsContextCustomShader>(compositeCode);
        const float resize = std::isfinite(source.settings.resize) && source.settings.resize > 0.0f
                           ? source.settings.resize : 1.0f;
        const float brightness = juce::jlimit(0.0f, 1.0f, source.settings.brightness);
        const float opacity = brightness
                    * juce::jlimit(0.0f, 1.0f, source.loadMultiplier);
        const float scaledBackground = juce::jlimit(0.0f, 1.0f, backgroundScale);
        const int mode = (int)source.settings.boundMode;
        const int logoTexW = cache->logo.getWidth(), logoTexH = cache->logo.getHeight();
        const int brandTexW = cache->brand.getWidth(), brandTexH = cache->brand.getHeight();
        compositeShader->onShaderActivated = [=, &resources = *cache](juce::OpenGLShaderProgram& program)
        {
            auto* ctx = juce::OpenGLContext::getCurrentContext();
            if(ctx == nullptr) return;
            GLint oldUnit = juce::gl::GL_TEXTURE0;
            juce::gl::glGetIntegerv(juce::gl::GL_ACTIVE_TEXTURE, &oldUnit);
            bindTexture(*ctx, 3, wallpaperID);
            bindTexture(*ctx, 4, resources.logo.getTextureID());
            bindTexture(*ctx, 5, resources.brand.getTextureID());
            uniform(program, "wallpaper", 3);
            uniform(program, "logoSampler", 4);
            uniform(program, "brandSampler", 5);
            uniform(program, "canvasSize", (float)canvasW, (float)canvasH);
            uniform(program, "coverSize", (float)coverW, (float)coverH);
            uniform(program, "sourceSize", (float)raw.getWidth(), (float)raw.getHeight());
            uniform(program, "textureSize", (float)wallpaperTexW, (float)wallpaperTexH);
            uniform(program, "logoTexSize", (float)logoTexW, (float)logoTexH);
            uniform(program, "brandTexSize", (float)brandTexW, (float)brandTexH);
            uniform(program, "logoRect", logoBounds.toFloat());
            uniform(program, "brandRect", brandBounds.toFloat());
            uniform(program, "overlayParams", resize, source.settings.translateX,
                    source.settings.translateY, opacity);
            uniform(program, "boundMode", mode);
            uniform(program, "backgroundDarkenWidth", (float)backgroundDarkenWidth);
            uniform(program, "backgroundScale", scaledBackground);
            GLint viewport[4] {};
            juce::gl::glGetIntegerv(juce::gl::GL_VIEWPORT, viewport);
            uniform(program, "targetSize", (float)juce::jmax(1,viewport[2]),
                    (float)juce::jmax(1,viewport[3]));
            ctx->extensions.glActiveTexture((GLenum)oldUnit);
        };
        if(compositeShader->checkCompilation(g.getInternalContext()).failed())
            return false;
        compositeShader->fillRect(g.getInternalContext(), {0, 0, canvasW, canvasH});
        return true;
    }
private:
    static constexpr const char* cacheKey = "AlletisGpuWallpaperBackground_opaqueRGB_v2";
    static juce::Image flattenToOpaqueRGB(const juce::Image& src)
    {
        if(!src.isValid())
            return {};
        if(src.getFormat() == juce::Image::RGB)
            return src;
        juce::Image opaque(juce::Image::RGB, src.getWidth(), src.getHeight(), true);
        {
            juce::Graphics g(opaque);
            g.fillAll(juce::Colours::black);
            g.drawImageAt(src, 0, 0);
        }
        return opaque;
    }
    struct Resources final : juce::ReferenceCountedObject
    {
        juce::OpenGLTexture raw, logo, brand;
        juce::OpenGLFrameBuffer horizontal, vertical;
        juce::Image rawImage, logoImage, brandImage;
        float blurSigma = -1.0f;
        int width = 0, height = 0;
    };
    static void bindTexture(juce::OpenGLContext& c, int unit, GLuint id)
    {
        c.extensions.glActiveTexture((GLenum)(juce::gl::GL_TEXTURE0 + unit));
        juce::gl::glBindTexture(juce::gl::GL_TEXTURE_2D, id);
        if(id == 0) return;
        juce::gl::glTexParameteri(juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MIN_FILTER, juce::gl::GL_LINEAR);
        juce::gl::glTexParameteri(juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MAG_FILTER, juce::gl::GL_LINEAR);
        juce::gl::glTexParameteri(juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_WRAP_S, juce::gl::GL_CLAMP_TO_EDGE);
        juce::gl::glTexParameteri(juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_WRAP_T, juce::gl::GL_CLAMP_TO_EDGE);
    }
    static void uniform(juce::OpenGLShaderProgram& p, const char* name, int v)
    { juce::OpenGLShaderProgram::Uniform u(p, name); if(u.uniformID >= 0) u.set(v); }
    static void uniform(juce::OpenGLShaderProgram& p, const char* name, float v)
    { juce::OpenGLShaderProgram::Uniform u(p, name); if(u.uniformID >= 0) u.set((GLfloat)v); }
    static void uniform(juce::OpenGLShaderProgram& p, const char* name, float x, float y)
    { juce::OpenGLShaderProgram::Uniform u(p, name); if(u.uniformID >= 0) u.set((GLfloat)x, (GLfloat)y); }
    static void uniform(juce::OpenGLShaderProgram& p, const char* name, float x, float y, float z, float w)
    { juce::OpenGLShaderProgram::Uniform u(p, name); if(u.uniformID >= 0) u.set((GLfloat)x,(GLfloat)y,(GLfloat)z,(GLfloat)w); }
    static void uniform(juce::OpenGLShaderProgram& p, const char* name, juce::Rectangle<float> r)
    { uniform(p,name,r.getX(),r.getY(),r.getWidth(),r.getHeight()); }
    bool updateBlur(juce::OpenGLContext& context, Resources& r, int w, int h, float sigma)
    {
        const bool resize = r.width != w || r.height != h
                         || !r.horizontal.isValid() || !r.vertical.isValid();
        if(resize)
        {
            r.horizontal.release();
            r.vertical.release();
            if(!r.horizontal.initialise(context,w,h) || !r.vertical.initialise(context,w,h))
                return false;
            r.width = w; r.height = h;
            r.blurSigma = -1.0f;
        }
        if(std::abs(r.blurSigma-sigma) < 0.005f)
            return true;
        if(!horizontalShader)
            horizontalShader = std::make_unique<juce::OpenGLGraphicsContextCustomShader>(blurCode);
        if(!verticalShader)
            verticalShader = std::make_unique<juce::OpenGLGraphicsContextCustomShader>(blurCode);
        GLint previousFramebuffer = 0;
        GLint previousViewport[4] {};
        juce::gl::glGetIntegerv(juce::gl::GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
        juce::gl::glGetIntegerv(juce::gl::GL_VIEWPORT, previousViewport);
        auto restoreFramebuffer = [&]
        {
            context.extensions.glBindFramebuffer(juce::gl::GL_FRAMEBUFFER, (GLuint)previousFramebuffer);
            juce::gl::glViewport(previousViewport[0], previousViewport[1],
                                previousViewport[2], previousViewport[3]);
        };
        auto drawPass = [&](juce::OpenGLFrameBuffer& dest,
                            juce::OpenGLGraphicsContextCustomShader& shader,
                            GLuint sourceTex, int texW, int texH, float dx, float dy)
        {
            const bool hadScissor = juce::gl::glIsEnabled(juce::gl::GL_SCISSOR_TEST) != 0;
            if(hadScissor)
                juce::gl::glDisable(juce::gl::GL_SCISSOR_TEST);
            dest.clear(juce::Colours::transparentBlack);
            if(hadScissor)
                juce::gl::glEnable(juce::gl::GL_SCISSOR_TEST);
            auto ll = juce::createOpenGLGraphicsContext(context, dest);
            if(!ll) return false;
            shader.onShaderActivated = [&, sourceTex, texW, texH, dx, dy](juce::OpenGLShaderProgram& p)
            {
                GLint old = juce::gl::GL_TEXTURE0;
                juce::gl::glGetIntegerv(juce::gl::GL_ACTIVE_TEXTURE, &old);
                bindTexture(context, 3, sourceTex);
                uniform(p,"tex",3);
                uniform(p,"texSize",(float)texW,(float)texH);
                uniform(p,"imageSize",(float)w,(float)h);
                uniform(p,"direction",dx,dy);
                uniform(p,"sigma",juce::jmax(0.15f,sigma));
                context.extensions.glActiveTexture((GLenum)old);
            };
            bool ok = false;
            {
                juce::Graphics gg(*ll);
                ok = shader.checkCompilation(gg.getInternalContext()).wasOk();
                if(ok)
                    shader.fillRect(gg.getInternalContext(), {0,0,w,h});
            }
            ll.reset();
            return ok;
        };
        const bool horizontalOk = drawPass(r.horizontal, *horizontalShader,
                                            r.raw.getTextureID(),
                                            r.raw.getWidth(), r.raw.getHeight(), 1.0f, 0.0f);
        const bool verticalOk = horizontalOk && drawPass(r.vertical, *verticalShader,
                                             r.horizontal.getTextureID(),
                                             r.horizontal.getWidth(),
                                             r.horizontal.getHeight(), 0.0f, 1.0f);
        restoreFramebuffer();
        if(!verticalOk) return false;
        r.blurSigma = sigma;
        return true;
    }
    static constexpr const char* blurCode = R"GLSL(
        uniform sampler2D tex;
        uniform vec2 imageSize, texSize, direction;
        uniform float sigma;
        void main()
        {
            vec2 px = vec2(pixelPos.x, pixelPos.y);
            vec2 texel = vec2(1.0) / texSize;
            float stride = max(1.0, (3.0 * sigma) / 12.0);
            float sumW = 0.0;
            vec3 sum = vec3(0.0);
            for(int i = -12; i <= 12; ++i)
            {
                float d = float(i) * stride;
                float wt = exp(-0.5 * d * d / (sigma*sigma));
                vec2 at = clamp(px + direction * d, vec2(0.5), imageSize - vec2(0.5));
                vec2 uv = vec2(at.x * texel.x, 1.0 - at.y * texel.y);
                sum += texture2D(tex, uv).rgb * wt;
                sumW += wt;
            }
            gl_FragColor = vec4(sum / sumW, 1.0) * pixelAlpha;
        }
    )GLSL";
    static constexpr const char* compositeCode = R"GLSL(
        uniform sampler2D wallpaper, logoSampler, brandSampler;
        uniform vec2 canvasSize, coverSize, sourceSize, textureSize;
        uniform vec2 logoTexSize, brandTexSize, targetSize;
        uniform vec4 overlayParams, logoRect, brandRect;
        uniform float backgroundDarkenWidth, backgroundScale;
        uniform int boundMode;
        float mapCoord(float p, float n)
        {
            if(boundMode == 1) return mod(mod(p,n)+n,n);
            if(boundMode == 2) {
                float len = max(1.0, 2.0 * (n - 1.0));
                float v = mod(mod(p,len)+len,len);
                return min(v, len-v);
            }
            return clamp(p,0.0,n-1.0);
        }
        vec4 sampleWallpaper(vec2 pos)
        {
            vec2 coverP = (pos - (canvasSize-1.0)*0.5 - overlayParams.yz) / overlayParams.x
                        + (coverSize-1.0)*0.5;
            if(boundMode == 3 && (coverP.x < 0.0 || coverP.y < 0.0
                  || coverP.x > coverSize.x-1.0 || coverP.y > coverSize.y-1.0))
                return vec4(0.0);
            coverP.x = mapCoord(coverP.x,coverSize.x);
            coverP.y = mapCoord(coverP.y,coverSize.y);
            vec2 originalP = (coverP + 0.5) * sourceSize / coverSize;
            vec2 uv = vec2(originalP.x/textureSize.x,1.0-originalP.y/textureSize.y);
            return texture2D(wallpaper, uv);
        }
        vec3 applyLogo(vec3 bg, vec2 point, vec4 rect, int which, vec2 imageSz)
        {
            if(rect.z <= 0.0 || rect.w <= 0.0 || imageSz.x <= 0.0 || imageSz.y <= 0.0
               || point.x < rect.x || point.y < rect.y
               || point.x >= rect.x+rect.z || point.y >= rect.y+rect.w)
                return bg;
            vec2 uv = (point - rect.xy)/rect.zw;
            float fit = min(rect.z/imageSz.x,rect.w/imageSz.y);
            vec2 fitted = imageSz * fit / rect.zw;
            vec2 start = (vec2(1.0)-fitted)*0.5;
            vec2 loc = (uv-start)/fitted;
            if(loc.x >= 0.0 && loc.x <= 1.0 && loc.y >= 0.0 && loc.y <= 1.0)
            {
                vec4 fg = which == 0
                        ? texture2D(logoSampler,vec2(loc.x,1.0-loc.y))
                        : texture2D(brandSampler,vec2(loc.x,1.0-loc.y));
                bg = max(bg, fg.rgb);
            }
            return bg;
        }
        void main()
        {
            vec2 at = pixelPos * canvasSize / targetSize;
            vec3 src = sampleWallpaper(at).rgb;
            vec3 base = vec3(18.0,18.0,22.0)/255.0;
            vec3 rgb = min(vec3(1.0),base + src*overlayParams.w);
            if(at.x < backgroundDarkenWidth)
                rgb *= backgroundScale;
            rgb = applyLogo(rgb,at,logoRect,0,logoTexSize);
            rgb = applyLogo(rgb,at,brandRect,1,brandTexSize);
            gl_FragColor = vec4(rgb*pixelAlpha,pixelAlpha);
        }
    )GLSL";
    std::unique_ptr<juce::OpenGLGraphicsContextCustomShader> horizontalShader;
    std::unique_ptr<juce::OpenGLGraphicsContextCustomShader> verticalShader;
    std::unique_ptr<juce::OpenGLGraphicsContextCustomShader> compositeShader;
};
class FluorescenceGpuSpectrumOverlay
{
public:
    FluorescenceGpuSpectrumOverlay()
        : cacheKey("AlletisGpuSpectrumOverlay_"
                   + juce::String::toHexString((juce::pointer_sized_int)this))
    {
    }
    bool paint(juce::Graphics& g, juce::Rectangle<float> plot,
               juce::Rectangle<float> targetPlot, juce::Point<float> targetSize,
               const FluorescenceOverlayAssets::Snapshot& source,
               const juce::Image& heatStrip)
    {
        auto* context = juce::OpenGLContext::getCurrentContext();
        const auto plotBounds = plot.toNearestInt();
        const int plotW = juce::jmax(1, plotBounds.getWidth());
        const int plotH = juce::jmax(1, plotBounds.getHeight());
        if(context == nullptr || !source.validImage || !source.blurredImage.isValid()
            || !source.settings.enabled || heatStrip.getWidth() <= 0)
            return false;
        auto* cache = static_cast<Resources*>(context->getAssociatedObject(cacheKey.toRawUTF8()));
        if(cache == nullptr)
        {
            cache = new Resources();
            context->setAssociatedObject(cacheKey.toRawUTF8(), cache);
        }
        const auto& raw = source.blurredImage;
        if(cache->rawImage != raw)
        {
            cache->raw.loadImage(flattenToPremultipliedRGB(raw));
            cache->rawImage = raw;
            cache->blurSigma = -1.0f;
        }
        if(cache->raw.getTextureID() == 0)
            return false;
        cache->heat.loadImage(heatStrip);
        const float coverScale = juce::jmax((float)plotW / (float)raw.getWidth(),
                                             (float)plotH / (float)raw.getHeight());
        const int coverW = juce::jmax(plotW, juce::roundToInt((float)raw.getWidth() * coverScale));
        const int coverH = juce::jmax(plotH, juce::roundToInt((float)raw.getHeight() * coverScale));
        const float sigmaCover = juce::jmax(0.0f, source.settings.blur)
                               * (float)juce::jmin(coverW, coverH) / 45.0f;
        const float sigmaSource = sigmaCover / coverScale;
        GLuint overlayTexture = cache->raw.getTextureID();
        int textureW = cache->raw.getWidth(), textureH = cache->raw.getHeight();
        if(sigmaSource >= 0.15f)
        {
            if(!updateBlur(*context, *cache, raw.getWidth(), raw.getHeight(), sigmaSource))
                return false;
            overlayTexture = cache->vertical.getTextureID();
            textureW = cache->vertical.getWidth();
            textureH = cache->vertical.getHeight();
        }
        if(!compositeShader)
            compositeShader = std::make_unique<juce::OpenGLGraphicsContextCustomShader>(compositeCode);
        const float resize = std::isfinite(source.settings.resize) && source.settings.resize > 0.0f
                           ? source.settings.resize : 1.0f;
        const float brightness = juce::jlimit(0.0f, 1.0f,
            source.settings.brightness * source.loadMultiplier);
        const int boundMode = (int)source.settings.boundMode;
        compositeShader->onShaderActivated = [=, &resources = *cache](juce::OpenGLShaderProgram& program)
        {
            auto* activeContext = juce::OpenGLContext::getCurrentContext();
            if(activeContext == nullptr) return;
            GLint oldUnit = juce::gl::GL_TEXTURE0;
            juce::gl::glGetIntegerv(juce::gl::GL_ACTIVE_TEXTURE, &oldUnit);
            bindTexture(*activeContext, 3, overlayTexture);
            bindTexture(*activeContext, 4, resources.heat.getTextureID());
            uniform(program, "overlayTexture", 3);
            uniform(program, "heatTexture", 4);
                uniform(program, "plotSize", (float)plotW, (float)plotH);
                uniform(program, "targetPlot", targetPlot);
                uniform(program, "targetLogicalSize", targetSize.x, targetSize.y);
            uniform(program, "coverSize", (float)coverW, (float)coverH);
            uniform(program, "overlayParams", resize, source.settings.translateX,
                    source.settings.translateY, brightness);
            uniform(program, "boundMode", boundMode);
                GLint viewport[4] {};
                juce::gl::glGetIntegerv(juce::gl::GL_VIEWPORT, viewport);
                uniform(program, "targetPhysicalSize",
                    (float)juce::jmax(1, viewport[2]),
                    (float)juce::jmax(1, viewport[3]));
            activeContext->extensions.glActiveTexture((GLenum)oldUnit);
        };
        if(compositeShader->checkCompilation(g.getInternalContext()).failed())
            return false;
        compositeShader->fillRect(g.getInternalContext(), plotBounds);
        return true;
    }
private:
    struct Resources final : juce::ReferenceCountedObject
    {
        juce::OpenGLTexture raw, heat;
        juce::OpenGLFrameBuffer horizontal, vertical;
        juce::Image rawImage;
        float blurSigma = -1.0f;
        int width = 0, height = 0;
    };
    static juce::Image flattenToPremultipliedRGB(const juce::Image& source)
    {
        if(!source.isValid()) return {};
        juce::Image result(juce::Image::RGB, source.getWidth(), source.getHeight(), true);
        juce::Image::BitmapData src(source, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData dst(result, juce::Image::BitmapData::writeOnly);
        for(int y = 0; y < source.getHeight(); ++y)
            for(int x = 0; x < source.getWidth(); ++x)
            {
                const auto colour = src.getPixelColour(x, y);
                const float alpha = colour.getFloatAlpha();
                dst.setPixelColour(x, y, juce::Colour::fromFloatRGBA(
                    colour.getFloatRed() * alpha, colour.getFloatGreen() * alpha,
                    colour.getFloatBlue() * alpha, 1.0f));
            }
        return result;
    }
    static void bindTexture(juce::OpenGLContext& context, int unit, GLuint id)
    {
        context.extensions.glActiveTexture((GLenum)(juce::gl::GL_TEXTURE0 + unit));
        juce::gl::glBindTexture(juce::gl::GL_TEXTURE_2D, id);
        if(id == 0) return;
        juce::gl::glTexParameteri(juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MIN_FILTER, juce::gl::GL_LINEAR);
        juce::gl::glTexParameteri(juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MAG_FILTER, juce::gl::GL_LINEAR);
        juce::gl::glTexParameteri(juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_WRAP_S, juce::gl::GL_CLAMP_TO_EDGE);
        juce::gl::glTexParameteri(juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_WRAP_T, juce::gl::GL_CLAMP_TO_EDGE);
    }
    static void uniform(juce::OpenGLShaderProgram& p, const char* name, int value)
    { juce::OpenGLShaderProgram::Uniform u(p, name); if(u.uniformID >= 0) u.set(value); }
    static void uniform(juce::OpenGLShaderProgram& p, const char* name, float x, float y)
    { juce::OpenGLShaderProgram::Uniform u(p, name); if(u.uniformID >= 0) u.set((GLfloat)x, (GLfloat)y); }
    static void uniform(juce::OpenGLShaderProgram& p, const char* name, float x, float y, float z, float w)
    { juce::OpenGLShaderProgram::Uniform u(p, name); if(u.uniformID >= 0) u.set((GLfloat)x, (GLfloat)y, (GLfloat)z, (GLfloat)w); }
    static void uniform(juce::OpenGLShaderProgram& p, const char* name, juce::Rectangle<float> value)
    { uniform(p, name, value.getX(), value.getY(), value.getWidth(), value.getHeight()); }
    bool updateBlur(juce::OpenGLContext& context, Resources& resources,
                    int width, int height, float sigma)
    {
        const bool resize = resources.width != width || resources.height != height
                         || !resources.horizontal.isValid() || !resources.vertical.isValid();
        if(resize)
        {
            resources.horizontal.release();
            resources.vertical.release();
            if(!resources.horizontal.initialise(context, width, height)
                || !resources.vertical.initialise(context, width, height))
                return false;
            resources.width = width;
            resources.height = height;
            resources.blurSigma = -1.0f;
        }
        if(std::abs(resources.blurSigma - sigma) < 0.005f)
            return true;
        if(!horizontalShader)
            horizontalShader = std::make_unique<juce::OpenGLGraphicsContextCustomShader>(blurCode);
        if(!verticalShader)
            verticalShader = std::make_unique<juce::OpenGLGraphicsContextCustomShader>(blurCode);
        GLint previousFramebuffer = 0;
        GLint previousViewport[4] {};
        juce::gl::glGetIntegerv(juce::gl::GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
        juce::gl::glGetIntegerv(juce::gl::GL_VIEWPORT, previousViewport);
        const auto restore = [&]
        {
            context.extensions.glBindFramebuffer(juce::gl::GL_FRAMEBUFFER, (GLuint)previousFramebuffer);
            juce::gl::glViewport(previousViewport[0], previousViewport[1],
                                previousViewport[2], previousViewport[3]);
        };
        const auto drawPass = [&](juce::OpenGLFrameBuffer& destination,
                                  juce::OpenGLGraphicsContextCustomShader& shader,
                                  GLuint sourceTexture, int textureW, int textureH,
                                  float directionX, float directionY)
        {
            const bool hadScissor = juce::gl::glIsEnabled(juce::gl::GL_SCISSOR_TEST) != 0;
            if(hadScissor) juce::gl::glDisable(juce::gl::GL_SCISSOR_TEST);
            destination.clear(juce::Colours::transparentBlack);
            if(hadScissor) juce::gl::glEnable(juce::gl::GL_SCISSOR_TEST);
            auto lowLevel = juce::createOpenGLGraphicsContext(context, destination);
            if(!lowLevel) return false;
            shader.onShaderActivated = [&, sourceTexture, textureW, textureH,
                                         directionX, directionY](juce::OpenGLShaderProgram& program)
            {
                GLint oldUnit = juce::gl::GL_TEXTURE0;
                juce::gl::glGetIntegerv(juce::gl::GL_ACTIVE_TEXTURE, &oldUnit);
                bindTexture(context, 3, sourceTexture);
                uniform(program, "tex", 3);
                uniform(program, "texSize", (float)textureW, (float)textureH);
                uniform(program, "imageSize", (float)width, (float)height);
                uniform(program, "direction", directionX, directionY);
                juce::OpenGLShaderProgram::Uniform sigmaUniform(program, "sigma");
                if(sigmaUniform.uniformID >= 0) sigmaUniform.set((GLfloat)juce::jmax(0.15f, sigma));
                context.extensions.glActiveTexture((GLenum)oldUnit);
            };
            bool ok = false;
            {
                juce::Graphics passGraphics(*lowLevel);
                ok = shader.checkCompilation(passGraphics.getInternalContext()).wasOk();
                if(ok) shader.fillRect(passGraphics.getInternalContext(), { 0, 0, width, height });
            }
            lowLevel.reset();
            return ok;
        };
        const bool horizontalOk = drawPass(resources.horizontal, *horizontalShader,
            resources.raw.getTextureID(), resources.raw.getWidth(), resources.raw.getHeight(), 1.0f, 0.0f);
        const bool verticalOk = horizontalOk && drawPass(resources.vertical, *verticalShader,
            resources.horizontal.getTextureID(), resources.horizontal.getWidth(),
            resources.horizontal.getHeight(), 0.0f, 1.0f);
        restore();
        if(!verticalOk) return false;
        resources.blurSigma = sigma;
        return true;
    }
    static constexpr const char* blurCode = R"GLSL(
        uniform sampler2D tex;
        uniform vec2 imageSize, texSize, direction;
        uniform float sigma;
        void main()
        {
            vec2 texel = vec2(1.0) / texSize;
            float stride = max(1.0, (3.0 * sigma) / 12.0);
            float sumW = 0.0;
            vec3 sum = vec3(0.0);
            for(int i = -12; i <= 12; ++i)
            {
                float distance = float(i) * stride;
                float weight = exp(-0.5 * distance * distance / (sigma * sigma));
                vec2 sampleAt = clamp(pixelPos + direction * distance,
                                      vec2(0.5), imageSize - vec2(0.5));
                vec2 uv = vec2(sampleAt.x * texel.x, 1.0 - sampleAt.y * texel.y);
                sum += texture2D(tex, uv).rgb * weight;
                sumW += weight;
            }
            gl_FragColor = vec4(sum / sumW, 1.0) * pixelAlpha;
        }
    )GLSL";
    static constexpr const char* compositeCode = R"GLSL(
        uniform sampler2D overlayTexture, heatTexture;
        uniform vec4 targetPlot, overlayParams;
        uniform vec2 plotSize, coverSize, targetLogicalSize, targetPhysicalSize;
        uniform int boundMode;
        float mapCoord(float value, float length)
        {
            if(boundMode == 1) return mod(mod(value, length) + length, length);
            if(boundMode == 2)
            {
                float period = max(1.0, 2.0 * (length - 1.0));
                float wrapped = mod(mod(value, period) + period, period);
                return min(wrapped, period - wrapped);
            }
            return clamp(value, 0.0, length - 1.0);
        }
        vec3 sampleOverlay(vec2 position)
        {
            vec2 coverPoint = (position - (plotRect.zw - 1.0) * 0.5 - overlayParams.yz)
                            / overlayParams.x + (coverSize - 1.0) * 0.5;
            if(boundMode == 3 && (coverPoint.x < 0.0 || coverPoint.y < 0.0
                || coverPoint.x > coverSize.x - 1.0 || coverPoint.y > coverSize.y - 1.0))
                return vec3(0.0);
            coverPoint.x = mapCoord(coverPoint.x, coverSize.x);
            coverPoint.y = mapCoord(coverPoint.y, coverSize.y);
            vec2 coverUv = (coverPoint + 0.5) / coverSize;
            vec2 uv = vec2(coverUv.x, 1.0 - coverUv.y);
            return texture2D(overlayTexture, uv).rgb;
        }
        void main()
        {
            vec2 targetPoint = pixelPos * targetLogicalSize / targetPhysicalSize;
            vec2 local = (targetPoint - targetPlot.xy) * plotSize / targetPlot.zw;
            float x = clamp((local.x + 0.5) / plotSize.x, 0.0, 1.0);
            vec3 overlay = sampleOverlay(local) * overlayParams.w;
            float heat = texture2D(heatTexture, vec2(x, 0.5)).r * 0.54;
            vec3 rgb = min(vec3(1.0), vec3(10.0, 10.0, 12.0) / 255.0
                                      + overlay + vec3(heat));
            gl_FragColor = vec4(rgb * pixelAlpha, pixelAlpha);
        }
    )GLSL";
    juce::String cacheKey;
    std::unique_ptr<juce::OpenGLGraphicsContextCustomShader> horizontalShader;
    std::unique_ptr<juce::OpenGLGraphicsContextCustomShader> verticalShader;
    std::unique_ptr<juce::OpenGLGraphicsContextCustomShader> compositeShader;
};
#endif