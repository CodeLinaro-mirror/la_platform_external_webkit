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

#include "HTMLVNodeElement.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "HTMLElement.h"
#include "RenderVNode.h"
#include "HTMLNames.h"
#include "V8HTMLVNodeElement.h"
#include "VNodeLayerAndroid.h"

namespace WebCore {

using namespace HTMLNames;

HTMLVNodeElement::HTMLVNodeElement(const QualifiedName& tagName, Document* doc, dapi::INodeCameraSource *source)
  : HTMLElement(tagName, doc)
  , m_attached(false)
{
  m_view = new INodeClientCameraViewImpl(source);
  m_texture = adoptRef(new VNodeTexture(this));
  NODE_LOGD("VNODE_API: +HTMLVNodeElement (%p)", this, m_texture.get());
}

PassRefPtr<HTMLVNodeElement> HTMLVNodeElement::create(const QualifiedName& tagName, Document* document) {
  // FIXME: we should remove this constructor, however boilerplate depends on this
  NODE_LOGW("HTMLVNodeElement::create, creation from webpage unsupported");
  return 0;
}

PassRefPtr<HTMLVNodeElement> HTMLVNodeElement::create(const QualifiedName& tagName, Document* document, dapi::INodeCameraSource *source) {
  return adoptRef(new HTMLVNodeElement(tagName, document, source));
}

HTMLVNodeElement::~HTMLVNodeElement() {
  NODE_LOGD("VNODE_API: ~HTMLVNodeElement (%p)", this);
	delete m_view;
  m_texture->detachVNode();
}

bool HTMLVNodeElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const {
  NODE_LOGF();
  if (attrName == HTMLNames::widthAttr ||
      attrName == HTMLNames::heightAttr ) {
    result = eUniversal;
    return false;
  }

  if (attrName == HTMLNames::alignAttr) {
    result = eReplaced;
    return false;
  }

  return HTMLElement::mapToEntry(attrName, result);
}

void HTMLVNodeElement::parseMappedAttribute(Attribute* attr) {
  NODE_LOGF();
  const QualifiedName& attrName = attr->name();

  if (attrName == HTMLNames::widthAttr)
    addCSSLength(attr, CSSPropertyWidth, attr->value());
  else if (attrName == HTMLNames::heightAttr)
    addCSSLength(attr, CSSPropertyHeight, attr->value());
  else
    HTMLElement::parseMappedAttribute(attr);
}

void HTMLVNodeElement::attach() {
  NODE_LOGD("VNODE_API: attach (%p)", this);
  HTMLElement::attach(); // FIXME: shouldnt it be image element

  if (renderer() && m_view->source()) {
    NODE_LOGD("VNODE_API: Attach (view=%p, source=%p, renderer=%p, surfacetexture=%p)",
        this, m_view->source(), renderer(), m_texture->surfaceTexture());
    {
      MutexLocker lock(m_texture->lock());
      m_view->source()->attach(m_view, static_cast<void*>(m_texture->surfaceTexture()));
      m_attached = true;
    }
  } else {
    NODE_LOGW("attach ignored (view=%p, source=%p, renderer=%p)", this, m_view->source(), renderer());
  }
}

void HTMLVNodeElement::detach() {
  NODE_LOGD("VNODE_API: detach (%p)", this);

  // FIXME: we shouldnt be getting detach calls with no renderer
  if (renderer() && m_view->source()) {
    NODE_LOGD("VNODE_API: Detach (view=%p, source=%p)", this, m_view->source());
    {
      MutexLocker lock(m_texture->lock());
      m_view->source()->detach();
      m_attached = false;
    }
  } else {
    NODE_LOGW("VNODE_API: ignoring detach renderer(%p), source(%p)", renderer(), m_view->source());
  }

  HTMLElement::detach();
}

RenderObject* HTMLVNodeElement::createRenderer(RenderArena* arena, RenderStyle*) {
  RenderObject *r = new (arena) RenderVNode(this);
  NODE_LOGD("VNODE_API: createRenderer (vnode:%p, renderer:%p)", this, r);
  return r;
}

void HTMLVNodeElement::insertedIntoDocument() {
  NODE_LOGD("VNODE_API: insertedIntoDocument (%p)", this);
  HTMLElement::insertedIntoDocument();
}

bool HTMLVNodeElement::isURLAttribute(Attribute* attr) const {
  return attr->name() == HTMLNames::srcAttr;
}

const QualifiedName& HTMLVNodeElement::imageSourceAttributeName() const {
  return HTMLNames::srcAttr;
}

String HTMLVNodeElement::altText() const {
  return "vnode";
}

unsigned HTMLVNodeElement::width() const {
  bool ok;
  unsigned w = getAttribute(widthAttr).string().toUInt(&ok);
  return ok ? w : 0;
}

void HTMLVNodeElement::setWidth(unsigned value) {
  NODE_LOGF();
  setAttribute(widthAttr, String::number(value));
}

unsigned HTMLVNodeElement::height() const {
  bool ok;
  unsigned h = getAttribute(heightAttr).string().toUInt(&ok);
  return ok ? h : 0;
}

void HTMLVNodeElement::setHeight(unsigned value) {
  setAttribute(heightAttr, String::number(value));
}

void HTMLVNodeElement::invalidate() {
  // we may not have renderer if the element is not attached to the DOM..
  if (renderer()) {
    NODE_LOGV("VNODE_API: repaint (vnode:%p renderer:%p)", this, renderer());
    renderer()->repaint();
  }
}

VNodeTexture* HTMLVNodeElement::texture() {
  return m_texture.get();
}

}

#endif
