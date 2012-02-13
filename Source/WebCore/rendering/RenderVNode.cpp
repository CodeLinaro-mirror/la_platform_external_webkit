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
#include "RenderVNode.h"
#include "RenderLayer.h"
#include "HTMLVNodeElement.h"

namespace WebCore {

RenderVNode::RenderVNode(HTMLVNodeElement* vnode)
    : RenderImage(vnode)
{
  setImageResource(RenderImageResource::create());
  m_vnodeLayer = new VNodeLayerAndroid(vnode->texture());
  NODE_LOGD("VNODE_API: new RenderVNode (%p), vnode(%p) layer(%p) texture(%p)",
      this, vnode, m_vnodeLayer, vnode->texture());
}

RenderVNode::~RenderVNode() {
  NODE_LOGD("VNODE_API: ~RenderVNode (%p)", __FUNCTION__, this );
  m_vnodeLayer->unref();
}

void RenderVNode::paintIntoRect(GraphicsContext* gc, const IntRect& rect) {
  NODE_LOGF();
}

void RenderVNode::paintReplaced(PaintInfo& paintInfo, int tx, int ty) {
  NODE_LOGF();
  //IntRect rect(tx, ty, contentWidth(), contentHeight());
  //paintIntoRect(paintInfo.context, rect);
  NODE_ASSERT(layer());
  layer()->contentChanged(RenderLayer::ImageChanged);
}

bool RenderVNode::requiresLayer() const {
  NODE_LOGF();
  return true;
}

PlatformLayer* RenderVNode::platformLayer() const {
  NODE_LOGF();
  return m_vnodeLayer;
}

void RenderVNode::layout(){
  NODE_LOGF();
  RenderImage::layout();
  IntSize size = contentBoxRect().size();
  NODE_LOGI("VNODE_API: layout (%d, %d)", size.width(), size.height());
}

} // namespace WebCore

#endif // PROTEUS_DEVICE_API

