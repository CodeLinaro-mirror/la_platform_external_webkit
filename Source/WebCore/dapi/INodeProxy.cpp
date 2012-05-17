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
#include "Frame.h"
#include "text/CString.h"
#include "INodeProxy.h"
#include "INodeClientAudio.h"
#include "INodeClientCamera.h"
#include "INodeClientPermission.h"
#include "INodeClientWebView.h"
#include "INodeClientJavaBridge.h"

namespace WebCore {
using namespace v8;
using namespace dapi;

WTF::String* INodeProxy::s_moduleRootPath = 0;
bool INodeProxy::s_nodeInitialized = false;

INodeProxy::INodeProxy(Frame* frame)
  : m_frame(frame) {
  NODE_LOGF();
  NODE_ASSERT(frame);

  // Initialize the node event system on demand
  if (!s_nodeInitialized) {
    NODE_ASSERT(s_moduleRootPath);
    NODE_LOGI("%s, rootModulePath(%s)", __FUNCTION__, s_moduleRootPath->latin1().data());
    INode::initialize(HandleInvokePending, true, s_moduleRootPath->latin1().data());
    s_nodeInitialized = true;
  }

  // Create all the client objects
  m_audioImpl = new INodeClientAudioImpl(this);
  m_cameraImpl = new INodeClientCameraImpl(this);
  m_permissionImpl = new INodeClientPermissionImpl(this);
  m_webviewImpl = new INodeClientWebViewImpl(this);
  m_javaBridgeImpl = new INodeClientJavaBridgeImpl(this);

  // load the node instance
  m_inode = new INode(this);
  NODE_LOGI("INodeProxy(), frame(%p), nodeproxy(%p), inode(%p)", m_frame, this, m_inode);
}

class InvokePendingTimer : public TimerBase {
  public:
    static void handleOnMainThread(void* data) {
      InvokePendingTimer *timer = static_cast<InvokePendingTimer*>(data);
      timer->startOneShot(0.01); // 10ms
    }

    virtual void fired() {
      INode::invokePending();
      delete this;
    }
};

void INodeProxy::registerInterface(Interface interface, void* object) {
  m_interfaceMap[interface] = object;
}

bool INodeProxy::queryInterface(Interface interface, void** object) {
  *object = 0;

  // check the interface map
  map<Interface, void*>::iterator it = m_interfaceMap.find(interface);
  if (it != m_interfaceMap.end()) {
    *object = it->second;
    return true;
  }

  // Events used in offtarget currently
  if (interface == INTERFACE_EVENTS) {
    return false;
  }

  // FIXME: Implement delegates if required
  NODE_LOGE("NodeClient(webkit) interface %d not implemented", interface);
  NODE_ASSERT_REACHABLE();
  return false;
}

void INodeProxy::HandleInvokePending() {
  NODE_LOGF();

  NODE_ASSERT(!isMainThread());
  InvokePendingTimer *timer = new InvokePendingTimer;
  callOnMainThread(InvokePendingTimer::handleOnMainThread, timer);
}

INodeProxy::~INodeProxy() {
  NODE_LOGF();
  disconnectFrame();
}

// Tie the lifecycle of node instance to the page
void INodeProxy::disconnectFrame() {
  NODE_LOGF();
  if (m_inode) {
    delete m_inode;
    m_inode = 0;
  }

  // clear the frame after we destroy the node, this
  // enables exit handlers to callback in to proxy
  m_frame = 0;
}

// Handle browser focusin/out events
void INodeProxy::pause() {
  NODE_LOGF();

  INodeEvents *events;
  inode()->queryInterface(INTERFACE_EVENTS, (void **)&events);
  events->onPause();
}

// Handle browser focusin/out events
void INodeProxy::resume() {
  NODE_LOGF();

  INodeEvents *events;
  inode()->queryInterface(INTERFACE_EVENTS, (void **)&events);
  events->onResume();
}

// This is the app path on the device to install app specific files
// node modules will be installed in a sub directory under this
// so each app gets its own set of node modules
void INodeProxy::setAppDataPath(const WTF::String& dataPath) {
  NODE_LOGI("%s, module path: %s", __FUNCTION__, dataPath.latin1().data());
  s_moduleRootPath = new WTF::String(dataPath);
}


}

#endif
