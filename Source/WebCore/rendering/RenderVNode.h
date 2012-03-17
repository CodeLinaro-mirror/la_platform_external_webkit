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

#ifndef RenderVNode_h
#define RenderVNode_h

#ifdef PROTEUS_DEVICE_API

#include "RenderImage.h"
#include "GraphicsContext.h"
#include "PlatformGraphicsContext.h"
#include <SkCanvas.h> // for paint
#include <SkPixelRef.h> // for paint
#include <wtf/MainThread.h>
#include "VNodeLayerAndroid.h"

#include "GraphicsLayer.h"

// dapi
#include "dapi_log.h"

namespace WebCore {

class HTMLVNodeElement;
class RenderVNode : public RenderImage {
  public:
    RenderVNode(HTMLVNodeElement*);
    virtual ~RenderVNode();
    virtual const char* renderName() const { return "RenderVNode"; }
    virtual bool requiresLayer() const;
    virtual void paintReplaced(PaintInfo& paintInfo, int tx, int ty);
    virtual void layout();
    virtual bool isVNode() const { return true; }
    void paintIntoRect(GraphicsContext* gc, const IntRect& rect);

    PlatformLayer* platformLayer() const;

  private:
    VNodeLayerAndroid *m_vnodeLayer;
};

inline RenderVNode* toRenderVNode(RenderObject* object) {
    NODE_ASSERT(!object || object->isVNode());
    return static_cast<RenderVNode*>(object);
}

} // namespace WebCore

#endif // PROTEUS_DEVICE_API
#endif // RenderVNode_h
