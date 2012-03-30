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

#ifdef PROTEUS_DEVICE_API

#include "config.h"
#include "VNodeLayerAndroid.h"

#include "RenderSkinMediaButton.h"
#include "TilesManager.h"
#include <GLES2/gl2.h>
#include <gui/SurfaceTexture.h>

#include "HTMLVNodeElement.h"
#include <dapi_log.h>

#if USE(ACCELERATED_COMPOSITING)

using namespace android;

namespace WebCore {

VNodeLayerAndroid::VNodeLayerAndroid(VNodeTexture *texture)
  : LayerAndroid((RenderLayer*)0)
  , m_texture(texture)
{
  NODE_LOGD("VNODE_API: VNodeLayerAndroid (%p, vnode:%p, texture:%p, ref:%d))",
      this, m_texture->vnode(), m_texture.get(), m_texture->refCount());
}

VNodeLayerAndroid::VNodeLayerAndroid(const VNodeLayerAndroid& layer)
  : LayerAndroid(layer)
  , m_texture(layer.m_texture)
{
  NODE_LOGV("VNODE_API: VNodeLayerAndroid copy (%p -> %p), (vnode:%p, texture:%p, ref:%d",
      &layer, this, m_texture->vnode(), m_texture.get(), m_texture->refCount());
}

VNodeLayerAndroid::~VNodeLayerAndroid() {
  if (type() == LayerAndroid::WebCoreLayer) {
    NODE_LOGD("VNODE_API: ~VNodeLayerAndroid (%p, webcore, ref:%d)",
        this, m_texture->refCount());
  } else {
    NODE_LOGV("VNODE_API: ~VNodeLayerAndroid (%p, ui, ref:%d)",
        this, m_texture->refCount());
  }
}

// This gets invoked only from UI thread
bool VNodeLayerAndroid::drawGL() {
  // Texture could be cleared from the main thread when the element is destroyed
  MutexLocker lock(m_texture->lock());
  NODE_LOGV("VNODE_API: drawGL (layer:%p, vnode:%p, texture:%p)",
      this, m_texture->vnode(), m_texture.get());

  // FIXME: if the vnode element is destroyed we still get calls from the UI thread
  // we dont draw the frames once the element is removed, this is at best a hack
  if (!m_texture->vnode()) {
    NODE_LOGI("%s [%p], layer is destroyed not drawing", __FUNCTION__, this);
    return false;
  }

  // node source is not available yet or has been detached, dont paint
  // the detached case occurs since the html element gets destroyed later in GC phase
  // but the node source is destroyed as soon as the page is closed
  if (!m_texture->vnode()->attached()) {
    NODE_LOGI("%s [%p], source detached or not available, ignoring draw, vnode (%p)",
        __FUNCTION__, this, m_texture->vnode());
    return false;
  }

  // create/update the source with the texture
  m_texture->updateTextureIfNeeded();

  // paint the contents of the texture to the screen
  SkRect rect = SkRect::MakeSize(getSize());
  m_texture->drawTexture(m_drawTransform, rect);

  return drawChildrenGL();
}

///////////////////////////// VNodeFrameListener /////////////////////////////
VNodeFrameListener::VNodeFrameListener(VNodeTexture* texture)
  : m_texture(texture) {
  NODE_LOGF();
}

VNodeFrameListener::~VNodeFrameListener() {
  NODE_LOGF();
}

void VNodeFrameListener::onFrameAvailable() {
  NODE_LOGV("VNODE_API: onFrameAvailable texture(%p)", m_texture);
  callOnMainThread(VNodeTexture::onFrameAvailableMainThread, m_texture);
}

///////////////////////////////VNodeTexture/////////////////////////////
// VNodeTexture ownership is shared with HTMLVNodeElement and the layers
VNodeTexture::VNodeTexture(HTMLVNodeElement *vnode)
  : m_vnode(vnode)
  , m_textureId(0)
{
  NODE_LOGI("VNODE_API: VNodeTexture (%p)", this);
}

void VNodeTexture::detachVNode() {
  // vnode could be accessed from the UI thread
  MutexLocker lock(m_lock);
  NODE_LOGV("VNODE_API: detachVNode [%p] vnode(%p)", this, m_vnode);
  m_vnode = 0;
}

VNodeTexture::~VNodeTexture() {
  NODE_LOGI("VNODE_API: ~VNodeTexture (%p)", this);
  if (m_surfaceTexture.get()) {
    m_surfaceTexture->setFrameAvailableListener(0);
    glDeleteTextures(1, &m_textureId);
    m_surfaceTexture.clear();
    m_listener.clear();
  }
}

void VNodeTexture::drawTexture(const TransformationMatrix& contentMatrix, SkRect& rect) {
  NODE_LOGV("VNODE_API: drawTexture (texture:%p vnode:%p)", this, m_vnode);
  NODE_ASSERT(m_surfaceTexture.get());

  GLfloat surfaceMatrix[16];
  m_surfaceTexture->updateTexImage();
  m_surfaceTexture->getTransformMatrix(surfaceMatrix);
  TilesManager::instance()->shader()->drawVideoLayerQuad(contentMatrix,
      surfaceMatrix, rect, m_textureId);
}

void VNodeTexture::updateTextureOnMainThread(void *data) {
  NODE_LOGF();

  VNodeTexture *texture = static_cast<VNodeTexture*>(data);
  MutexLocker lock(texture->m_lock);
  // BUGFIX
  // we dont make a blocking call to updateTextureOnMainThread, so by the time
  // we are invoked, the vnode could have been detached (run camera-multiple-preview
  // with a timer less than 250ms), so this assert doesnt hold
  // FIXME - we need to investigate a blocking callOnMainThreadAndWait to avoid this
  // condition (but that api had issues working reliably. e.g. it could deadlock)
  // NODE_ASSERT(texture->vnode() && texture->vnode()->view()->source());
  if (texture->vnode() && texture->vnode()->view()->source()) {
    NODE_LOGD("updateTextureMainThread: vnode(%p), source(%p) texture(%p)", texture->vnode(),
        texture->vnode()->view()->source(), texture->m_surfaceTexture.get());
    texture->vnode()->view()->source()->setPreviewTexture((void *)texture->m_surfaceTexture.get());
  }
  texture->deref();
}

void VNodeTexture::updateTextureIfNeeded() {
  NODE_LOGFT();

  if (!m_surfaceTexture.get()) {
    glGenTextures(1, &m_textureId);
    m_surfaceTexture = new SurfaceTexture(m_textureId);
    m_listener = new VNodeFrameListener(this);
    m_surfaceTexture->setFrameAvailableListener(m_listener);

    // keep texture alive till the main thread consumes it..
    NODE_LOGD("updateTextureIfNeeded: posting message to main thread");
    ref();
    callOnMainThread(updateTextureOnMainThread, this);
  }
}

void VNodeTexture::onFrameAvailableMainThread(void* userData) {
  VNodeTexture* texture = static_cast<VNodeTexture*>(userData);

  // vnode could be cleared from the main thread
  MutexLocker lock(texture->m_lock);
  NODE_LOGV("VNODE_API: invalidate texture(%p) vnode(%p)", texture, texture->vnode());
  if (texture->vnode()) {
    texture->vnode()->invalidate();
  }
}

}
#endif // USE(ACCELERATED_COMPOSITING)
#endif // PROTEUS_DEVICE_API
