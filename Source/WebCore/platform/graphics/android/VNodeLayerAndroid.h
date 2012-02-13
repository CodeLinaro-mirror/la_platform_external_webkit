/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef VNodeLayerAndroid_h
#define VNodeLayerAndroid_h

#ifdef PROTEUS_DEVICE_API

#if USE(ACCELERATED_COMPOSITING)

#include "GLUtils.h"
#include "LayerAndroid.h"
#include "Vector.h"
#include <GLES2/gl2.h>
#include <gui/SurfaceTexture.h>
#include <jni.h>

namespace WebCore {

class HTMLVNodeElement;
class VNodeLayerAndroid;
class VNodeTexture;

class VNodeFrameListener : public android::SurfaceTexture::FrameAvailableListener {
  public:
    VNodeFrameListener(VNodeTexture *);
    virtual ~VNodeFrameListener();
    virtual void onFrameAvailable();

  private:
    VNodeTexture* m_texture;
};

class VNodeLayerAndroid : public LayerAndroid {
  public:
    VNodeLayerAndroid(VNodeTexture* texture);
    VNodeLayerAndroid(const VNodeLayerAndroid& layer);
    virtual ~VNodeLayerAndroid();

    virtual LayerAndroid* copy() const { return new VNodeLayerAndroid(*this); }
    virtual bool drawGL();

  private:
    void createTextureIfNeeded();
    RefPtr<VNodeTexture> m_texture;
};

using WTF::Mutex;

class VNodeTexture : public RefCounted<VNodeTexture> {
  public:
    VNodeTexture(HTMLVNodeElement *vnode);
    virtual ~VNodeTexture();
    void drawTexture(const TransformationMatrix& contentMatrix, SkRect& rect);
    void updateTextureIfNeeded();
    void detachVNode();
    Mutex& lock() { return m_lock; }
    HTMLVNodeElement* vnode() { return m_vnode; }
    android::SurfaceTexture* surfaceTexture() { return m_surfaceTexture.get(); }
    static void onFrameAvailableMainThread(void*);
    static void updateTextureOnMainThread(void *data);

  private:
    HTMLVNodeElement *m_vnode;
    GLuint m_textureId;
    android::sp<android::SurfaceTexture> m_surfaceTexture;
    android::sp<VNodeFrameListener> m_listener;
    Mutex m_lock;
};

using WTF::Vector;

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif // PROTEUS_DEVICE_API

#endif // VNodeLayerAndroid_h
