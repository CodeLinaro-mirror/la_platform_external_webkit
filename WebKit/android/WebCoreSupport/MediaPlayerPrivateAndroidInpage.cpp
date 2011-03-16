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

#include "config.h"

#if ENABLE(VIDEO) && ENABLE(INPAGE_VIDEO)

#define LOG_TAG "MediaPlayerPrivateAndroidInpage"
// #define LOG_NDEBUG 0

#include "MediaPlayerPrivateAndroidInpage.h"

#include "CString.h"
#include "CurrentTime.h"
#include "GraphicsContext.h"
#include "HTMLVideoElement.h"
#include "HashSet.h"
#include "IntRect.h"
#include "IntSize.h"
#include "MediaPlayer.h"
#include "MediaPlayerPrivate.h"
#include "MessageQueue.h"
#include "NotImplemented.h"
#include "OMX_Video.h"
#include "PassOwnPtr.h"
#include "PassRefPtr.h"
#include "PlatformBridge.h"
#include "PlatformGraphicsContext.h"
#include "PlatformString.h"
#include "SkBitmap.h"
#include "SkCanvas.h"
#include "SkColor.h"
#include "SkPixelRef.h"
#include "SkRect.h"
#include "TimeRanges.h"
#include "Timer.h"
#include "WKAccel.h"
#include "binder/MemoryHeapBase.h"
#include "binder/Parcel.h"
#include "binder/ProcessState.h"
#include "media/IMediaPlayer.h"
#include "media/mediaplayer.h"
#include "media/stagefright/MediaErrors.h"
#include "ui/PixelFormat.h"
#include "utils/Errors.h"
#include "utils/KeyedVector.h"
#include "utils/RefBase.h"
#include "utils/String16.h"
#include "utils/String8.h"

using namespace WTF;

namespace WebCore {

// Stagefright media player message handlers.
#define MPPS_BEGIN_MESSAGE_MAP(messagePacket) \
{ \
    MessagePacket* msgPacket = messagePacket.get(); \
    switch (msgPacket->msg()) { \

#define MPPS_MESSAGE_HANDLER(messageType, handlerFunction) \
    case messageType: \
        handlerFunction(msgPacket); \
        break;

#define MPPS_MESSAGE_UNHANDLED(handlerFunction) \
    default: \
        handlerFunction(msgPacket); \
        break;

#define MPPS_END_MESSAGE_MAP() \
    } \
}

#define CHECK_NETWORK_STATE(status) \
    if (status != android::NO_ERROR) \
        return setNetworkErrorState(status);

#define CHECK_REPLY_SUCCESS(status) \
    if (status != android::NO_ERROR) \
        return setNetworkErrorState(android::INVALID_OPERATION);

void MediaPlayerPrivateAndroidInpage::registerMediaEngine(MediaEngineRegistrar registrar)
{
    SLOGV("In-page video media engine registered");
    registrar(create, getMediaEngineType, getSupportedTypes, supportsType);
}

MediaPlayerPrivateInterface* MediaPlayerPrivateAndroidInpage::create(MediaPlayer* player)
{
    return new MediaPlayerPrivateAndroidInpage(player);
}

void MediaPlayerPrivateAndroidInpage::getSupportedTypes(HashSet<String>& types)
{
    types = mimeTypeCache();
}

MediaPlayer::SupportsType MediaPlayerPrivateAndroidInpage::supportsType(const String& type, const String& codecs)
{
    if (type.isNull())
        return MediaPlayer::IsNotSupported;

    if (mimeTypeCache().contains(type))
        return codecs.isEmpty() ? MediaPlayer::MayBeSupported : MediaPlayer::IsSupported;

    return MediaPlayer::IsNotSupported;
}

HashSet<String> MediaPlayerPrivateAndroidInpage::mimeTypeCache()
{
    static HashSet<String> cache;

    if (cache.isEmpty()) {
        cache.add(String("video/mp4"));
        cache.add(String("video/3gpp"));
    }

    return cache;
}

MediaPlayerPrivateAndroidInpage::~MediaPlayerPrivateAndroidInpage()
{
    m_timer.stop();
    m_notifyQueue.kill();

    if (!m_prepared)
        return;

    unregisterHeap();
    m_mp->suspend();
    m_mp->stop();
    m_mp->setListener(0);
    m_mp->disconnect();
}

void MediaPlayerPrivateAndroidInpage::load(const String& url)
{
    m_url = url;

    String cookie = PlatformBridge::cookies(KURL(ParsedURLString, url));

    android::KeyedVector<android::String8, android::String8> headers;
    if (!cookie.isEmpty())
        headers.add(android::String8("Cookie"), android::String8(cookie.utf8().data()));

    CHECK_NETWORK_STATE(m_mp->setDataSource(url.utf8().data(), &headers));
    CHECK_NETWORK_STATE(m_mp->setListener(WebCoreMediaPlayerListener::create(this)));

    // TODO Should query the media player for this.
    m_hasAudio = true;
    m_hasVideo = m_player->mediaElementType() == MediaPlayer::Video;

    if (m_hasVideo) // For video we need to update at 30 FPS.
        m_timer.augmentRepeatInterval(VideoTimerFrequency - TimerFrequency);

    m_networkState = MediaPlayer::Loading;
    m_player->networkStateChanged();

    m_readyState = MediaPlayer::HaveMetadata;
    m_player->readyStateChanged();

    registerHeap(false);
    m_mp->prepareAsync();
}

void MediaPlayerPrivateAndroidInpage::cancelLoad()
{
    notImplemented();
}

void MediaPlayerPrivateAndroidInpage::play()
{
    if (m_hasVideo && m_shouldShowPoster) {
        m_shouldShowPoster = false;
        static_cast<HTMLVideoElement*>(m_player->mediaPlayerClient())->updatePosterImage();
    }

    m_mp->start();
}

void MediaPlayerPrivateAndroidInpage::pause()
{
    m_mp->pause();
}

void MediaPlayerPrivateAndroidInpage::setVisible(bool b)
{
    if (b == m_visible)
        return;

    m_visible = b;
    if (m_visible) {
        registerHeap();
        m_mp->resume();

        if (!m_hasVideo)
            return;

        for (int index = NumFrameBuffer; index; --index)
            sendFrameToMediaPlayer(index-1);
    } else {
        unregisterHeap();
        m_mp->suspend();
    }
}

float MediaPlayerPrivateAndroidInpage::currentTime() const
{
    if (m_completed)
        return duration();

    int msec;
    if (android::NO_ERROR != m_mp->getCurrentPosition(&msec))
        return 0.f;

    return static_cast<float>(msec) / SecondsToMilliseconds;
}

void MediaPlayerPrivateAndroidInpage::seek(float seconds)
{
    if (!m_prepared) {
        m_seekTime  = seconds;
        return;
    }

    m_mp->seekTo(static_cast<int>(seconds) * SecondsToMilliseconds);
    m_seeking = true;
    m_completed = false;

    // TODO We need to consider the ready state when seeking. If we are
    // streaming and we do a seek, we may have to drop out of the
    // HaveEnoughData state.
}

void MediaPlayerPrivateAndroidInpage::setRate(float f)
{
    notImplemented();
}

bool MediaPlayerPrivateAndroidInpage::paused() const
{
    return m_prepared ? !m_mp->isPlaying() : true;
}

void MediaPlayerPrivateAndroidInpage::setVolume(float f)
{
    m_mp->setVolume(f, f);
}

float MediaPlayerPrivateAndroidInpage::maxTimeSeekable() const
{
    return duration();
}

PassRefPtr<TimeRanges> MediaPlayerPrivateAndroidInpage::buffered() const
{
    RefPtr<TimeRanges> buffered = TimeRanges::create();
    buffered->add(0.0f, duration());

    return buffered;
}

unsigned MediaPlayerPrivateAndroidInpage::bytesLoaded() const
{
    notImplemented();
    return 0;
}

void MediaPlayerPrivateAndroidInpage::setSize(const IntSize& size)
{
    notImplemented();
}

void MediaPlayerPrivateAndroidInpage::paint(GraphicsContext* gc, const IntRect& rect)
{
    if (gc->paintingDisabled())
        return;

    if (!m_prepared)
        return;

    // No need to paint if we're just playing audio.
    if (!m_hasVideo)
        return;

    if (m_videoBitmap.empty()) {
#if ENABLE(INPAGE_RGB565)
        SLOGV("using RGB565 bitmaps");
        m_videoBitmap.setConfig(SkBitmap::kRGB_565_Config, m_width, m_height);
#else
        SLOGV("using ARGB8888 bitmaps");
        m_videoBitmap.setConfig(SkBitmap::kARGB_8888_Config, m_width, m_height);
#endif
        m_videoBitmap.allocPixels();
        m_videoBitmap.eraseColor(SK_ColorBLACK);
    }

    SkCanvas* canvas = gc->platformContext()->mCanvas;

    if (m_frameQueue.isEmpty()) {
        // Draw a placeholder rectangle to indicate size and position of the video.
        canvas->drawBitmapRect(m_videoBitmap, 0, rect, 0);
        return;
    }

    // This will free current pixels, re-allocate, and lockPixels all in one
    // call. No need to call lock/unlock pixels when allocating pixels from
    // malloc heap.
    m_videoBitmap.allocPixels();

    int frame = m_frameQueue.first();
    m_frameQueue.removeFirst();

    char* yuvFrame = getFramePtr(frame);

    if (yuvFrame) {
        char* pixels = static_cast<char*>(m_videoBitmap.getPixels());
        yuvToRGBFrame(yuvFrame, pixels);

        sendFrameToMediaPlayer(frame);

        canvas->drawBitmapRect(m_videoBitmap, 0, rect, 0);
    }
}

bool MediaPlayerPrivateAndroidInpage::hasAvailableVideoFrame() const
{
    // The HTMLVideoElement creates the RenderImage for the poster. The poster
    // is shown until the media player has buffered enough content and is able
    // to generate a thumbnail based on the first few frames of the video. The
    // Android media player currently cannot generate such a thumbnail. Our
    // workaround is to tell the HTMLVideoElement that we have available video
    // frames once we actually start playing. That way we don't have to fetch
    // poster image as in MediaPlayerPrivateAndroid.  See
    // http://webkit.org/b/29133.
    return !m_shouldShowPoster && m_readyState >= MediaPlayer::HaveCurrentData;
}

MediaPlayerPrivateAndroidInpage::MediaPlayerPrivateAndroidInpage(MediaPlayer* player)
    : m_timer(this, &MediaPlayerPrivateAndroidInpage::timerFired)
    , m_player(player)
    , m_mp(0)
    , m_url("")
    , m_networkState(MediaPlayer::Empty)
    , m_readyState(MediaPlayer::HaveNothing)
    , m_hasVideo(false)
    , m_hasAudio(false)
    , m_width(0)
    , m_height(0)
    , m_colorFormat(0)
    , m_frameSize(0)
    , m_duration(0.f)
    , m_pendingWidth(0)
    , m_pendingHeight(0)
    , m_pendingColorFormat(0)
    , m_shouldShowPoster(true)
    , m_visible(true)
    , m_prepared(false)
    , m_seeking(false)
    , m_completed(false)
    , m_realloc(false)
    , m_seekTime(0.f)
    , m_frameUsecsBuffered(0)
{
    m_timer.startRepeating(TimerFrequency);

    m_mp = new android::MediaPlayer();
    LOG_ASSERT(m_mp.get(), "Failed to retrieve android MediaPlayer framework");

    android::ProcessState::self()->startThreadPool();
}

void MediaPlayerPrivateAndroidInpage::onMediaNop(MessagePacket* messagePtr)
{
    SLOGV("MEDIA_NOP(%i, %i)", messagePtr->ext1(), messagePtr->ext2());
}

void MediaPlayerPrivateAndroidInpage::onMediaPrepared(MessagePacket* messagePtr)
{
    SLOGV("MEDIA_PREPARED(%i, %i)", messagePtr->ext1(), messagePtr->ext2());
    m_prepared = true;

    registerHeap();

    int msec;
    if (android::NO_ERROR == m_mp->getDuration(&msec)) {
        m_duration = static_cast<float>(msec) / SecondsToMilliseconds;
        m_player->durationChanged();
    }

    m_networkState = MediaPlayer::Loaded;
    m_player->networkStateChanged();

    m_readyState = MediaPlayer::HaveEnoughData;
    m_player->readyStateChanged();

    if (m_seekTime)
        seek(m_seekTime);

    m_player->repaint();
}

void MediaPlayerPrivateAndroidInpage::onMediaPlaybackComplete(MessagePacket* messagePtr)
{
    SLOGV("MEDIA_PLAYBACK_COMPLETE(%i, %i)", messagePtr->ext1(), messagePtr->ext2());
    // Notify client of end of stream
    m_completed = true;

    m_player->timeChanged();
}

void MediaPlayerPrivateAndroidInpage::onMediaBufferingUpdate(MessagePacket* messagePtr)
{
    // ext1 is percentage buffered in usec of total duration
    SLOGV("MEDIA_BUFFERING_UPDATE buffered: %i usecs duration: %f", messagePtr->ext1(), duration());
    m_frameUsecsBuffered = messagePtr->ext1();

    // HaveEnoughData should mean we have frames coming in fast enough to
    // maintain the playrate.
    m_readyState = MediaPlayer::HaveEnoughData;
    m_player->readyStateChanged();
}

void MediaPlayerPrivateAndroidInpage::onMediaSeekComplete(MessagePacket* messagePtr)
{
    SLOGV("MEDIA_SEEK_COMPLETE(%i, %i)", messagePtr->ext1(), messagePtr->ext2());
    m_seeking = false;
    m_player->timeChanged();

    // TODO If we are paused, the Android MediaPlayer doesn't update the
    // displayed frame after a seek. Playback must be started to see the new
    // frame.
}

void MediaPlayerPrivateAndroidInpage::onMediaSetVideoSize(MessagePacket* messagePtr)
{
    SLOGV("MEDIA_SET_VIDEO_SIZE(%i, %i)", messagePtr->ext1(), messagePtr->ext2());

    if ((!messagePtr->ext1()) || (!messagePtr->ext2())) {
        m_hasVideo = false;
        return;
    }

    m_pendingWidth = messagePtr->ext1();
    m_pendingHeight = messagePtr->ext2();
}

void MediaPlayerPrivateAndroidInpage::onMediaError(MessagePacket* messagePtr)
{
    SLOGE("MEDIA_ERROR(%i, %i)", messagePtr->ext1(), messagePtr->ext2());
    /* ext1 framework error, ext2 implementation error */
    setNetworkErrorState(messagePtr->ext1());

    if (android::MEDIA_INFO_UNKNOWN == messagePtr->ext1() && android::ERROR_IO == messagePtr->ext2())
        SLOGE("Unknown stagefright I/O error");

    /* report complete if error received */
    m_completed = true;
    m_player->timeChanged();
}

void MediaPlayerPrivateAndroidInpage::onMediaInfo(MessagePacket* messagePtr)
{
    SLOGW("MEDIA_INFO(%i, %i)", messagePtr->ext1(), messagePtr->ext2());
    /* ext1 framework error, ext2 implementation error */

    if (android::MEDIA_INFO_BUFFERING_START == messagePtr->ext1())
        SLOGW("Temporarily pausing new frames while buffering more data");

    if (android::MEDIA_INFO_BUFFERING_END == messagePtr->ext1())
        SLOGW("Resuming new frames");
}

void MediaPlayerPrivateAndroidInpage::onMediaFormatChanged(MessagePacket* messagePtr)
{
    SLOGV("MEDIA_FORMAT_CHANGED(%i, %i)", messagePtr->ext1(), messagePtr->ext2());
    m_realloc = true;

    m_videoBitmap.reset();

    if (m_pendingWidth && m_pendingHeight && m_pendingColorFormat
            && (static_cast<unsigned int>(NumFrameBuffer) == (m_reallocQueue.size() + m_frameQueue.size()))) {
        registerHeap();
    } else {
        SLOGV("missing buffers before reallocation, have %i+%i buffers", m_reallocQueue.size(), m_frameQueue.size());
        m_notifyQueue.append(MessagePacket::create(messagePtr->msg(), messagePtr->ext1(), messagePtr->ext2()));
    }
}

void MediaPlayerPrivateAndroidInpage::onMediaBufferReady(MessagePacket* messagePtr)
{
    SLOGV("MEDIA_BUFFER_READY(%i, %i)", messagePtr->ext1(), messagePtr->ext2());

    if (!m_hasVideo)
        return;

    if (m_mp->isPlaying()) {
        while (!m_frameQueue.isEmpty()) {
            int frame = m_frameQueue.first();
            m_frameQueue.removeFirst();
            SLOGV("returning frame without painting");
            sendFrameToMediaPlayer(frame);
        }
        m_player->repaint();
    }

    m_frameQueue.append(messagePtr->ext1());
}

void MediaPlayerPrivateAndroidInpage::onMediaBufferEmpty(MessagePacket* messagePtr)
{
    SLOGV("MEDIA_BUFFER_EMPTY(%i, %i)", messagePtr->ext1(), messagePtr->ext2());
    m_reallocQueue.append(messagePtr->ext1());
}

void MediaPlayerPrivateAndroidInpage::onMediaBufferFormat(MessagePacket* messagePtr)
{
    SLOGV("MEDIA_BUFFER_FORMAT(%i, %i)", messagePtr->ext1(), messagePtr->ext2());
    m_pendingColorFormat = messagePtr->ext1();
}

void MediaPlayerPrivateAndroidInpage::onUnhandledMessage(MessagePacket* messagePtr)
{
    SLOGV("UNKNOWN(%i, %i, %i)", messagePtr->msg(), messagePtr->ext1(), messagePtr->ext2());
}

void MediaPlayerPrivateAndroidInpage::timerFired(Timer<MediaPlayerPrivateAndroidInpage>*)
{
    while (OwnPtr<MessagePacket> messagePacket = m_notifyQueue.tryGetMessage()) {
        MPPS_BEGIN_MESSAGE_MAP(messagePacket)
            MPPS_MESSAGE_HANDLER(android::MEDIA_NOP, onMediaNop);
            MPPS_MESSAGE_HANDLER(android::MEDIA_PREPARED, onMediaPrepared);
            MPPS_MESSAGE_HANDLER(android::MEDIA_PLAYBACK_COMPLETE, onMediaPlaybackComplete);
            MPPS_MESSAGE_HANDLER(android::MEDIA_BUFFERING_UPDATE, onMediaBufferingUpdate);
            MPPS_MESSAGE_HANDLER(android::MEDIA_SEEK_COMPLETE, onMediaSeekComplete);
            MPPS_MESSAGE_HANDLER(android::MEDIA_SET_VIDEO_SIZE, onMediaSetVideoSize);
            MPPS_MESSAGE_HANDLER(android::MEDIA_ERROR, onMediaError);
            MPPS_MESSAGE_HANDLER(android::MEDIA_INFO, onMediaInfo);
            MPPS_MESSAGE_HANDLER(android::MEDIA_FORMAT_CHANGED, onMediaFormatChanged);
            MPPS_MESSAGE_HANDLER(android::MEDIA_BUFFER_READY, onMediaBufferReady);
            MPPS_MESSAGE_HANDLER(android::MEDIA_BUFFER_EMPTY, onMediaBufferEmpty);
            MPPS_MESSAGE_HANDLER(android::MEDIA_BUFFER_FORMAT, onMediaBufferFormat);
            MPPS_MESSAGE_UNHANDLED(onUnhandledMessage);
        MPPS_END_MESSAGE_MAP()
    }
}

void MediaPlayerPrivateAndroidInpage::setNetworkErrorState(android::status_t status)
{
    m_networkState = MediaPlayer::NetworkError;
    m_player->networkStateChanged();
}

void MediaPlayerPrivateAndroidInpage::sendFrameToMediaPlayer(int index)
{
     if (!m_hasVideo)
        return;

    if (m_realloc) {
        // place frame index onto reallocation queue
        m_reallocQueue.append(index);
        return;
    }

    android::Parcel request;
    android::Parcel reply;

    CHECK_NETWORK_STATE(request.writeInterfaceToken(android::String16("android.media.IMediaPlayer")));
    CHECK_NETWORK_STATE(request.writeInt32(android::MEDIA_INVOKE_QUEUE_BUFFER));
    CHECK_NETWORK_STATE(request.writeInt32(static_cast<int32_t>(index)));
    CHECK_NETWORK_STATE(m_mp->invoke(request, &reply));
    CHECK_REPLY_SUCCESS(reply.readInt32());
}

void MediaPlayerPrivateAndroidInpage::registerHeap(bool alloc)
{
     if (!m_hasVideo)
        return;

    if (m_frameHeap.get()) {
        SLOGV("unregistering previous shared memory, frames %d, m_frameHeap ID %d, base %p, size %d", NumFrameBuffer, m_frameHeap->getHeapID(), m_frameHeap->getBase(), m_frameHeap->getSize());
        unregisterHeap();
    }

    if (alloc) {
        if (m_pendingWidth) {
            m_width = m_pendingWidth;
            m_pendingWidth = 0;
        }
        if (m_pendingHeight) {
            m_height = m_pendingHeight;
            m_pendingHeight = 0;
        }
        if (m_pendingColorFormat) {
            m_colorFormat = m_pendingColorFormat;
            m_pendingColorFormat = 0;
        }
        m_player->sizeChanged();

        LOG_ASSERT(m_width, "Invalid width");
        LOG_ASSERT(m_height, "Invalid height");
        LOG_ASSERT(m_colorFormat, "Invalid color format");

        m_frameSize = WKAccel::calcFrameSize(m_colorFormat, m_width, m_height); // tiled YUV or YUV420
        m_frameHeap = new android::MemoryHeapBase(m_frameSize * NumFrameBuffer);
    } else
        m_frameHeap = new android::MemoryHeapBase(static_cast<size_t>(0));

    SLOGV("m_frameHeap is (0x%08lX, %i bytes, 0x%08lX flags)", reinterpret_cast<long>(m_frameHeap->getBase()), m_frameHeap->getSize(), m_frameHeap->getFlags());

    // register shared memory
    android::Parcel request;
    android::Parcel reply;

    CHECK_NETWORK_STATE(request.writeInterfaceToken(android::String16("android.media.IMediaPlayer")));
    CHECK_NETWORK_STATE(request.writeInt32(android::MEDIA_INVOKE_REGISTER_BUFFERS));
    CHECK_NETWORK_STATE(request.writeInt32(static_cast<int32_t>(NumFrameBuffer)));
    CHECK_NETWORK_STATE(request.writeInt32(static_cast<int32_t>(m_frameSize)));
    CHECK_NETWORK_STATE(request.writeStrongBinder(m_frameHeap->asBinder()));
    CHECK_NETWORK_STATE(m_mp->invoke(request, &reply));
    CHECK_REPLY_SUCCESS(reply.readInt32());

    // Passing frame indexes to mediaplayer/stagefright.
    if (alloc && m_visible)
        for (int index = NumFrameBuffer; index; --index)
            sendFrameToMediaPlayer(index-1);
}

void MediaPlayerPrivateAndroidInpage::unregisterHeap()
{
    if (!m_hasVideo)
        return;

    android::Parcel request;
    android::Parcel reply;

    CHECK_NETWORK_STATE(request.writeInterfaceToken(android::String16("android.media.IMediaPlayer")));
    CHECK_NETWORK_STATE(request.writeInt32(android::MEDIA_INVOKE_UNREGISTER_BUFFERS));
    CHECK_NETWORK_STATE(m_mp->invoke(request, &reply));
    CHECK_REPLY_SUCCESS(reply.readInt32());

    m_frameSize = 0;
    m_frameQueue.clear();
    m_reallocQueue.clear();
    m_realloc = false;
    m_frameHeap.clear();
}

void MediaPlayerPrivateAndroidInpage::yuvToRGBFrame(char* yuvFrame, char* rgbFrame)
{
    WKAccel::colorConverter(yuvFrame,
                            m_colorFormat,
                            rgbFrame,
#if ENABLE(INPAGE_RGB565)
                            OMX_COLOR_Format16bitRGB565,
#else
                            OMX_COLOR_Format32bitARGB8888,
#endif
                            m_width,
                            m_height);
}

android::sp<MediaPlayerPrivateAndroidInpage::WebCoreMediaPlayerListener> MediaPlayerPrivateAndroidInpage::WebCoreMediaPlayerListener::create(MediaPlayerPrivateAndroidInpage* mpp)
{
    return new MediaPlayerPrivateAndroidInpage::WebCoreMediaPlayerListener(mpp);
}

void MediaPlayerPrivateAndroidInpage::WebCoreMediaPlayerListener::notify(int msg, int ext1, int ext2)
{
    // notify is called by Android MediaPlayer thread -- notify is processed in the WebCore thread via the message queue
    m_mpp->m_notifyQueue.append(MessagePacket::create(msg, ext1, ext2));
}

PassOwnPtr<MediaPlayerPrivateAndroidInpage::MessagePacket> MediaPlayerPrivateAndroidInpage::MessagePacket::create(int msg, int ext1, int ext2)
{
    return new MessagePacket(msg, ext1, ext2);
}

} // namespace WebCore

#endif // ENABLE(VIDEO) && ENABLE(INPAGE_VIDEO)
