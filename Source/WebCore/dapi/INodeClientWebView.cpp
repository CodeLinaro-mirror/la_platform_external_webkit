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
#include "Page.h"
#include "Chrome.h"
#include "WebViewCore.h"
#include "text/CString.h"
#include "INodeProxy.h"
#include "INodeClientWebView.h"

namespace WebCore {

INodeClientWebViewImpl::INodeClientWebViewImpl(INodeProxy *proxy)
  : m_proxy(proxy) {
    m_proxy->registerInterface(dapi::INTERFACE_WEBVIEW, this);
}

const char* INodeClientWebViewImpl::getEnvironmentProperty(const char *type, bool privateType) {
  NODE_LOGV("%s, type: %s", __FUNCTION__, type);
  android::WebViewCore* webViewCore =
  android::WebViewCore::getWebViewCore(m_proxy->frame()->view());

  static std::string env;
  env = webViewCore->getEnvironmentProperty(type, privateType);
  return env.c_str();
}

const char* INodeClientWebViewImpl::url() {
  NODE_ASSERT(m_proxy->frame());

  static std::string url;
  url = m_proxy->frame()->document()->url().string().latin1().data();
  NODE_LOGV("%s, URL for the INodeProxy %p, frame %p: %s", __FUNCTION__, this,
  m_proxy->frame(), url.c_str());
  return url.c_str();
}

void INodeClientWebViewImpl::setScreenOrientationLock(const char *orientation) {
  NODE_LOGV("%s, type: %s", __FUNCTION__, orientation);
  if(m_proxy->frame()->view()) {
    android::WebViewCore* webViewCore = android::WebViewCore::getWebViewCore(m_proxy->frame()->view());
    webViewCore->setScreenOrientationLock(orientation);
  }
}

}

