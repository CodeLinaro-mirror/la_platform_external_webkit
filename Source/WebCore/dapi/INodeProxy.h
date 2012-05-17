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
#ifndef NODE_PROXY_H
#define NODE_PROXY_H

#ifdef PROTEUS_DEVICE_API

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

#include <dapi.h>
#include <map>

namespace WTF {
  class String;
}

namespace WebCore {

  class Frame;
  class INodeClientAudioImpl;
  class INodeClientCameraImpl;
  class INodeClientWebViewImpl;
  class INodeClientPermissionImpl;
  class INodeClientJavaBridgeImpl;

  /**
   * INodeProxy is the webcore side object for a node instance
   * its a 1-1 mapping to node instance in that page (one per context/frame)
   * Its owned by the DOMWindow and gets destroyed when the DOMWindow is detached
   * from frame
   */
  class INodeProxy: public RefCounted<INodeProxy>, public dapi::INodeClient {
    public:
      static PassRefPtr<INodeProxy> create(Frame *frame){
        return adoptRef(new INodeProxy(frame));
      }

      // releases node resources
      ~INodeProxy();

      // handle app focus in/out events
      void pause();
      void resume();

      // called by the webkit engine when the frame is detached from window
      // we release node resources here
      void disconnectFrame();

      // this the app data path that will be used as the root
      // for installing and looking up node modules
      static void setAppDataPath(const WTF::String&);

      // handles request from node ev thread, will schedule
      // the request on the webcore thread
      static void HandleInvokePending();

      /**
       * This is the core bridge api that exposes services from the webkit to node
       * @interface interface that is being queried
       * @object object that implements the interface, null if no object implements
       * @return true if there is object that implements the interface, false otherwise
       */
      bool queryInterface(dapi::Interface, void** object);

      /**
       * Register implementation for a interface. This is useful to
       * register objects already available that implement the interface.
       * @interface interface this object implements
       * @imp object implementing the interface
       */
      void registerInterface(dapi::Interface interface, void* object);

      // accessors
      Frame *frame() const { return m_frame; }
      dapi::INode *inode() const { return m_inode; }

    private:
      INodeProxy(Frame*);
      Frame *m_frame;
      dapi::INode *m_inode;

      OwnPtr<INodeClientAudioImpl> m_audioImpl;
      OwnPtr<INodeClientCameraImpl> m_cameraImpl;
      OwnPtr<INodeClientPermissionImpl> m_permissionImpl;
      OwnPtr<INodeClientWebViewImpl> m_webviewImpl;
      OwnPtr<INodeClientJavaBridgeImpl> m_javaBridgeImpl;

      std::map<dapi::Interface, void*> m_interfaceMap;

      static WTF::String* s_moduleRootPath;
      static bool s_nodeInitialized;
  };
}

#endif
#endif

