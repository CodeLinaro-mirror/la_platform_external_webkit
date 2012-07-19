/*
 * Copyright 2011 The Android Open Source Project
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "VideoLayerAndroid.h"

#include "RenderSkinMediaButton.h"
#include "TilesManager.h"
#include <GLES2/gl2.h>
#include <gui/SurfaceTexture.h>

#if USE(ACCELERATED_COMPOSITING)

#ifdef DEBUG
#include <cutils/log.h>
#include <wtf/text/CString.h>

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "VideoLayerAndroid", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

namespace WebCore {

GLuint VideoLayerAndroid::m_spinnerOuterTextureId = 0;
GLuint VideoLayerAndroid::m_spinnerInnerTextureId = 0;
GLuint VideoLayerAndroid::m_posterTextureId = 0;
GLuint VideoLayerAndroid::m_backgroundTextureId = 0;
bool VideoLayerAndroid::m_createdTexture = false;

double VideoLayerAndroid::m_rotateDegree = 0;

const IntRect VideoLayerAndroid::buttonRect(0, 0, IMAGESIZE, IMAGESIZE);

android::Mutex videoLayerObserverLock;

VideoLayerAndroid::VideoLayerAndroid()
    : LayerAndroid((RenderLayer*)0)
    , m_playerState(INITIALIZED)
    , m_observer(NULL)
{
}

VideoLayerAndroid::VideoLayerAndroid(const VideoLayerAndroid& layer)
    : LayerAndroid(layer)
    , m_observer(NULL)
{
    // m_surfaceTexture is only useful on UI thread, no need to copy.
    // And it will be set at setBaseLayer timeframe
    m_playerState = layer.m_playerState;
}

VideoLayerAndroid::~VideoLayerAndroid()
{
    android::Mutex::Autolock lock(videoLayerObserverLock);
    SkSafeUnref(m_observer);
}

void VideoLayerAndroid::setPlayerState(PlayerState state) {
    m_playerState = state;
}

// We can use this function to set the Layer to point to surface texture.
void VideoLayerAndroid::setSurfaceTexture(sp<SurfaceTexture> texture,
                                          int textureName, PlayerState playerState)
{
    m_surfaceTexture = texture;
    m_playerState = playerState;
    XLOG("[%08x] setSurfaceTexture layerId %d textureName %d playerstate %d", this,
            uniqueId(), textureName, m_playerState);
}

void VideoLayerAndroid::registerVideoLayerObserver(VideoLayerObserverInterface* observer)
{
    android::Mutex::Autolock lock(videoLayerObserverLock);
    if (m_observer != observer)
        SkRefCnt_SafeAssign(m_observer, observer);
}

GLuint VideoLayerAndroid::createSpinnerInnerTexture()
{
    return createTextureFromImage(RenderSkinMediaButton::SPINNER_INNER);
}

GLuint VideoLayerAndroid::createSpinnerOuterTexture()
{
    return createTextureFromImage(RenderSkinMediaButton::SPINNER_OUTER);
}

GLuint VideoLayerAndroid::createPosterTexture()
{
    return createTextureFromImage(RenderSkinMediaButton::VIDEO);
}

GLuint VideoLayerAndroid::createTextureFromImage(int buttonType)
{
    SkRect rect = SkRect(buttonRect);
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, rect.width(), rect.height());
    bitmap.allocPixels();
    bitmap.eraseColor(0);

    SkCanvas canvas(bitmap);
    canvas.drawARGB(0, 0, 0, 0, SkXfermode::kClear_Mode);
    RenderSkinMediaButton::Draw(&canvas, buttonRect, buttonType, true);

    GLuint texture;
    glGenTextures(1, &texture);

    GLUtils::createTextureWithBitmap(texture, bitmap);
    bitmap.reset();
    return texture;
}

GLuint VideoLayerAndroid::createBackgroundTexture()
{
    GLuint texture;
    glGenTextures(1, &texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    GLubyte pixels[4 *3] = {
        128, 128, 128,
        128, 128, 128,
        128, 128, 128,
        128, 128, 128
    };
    glBindTexture(GL_TEXTURE_2D, texture);
    GLUtils::checkGlError("glBindTexture");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    GLUtils::checkGlError("glTexImage2D");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return texture;
}

void VideoLayerAndroid::showProgressSpinner(SkRect& innerRect)
{
     // Show the progressing animation, with two rotating circles
     TransformationMatrix addReverseRotation;
     TransformationMatrix addRotation = m_drawTransform;
     addRotation.translate(innerRect.fLeft, innerRect.fTop);
     addRotation.translate(IMAGESIZE / 2, IMAGESIZE / 2);
     addReverseRotation = addRotation;
     addRotation.rotate(m_rotateDegree);
     addRotation.translate(-IMAGESIZE / 2, -IMAGESIZE / 2);

     SkRect size = SkRect::MakeWH(innerRect.width(), innerRect.height());
     TilesManager::instance()->shader()->drawLayerQuad(addRotation, size,
                                                       m_spinnerOuterTextureId,
                                                       1, true);

     addReverseRotation.rotate(-m_rotateDegree);
     addReverseRotation.translate(-IMAGESIZE / 2, -IMAGESIZE / 2);
     TilesManager::instance()->shader()->drawLayerQuad(addReverseRotation, size,
                                                       m_spinnerInnerTextureId,
                                                       1, true);

     m_rotateDegree += ROTATESTEP;
}

// Calculate the innerRect bounds centered inside the boundsRect
static bool calculateInnerRectBounds(const SkRect& boundsRect, SkRect& innerRect) {
    // Only draw the poster or spinner icon if the video bounds can contain it
    if (boundsRect.contains(innerRect)) {
        innerRect.offset((boundsRect.width() - innerRect.width()) / 2 , (boundsRect.height() - innerRect.height()) / 2);
        return true;
    }
    return false;
}

bool VideoLayerAndroid::drawGL()
{
    // Lazily allocated the textures.
    if (!m_createdTexture) {
        m_backgroundTextureId = createBackgroundTexture();
        m_spinnerOuterTextureId = createSpinnerOuterTexture();
        m_spinnerInnerTextureId = createSpinnerInnerTexture();
        m_posterTextureId = createPosterTexture();
        m_createdTexture = true;
    }

    SkRect rect = SkRect::MakeSize(getSize());

    if (((m_playerState == PREPARED) || (m_playerState == PLAYING) || (m_playerState == BUFFERING)) && m_surfaceTexture.get()) {
        // Show the real video.
        GLfloat surfaceMatrix[surfaceMatrixSize];
        m_surfaceTexture->updateTexImage();
        m_surfaceTexture->getTransformMatrix(surfaceMatrix);
        GLuint textureId =
            TilesManager::instance()->videoLayerManager()->getTextureId(uniqueId());

        if (textureId) {
            TilesManager::instance()->shader()->drawVideoLayerQuad(m_drawTransform,
                                                                   surfaceMatrix,
                                                                   rect, textureId);
            if (m_playerState == BUFFERING) {
                SkRect innerRect = SkRect(buttonRect);
                if (calculateInnerRectBounds(rect, innerRect)) {
                    // Show the spinner on top of the video texture
                    showProgressSpinner(innerRect);
                }
            }

            TilesManager::instance()->videoLayerManager()->updateMatrix(uniqueId(),
                                                                        surfaceMatrix);
        } else {
            // This can happen if the video texture is freed by the VideoLayerManager
            // when the video memory usage exceeds the maximum specified.
            // See VideoLayerManager::updateVideoLayerSize()
            XLOG("Warning: VideoLayerAndroid with layerId %d has lost its GL texture", uniqueId());
        }
    } else {
        GLuint textureId =
            TilesManager::instance()->videoLayerManager()->getTextureId(uniqueId());
        GLfloat* matrix =
            TilesManager::instance()->videoLayerManager()->getMatrix(uniqueId());
        if (textureId && matrix) {
            // Show the screen shot for each video.
            TilesManager::instance()->shader()->drawVideoLayerQuad(m_drawTransform,
                                                               matrix,
                                                               rect, textureId);
        } else {
            SkRect innerRect = SkRect(buttonRect);
            if (calculateInnerRectBounds(rect, innerRect)) {
                // Show the static poster b/c there is no screen shot available.
                TilesManager::instance()->shader()->drawLayerQuad(m_drawTransform, rect,
                                                                  m_backgroundTextureId,
                                                                  1, true);
                if (m_playerState != PREPARING) {
                    TilesManager::instance()->shader()->drawLayerQuad(m_drawTransform, innerRect,
                                                                 m_posterTextureId,
                                                                 1, true);
                }
            }
        }

        // Overlay the progress spinner over the video or the default background
        if (m_playerState == PREPARING) {
            SkRect innerRect = SkRect(buttonRect);
            if (calculateInnerRectBounds(rect, innerRect)) {
                // Show spinner while preparing
                showProgressSpinner(innerRect);
            }
        }
    }

    if (m_observer) {
        IntSize size(rect.width(), rect.height());
        m_observer->notifyRectChange(TilesManager::instance()->shader()->rectInScreenCoord(m_drawTransform, size));
    }

    return drawChildrenGL();
}

}
#endif // USE(ACCELERATED_COMPOSITING)
