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
#include "INodeClientJavaBridge.h"
#include "V8Proxy.h"
#include <JNIUtility.h>

namespace WebCore {

INodeClientJavaBridgeImpl::INodeClientJavaBridgeImpl(INodeProxy *proxy)
  : m_proxy(proxy) {
      m_proxy->registerInterface(dapi::INTERFACE_JAVABRIDGE, this);
}

jobject INodeClientJavaBridgeImpl::getWebView() {
    android::WebViewCore* webViewCore =
      android::WebViewCore::getWebViewCore(m_proxy->frame()->view());
    jobject javaObject  = webViewCore->getWebViewJavaObject();
    return javaObject;
}

// Gets the context
jobject INodeClientJavaBridgeImpl::getContext() {
    NODE_LOGV("INodeProxy::getContext");
    jclass webviewCls = 0;
    JNIEnv* env = getJNIEnv();
    jobject webViewObj = getWebView();
    webviewCls = env->FindClass("android/webkit/WebView");
    jmethodID getContextMet = env->GetMethodID(webviewCls, "getContext", "()Landroid/content/Context;");
    jobject contextObj = env->CallObjectMethod(webViewObj, getContextMet);
    env->DeleteLocalRef(webviewCls);
    return contextObj;
}

JNIEnv* INodeClientJavaBridgeImpl::getJNIEnv() {
    return JSC::Bindings::getJNIEnv();
}

}

