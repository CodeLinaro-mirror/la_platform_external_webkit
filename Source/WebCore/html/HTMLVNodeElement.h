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

#ifndef HTMLVNodeElement_h
#define HTMLVNodeElement_h

#ifdef PROTEUS_DEVICE_API

#include "HTMLElement.h"
#include <dapi.h>

namespace WebCore {

class VNodeTexture;

class INodeClientCameraViewImpl : public dapi::INodeClientCameraView {
  public:
    INodeClientCameraViewImpl(dapi::INodeCameraSource *source) : m_source(source) {}
    dapi::INodeCameraSource* source() { return m_source; }
    void detach() { m_source = 0; }

  private:
    dapi::INodeCameraSource *m_source;
};

class HTMLVNodeElement : public HTMLElement {
  public:
    static PassRefPtr<HTMLVNodeElement> create(const QualifiedName&, Document*);
    static PassRefPtr<HTMLVNodeElement> create(const QualifiedName&, Document*, dapi::INodeCameraSource*);
    HTMLVNodeElement(const QualifiedName& tagName, Document*, dapi::INodeCameraSource*);
    virtual ~HTMLVNodeElement();

    virtual bool mapToEntry(const QualifiedName&, MappedAttributeEntry&) const;
    virtual void parseMappedAttribute(Attribute*);
    virtual void attach();
    virtual void detach();
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    virtual void insertedIntoDocument();
    virtual bool isURLAttribute(Attribute*) const;
    virtual const QualifiedName& imageSourceAttributeName() const;
    String altText() const;

    unsigned width() const;
    void setWidth(unsigned);
    unsigned height() const;

    void setHeight(unsigned);
    void invalidate();

    VNodeTexture* texture();
    bool attached() { return m_attached; }
    INodeClientCameraViewImpl* view() { return m_view; }

  private:
    INodeClientCameraViewImpl *m_view;
    RefPtr<VNodeTexture> m_texture;
    bool m_attached;
};

}

#endif

#endif
