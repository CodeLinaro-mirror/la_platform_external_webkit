/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
#include "MainThread.h"
#include "text/CString.h"
#include "Page.h"
#include "Chrome.h"
#include "WebViewCore.h"
#include "Navigator.h"
#include "NodeProxy.h"
#include <vector>

#include "HTMLNames.h"
#include "V8HTMLVNodeElement.h"
#include "V8ArrayBuffer.h"

namespace WebCore {
using namespace android;
using namespace v8;
using WTF::String;

String* NodeProxy::s_moduleRootPath = 0;
bool NodeProxy::s_nodeInitialized = false;

void NodeProxy::handleRequestPermission(node::NodeEvent *ev) {
  NODE_ASSERT(ev->type == node::NODE_EVENT_FP_REQUEST_PERMISSION);
  node::RequestPermissionEvent *e = &ev->u.RequestPermissionEvent_;

  Vector<String> featurelist;
  void* efeatures = e->features;
  Handle<v8::Value> featureValue = *(Handle<v8::Value>*)efeatures;
  Handle<Array> v8Features  = Handle<Array>::Cast(featureValue);
  for(unsigned int i=0; i< v8Features->Length(); i++) {
    v8::String::AsciiValue  featureName(v8Features->Get(i)->ToString());
    featurelist.append(*featureName);
  }

  if(m_frame->page()) {
      m_frame->page()->chrome()->handleRequestPermission(m_frame, featurelist, e->context, e->callback);
  }

  return;
}

NodeProxy::NodeProxy(Frame* frame)
  : m_frame(frame) {
  NODE_LOGF();
  NODE_ASSERT(frame);

  // Initialize the node event system on demand
  if (!s_nodeInitialized) {
    NODE_ASSERT(s_moduleRootPath);
    NODE_LOGI("%s, rootModulePath(%s)", __FUNCTION__, s_moduleRootPath->latin1().data());
    node::Node::Initialize(HandleInvokePending, true, s_moduleRootPath->latin1().data());
    s_nodeInitialized = true;
  }

  // load the node instance
  NODE_LOGI("%s(tid=%d), frame(%p)", __FUNCTION__, gettid(), m_frame);
  m_node = new node::Node(this);
}

const char* NodeProxy::GetEnvironmentProperty(const char *type, bool privateType){
  WebViewCore* webViewCore = android::WebViewCore::getWebViewCore(m_frame->view());
  static std::string path;
  path = webViewCore->getEnvironmentProperty(type, privateType);
  NODE_LOGV("%s Environment property %s", __FUNCTION__ , path.c_str());
  return path.c_str();
}

const char* NodeProxy::url() {
  NODE_ASSERT(m_frame);
  static std::string url;
  url = m_frame->document()->url().string().latin1().data();
  NODE_LOGV("%s, URL for the NodeProxy %p, frame %p: %s", __FUNCTION__, this, m_frame, url.c_str());
  return url.c_str();
}

class InvokePendingTimer : public TimerBase {
  public:
    static void handleOnMainThread(void* data) {
      InvokePendingTimer *timer = static_cast<InvokePendingTimer*>(data);
      timer->startOneShot(0.01); // 10ms
    }

    virtual void fired() {
      node::Node::InvokePending();
      delete this;
    }
};

void NodeProxy::HandleNodeEvent(node::NodeEvent *ev) {
  NODE_LOGI("Node Bridge event type: %d", ev->type);
  NODE_ASSERT(isMainThread()); // should be called only on the main thread
  switch (ev->type) {
    case node::NODE_EVENT_FP_REQUEST_PERMISSION:
      handleRequestPermission(ev);
      break;

    default:
      NODE_ASSERT(0);
      break;
  }
}

void NodeProxy::HandleInvokePending() {
  NODE_LOGF();

  NODE_ASSERT(!isMainThread());
  InvokePendingTimer *timer = new InvokePendingTimer;
  callOnMainThread(InvokePendingTimer::handleOnMainThread, timer);
}

void NodeProxy::OnTestDone() {
  NODE_LOGF();
}

void NodeProxy::OnDelete() {
  NODE_LOGF();

  // FIXME: we should reset loadModule stub here..
  m_node = 0;
}

NodeProxy::~NodeProxy() {
  NODE_LOGF();
  disconnectFrame();
}

// Tie the lifecycle of node instance to the page
void NodeProxy::disconnectFrame() {
  NODE_LOGF();
  m_frame = 0;
  if (m_node) {
    delete m_node;
    m_node = 0;
  }
}

// Handle browser focusin/out events
void NodeProxy::pause() {
  NODE_LOGF();
  m_node->Pause();
}

// Handle browser focusin/out events
void NodeProxy::resume() {
  NODE_LOGF();
  m_node->Resume();
}

// This is the app path on the device to install app specific files
// node modules will be installed in a sub directory under this
// so each app gets its own set of node modules
void NodeProxy::setAppDataPath(const String& dataPath) {
  NODE_LOGI("%s, module path: %s", __FUNCTION__, dataPath.latin1().data());
  s_moduleRootPath = new String(dataPath);
}

node::NodeView* NodeProxy::Unwrap(Handle<Object> v8VNodeObject) {
  NODE_LOGF();

  HTMLVNodeElement* vnode = V8HTMLVNodeElement::toNative(v8VNodeObject);
  return static_cast<node::NodeView*>(vnode);
}

v8::Handle<v8::Value> NodeProxy::CreatePreviewNode(node::NodeSource *source) {
  NODE_LOGF();

  // use caller scope, create the element in browser context..
  v8::Local<v8::Context> context = V8Proxy::mainWorldContext(m_frame);
  v8::Context::Scope scope(context);

  // create the element in browser context..
  PassRefPtr<HTMLVNodeElement> vnode =
    HTMLVNodeElement::create(HTMLNames::vnodeTag, m_frame->document(), source);

  // wrap in v8 object and return the reference, the vnode is owned by the js handle
  return toV8(vnode);
}

v8::Handle<v8::Value> NodeProxy::CreateArrayBuffer(void *buf, int size) {
  NODE_LOGF();

  // use caller scope, create the element in browser context..
  v8::Local<v8::Context> context = V8Proxy::mainWorldContext(m_frame);
  v8::Context::Scope scope(context);

  PassRefPtr<ArrayBuffer> arrayBuffer = ArrayBuffer::create(buf, size);
  return toV8(arrayBuffer);
}

}

#endif
