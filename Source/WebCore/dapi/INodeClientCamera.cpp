/*
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#include "config.h"
#include "Frame.h"
#include "V8HTMLVNodeElement.h"
#include "V8ArrayBuffer.h"
#include "HTMLNames.h"
#include "INodeProxy.h"
#include "INodeClientCamera.h"

namespace WebCore {

INodeClientCameraImpl::INodeClientCameraImpl(INodeProxy *proxy)
  : m_proxy(proxy) {
    m_proxy->registerInterface(dapi::INTERFACE_CAMERA, this);
}

v8::Handle<v8::Value> INodeClientCameraImpl::createPreviewNode(dapi::INodeCameraSource *source) {
  NODE_LOGF();

  // use caller scope, create the element in browser context..
  v8::Local<v8::Context> context = V8Proxy::mainWorldContext(m_proxy->frame());
  v8::Context::Scope scope(context);

  // create the element in browser context..
  PassRefPtr<HTMLVNodeElement> vnode =
    HTMLVNodeElement::create(HTMLNames::vnodeTag, m_proxy->frame()->document(), source);

  // wrap in v8 object and return the reference, the vnode is owned by the js handle
  return toV8(vnode);
}

v8::Handle<v8::Value> INodeClientCameraImpl::createArrayBuffer(void *buf, int size) {
  NODE_LOGF();

  // use caller scope, create the element in browser context..
  v8::Local<v8::Context> context = V8Proxy::mainWorldContext(m_proxy->frame());
  v8::Context::Scope scope(context);

  PassRefPtr<ArrayBuffer> arrayBuffer = ArrayBuffer::create(buf, size);
  return toV8(arrayBuffer);
}

}
