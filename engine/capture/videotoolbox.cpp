// The platform's encoders on macOS and iOS (platform_video.hpp): VideoToolbox's compression and
// decompression sessions behind the packet interface. The only file that includes VideoToolbox;
// built only on Apple's platforms with GE_ENCODERS on (platform_video.cpp is the stub elsewhere).
//
// A compression session is asked for the hardware encoder and takes the system's software one
// when the machine has none (a virtual machine's runner); `hardware()` says which it got. Each
// picture is copied into a pixel buffer from the session's pool as NV12, encoded, and waited for
// (`VTCompressionSessionCompleteFrames`), so its packet is in `out` when `encode` returns. The
// callback runs on VideoToolbox's thread, so what it hands back goes through the Impl's lock.
//
// ge:summary VideoToolbox's H.264 and HEVC sessions as the platform encoder and decoder
// ge:keywords videotoolbox vtcompressionsession vtdecompressionsession cmsamplebuffer
// ge:keywords cvpixelbuffer nv12 avcc hvcc sample-description-atoms hardware-encoder macos ios
// ge:skill replay
// ge:test tests/capture_platform_video_test.cpp
#include "capture/platform_video.hpp"

#if defined(__APPLE__) && GE_ENCODERS

#include <CoreFoundation/CoreFoundation.h>
#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>
#include <VideoToolbox/VideoToolbox.h>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <string>

#include "capture/yuv.hpp"

namespace ge::capture {

namespace {

// A CoreFoundation reference, released when it goes.
template <typename T>
struct Cf {
  T ref = nullptr;
  Cf() = default;
  explicit Cf(T r) : ref(r) {}
  ~Cf() {
    if (ref) CFRelease(ref);
  }
  Cf(const Cf&) = delete;
  Cf& operator=(const Cf&) = delete;
  Cf(Cf&& o) noexcept : ref(o.ref) { o.ref = nullptr; }
  Cf& operator=(Cf&&) = delete;
  T get() const { return ref; }
};

std::string failed(const char* what, OSStatus s) {
  return std::string(what) + " failed with OSStatus " + std::to_string(static_cast<int>(s)) +
         "; run tests/capture_platform_video_test.cpp on this machine (skill: replay)";
}

CMVideoCodecType vt_type(PlatformCodec c) {
  return c == PlatformCodec::kHevc ? kCMVideoCodecType_HEVC : kCMVideoCodecType_H264;
}

CFStringRef atom_key(PlatformCodec c) {
  return c == PlatformCodec::kHevc ? CFSTR("hvcC") : CFSTR("avcC");
}

Cf<CFNumberRef> number(int64_t v) {
  return Cf<CFNumberRef>(CFNumberCreate(nullptr, kCFNumberSInt64Type, &v));
}
Cf<CFNumberRef> number(double v) {
  return Cf<CFNumberRef>(CFNumberCreate(nullptr, kCFNumberDoubleType, &v));
}

Cf<CFMutableDictionaryRef> dictionary() {
  return Cf<CFMutableDictionaryRef>(CFDictionaryCreateMutable(
      nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
}

// The `avcC` or `hvcC` record a format description carries in its sample description's atoms.
std::vector<uint8_t> config_of(CMFormatDescriptionRef f, PlatformCodec c) {
  if (!f) return {};
  CFTypeRef atoms = CMFormatDescriptionGetExtension(
      f, kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms);
  if (!atoms || CFGetTypeID(atoms) != CFDictionaryGetTypeID()) return {};
  CFTypeRef data = CFDictionaryGetValue(static_cast<CFDictionaryRef>(atoms), atom_key(c));
  if (!data || CFGetTypeID(data) != CFDataGetTypeID()) return {};
  const auto* d = static_cast<CFDataRef>(data);
  const UInt8* p = CFDataGetBytePtr(d);
  return {p, p + CFDataGetLength(d)};
}

// The pixel buffers a session takes or gives: NV12 at video range, `width` x `height`.
Cf<CFMutableDictionaryRef> nv12_attributes(int width, int height) {
  Cf<CFMutableDictionaryRef> a = dictionary();
  const Cf<CFNumberRef> f = number(int64_t{kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange});
  const Cf<CFNumberRef> w = number(int64_t{width}), h = number(int64_t{height});
  const Cf<CFMutableDictionaryRef> surface = dictionary();
  CFDictionarySetValue(a.get(), kCVPixelBufferPixelFormatTypeKey, f.get());
  CFDictionarySetValue(a.get(), kCVPixelBufferWidthKey, w.get());
  CFDictionarySetValue(a.get(), kCVPixelBufferHeightKey, h.get());
  CFDictionarySetValue(a.get(), kCVPixelBufferIOSurfacePropertiesKey, surface.get());
  return a;
}

}  // namespace

std::string_view platform_encoder_name() { return "videotoolbox"; }

struct PlatformEncoder::Impl {
  PlatformCodec codec = PlatformCodec::kH264;
  VTCompressionSessionRef session = nullptr;
  int width = 0, height = 0;
  std::mutex mu;                           // the callback's thread and the caller's
  std::vector<Packet>* out = nullptr;      // where the callback appends during a call
  std::vector<uint8_t>* config = nullptr;  // the record the file names; empty: the probe's
  std::string error;

  ~Impl() { close(); }
  void close() {
    if (!session) return;
    VTCompressionSessionInvalidate(session);
    CFRelease(session);
    session = nullptr;
  }

  // One packet out of the session (on VideoToolbox's thread).
  void encoded(OSStatus status, VTEncodeInfoFlags flags, CMSampleBufferRef sb) {
    std::lock_guard lk(mu);
    if (status != noErr) {
      error = failed("VideoToolbox's encode of a picture", status);
      return;
    }
    if ((flags & kVTEncodeInfo_FrameDropped) != 0 || !sb) {
      error = "VideoToolbox dropped a picture; the export takes every one (skill: replay)";
      return;
    }
    CMBlockBufferRef bb = CMSampleBufferGetDataBuffer(sb);
    if (!bb) return;
    Packet p;
    p.data.resize(CMBlockBufferGetDataLength(bb));
    if (CMBlockBufferCopyDataBytes(bb, 0, p.data.size(), p.data.data()) != kCMBlockBufferNoErr) {
      error = "VideoToolbox's packet could not be copied out of its block buffer";
      return;
    }
    const CMTime pts = CMSampleBufferGetPresentationTimeStamp(sb);
    p.ms = pts.timescale > 0 ? pts.value * 1000 / pts.timescale : 0;
    CFArrayRef att = CMSampleBufferGetSampleAttachmentsArray(sb, false);
    if (att && CFArrayGetCount(att) > 0) {
      const auto* d = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(att, 0));
      const void* not_sync = nullptr;
      if (CFDictionaryGetValueIfPresent(d, kCMSampleAttachmentKey_NotSync, &not_sync) && not_sync &&
          CFBooleanGetValue(static_cast<CFBooleanRef>(not_sync))) {
        p.key = false;
      }
    }
    if (p.key && config) {
      std::vector<uint8_t> c = config_of(CMSampleBufferGetFormatDescription(sb), codec);
      if (config->empty()) {
        *config = std::move(c);
      } else if (!c.empty() && c != *config) {
        error = "VideoToolbox's keyframe at " + std::to_string(p.ms) +
                " ms carries other parameter sets than the probe the file's track was opened with; "
                "the file cannot name two (skill: replay)";
        return;
      }
    }
    if (out) out->push_back(std::move(p));
  }

  bool start(PlatformCodec c, const PlatformVideoOptions& o, std::string& e) {
    codec = c;
    width = o.width;
    height = o.height;
    const Cf<CFMutableDictionaryRef> spec = dictionary();
    CFDictionarySetValue(spec.get(),
                         kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder,
                         kCFBooleanTrue);
    const Cf<CFMutableDictionaryRef> attrs = nv12_attributes(o.width, o.height);
    VTCompressionOutputCallback cb = [](void* self, void*, OSStatus status, VTEncodeInfoFlags flags,
                                        CMSampleBufferRef sb) {
      static_cast<Impl*>(self)->encoded(status, flags, sb);
    };
    OSStatus s = VTCompressionSessionCreate(nullptr, o.width, o.height, vt_type(c), spec.get(),
                                            attrs.get(), nullptr, cb, this, &session);
    if (s != noErr) {
      e = failed(c == PlatformCodec::kHevc ? "VideoToolbox's HEVC session"
                                           : "VideoToolbox's H.264 session",
                 s);
      return false;
    }
    const int64_t bitrate =
        o.bitrate > 0 ? o.bitrate : std::llround(0.1 * o.width * o.height * o.fps);
    const auto key_frames = static_cast<int64_t>(std::max(1.0, std::round(o.fps * o.key_seconds)));
    VTSessionSetProperty(session, kVTCompressionPropertyKey_RealTime, kCFBooleanFalse);
    VTSessionSetProperty(session, kVTCompressionPropertyKey_AllowFrameReordering, kCFBooleanFalse);
    VTSessionSetProperty(session, kVTCompressionPropertyKey_ProfileLevel,
                         c == PlatformCodec::kHevc ? kVTProfileLevel_HEVC_Main_AutoLevel
                                                   : kVTProfileLevel_H264_High_AutoLevel);
    VTSessionSetProperty(session, kVTCompressionPropertyKey_MaxKeyFrameInterval,
                         number(key_frames).get());
    VTSessionSetProperty(session, kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration,
                         number(o.key_seconds).get());
    VTSessionSetProperty(session, kVTCompressionPropertyKey_ExpectedFrameRate, number(o.fps).get());
    VTSessionSetProperty(session, kVTCompressionPropertyKey_AverageBitRate, number(bitrate).get());
    // The pictures are BT.709 at limited range (capture/yuv.hpp).
    VTSessionSetProperty(session, kVTCompressionPropertyKey_ColorPrimaries,
                         kCVImageBufferColorPrimaries_ITU_R_709_2);
    VTSessionSetProperty(session, kVTCompressionPropertyKey_TransferFunction,
                         kCVImageBufferTransferFunction_ITU_R_709_2);
    VTSessionSetProperty(session, kVTCompressionPropertyKey_YCbCrMatrix,
                         kCVImageBufferYCbCrMatrix_ITU_R_709_2);
    s = VTCompressionSessionPrepareToEncodeFrames(session);
    if (s != noErr) {
      e = failed("VideoToolbox's PrepareToEncodeFrames", s);
      return false;
    }
    return true;
  }

  bool hardware() const {
    CFTypeRef v = nullptr;
    if (VTSessionCopyProperty(session,
                              kVTCompressionPropertyKey_UsingHardwareAcceleratedVideoEncoder,
                              nullptr, &v) != noErr ||
        !v) {
      return false;
    }
    const bool on =
        CFGetTypeID(v) == CFBooleanGetTypeID() && CFBooleanGetValue(static_cast<CFBooleanRef>(v));
    CFRelease(v);
    return on;
  }

  bool encode(std::span<const uint8_t> i420, int64_t ms, std::vector<Packet>& to, std::string& e) {
    if (i420.size() != i420_bytes(width, height)) {
      e = "a picture of " + std::to_string(i420.size()) + " bytes, not the " +
          std::to_string(i420_bytes(width, height)) + " of I420 at the encoder's size";
      return false;
    }
    CVPixelBufferPoolRef pool = VTCompressionSessionGetPixelBufferPool(session);
    CVPixelBufferRef raw = nullptr;
    if (!pool || CVPixelBufferPoolCreatePixelBuffer(nullptr, pool, &raw) != kCVReturnSuccess) {
      e = "VideoToolbox's session gave no pixel buffer; run "
          "tests/capture_platform_video_test.cpp (skill: replay)";
      return false;
    }
    const Cf<CVPixelBufferRef> px(raw);
    CVPixelBufferLockBaseAddress(raw, 0);
    i420_to_nv12(i420, width, height,
                 static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(raw, 0)),
                 CVPixelBufferGetBytesPerRowOfPlane(raw, 0),
                 static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(raw, 1)),
                 CVPixelBufferGetBytesPerRowOfPlane(raw, 1));
    CVPixelBufferUnlockBaseAddress(raw, 0);
    {
      std::lock_guard lk(mu);
      out = &to;
      error.clear();
    }
    const CMTime pts = CMTimeMake(ms, 1000);
    OSStatus s = VTCompressionSessionEncodeFrame(session, raw, pts, kCMTimeInvalid, nullptr,
                                                 nullptr, nullptr);
    if (s == noErr) s = VTCompressionSessionCompleteFrames(session, pts);
    std::lock_guard lk(mu);
    out = nullptr;
    if (s != noErr) {
      e = failed("VideoToolbox's EncodeFrame", s);
      return false;
    }
    if (!error.empty()) {
      e = error;
      return false;
    }
    return true;
  }
};

PlatformEncoder::PlatformEncoder() = default;
PlatformEncoder::~PlatformEncoder() = default;

bool PlatformEncoder::open(PlatformCodec c, const PlatformVideoOptions& o, std::string& error) {
  if (o.width <= 0 || o.height <= 0 || o.width % 2 != 0 || o.height % 2 != 0 || !(o.fps > 0)) {
    error = "the platform encoder needs an even size and a frame rate over 0, not " +
            std::to_string(o.width) + "x" + std::to_string(o.height);
    return false;
  }
  config_.clear();
  // The probe: a black picture through a session of the same settings, for the record the
  // file's track names before the first real packet exists.
  {
    Impl probe;
    if (!probe.start(c, o, error)) return false;
    probe.config = &config_;
    std::vector<uint8_t> black(i420_bytes(o.width, o.height), 16);
    std::fill(black.begin() + static_cast<std::ptrdiff_t>(o.width) * o.height, black.end(), 128);
    std::vector<Packet> ignored;
    if (!probe.encode(black, 0, ignored, error)) return false;
    if (config_.empty()) {
      error = "VideoToolbox's first keyframe carried no parameter sets record";
      return false;
    }
  }
  impl_ = std::make_unique<Impl>();
  if (!impl_->start(c, o, error)) {
    impl_.reset();
    return false;
  }
  impl_->config = &config_;
  hardware_ = impl_->hardware();
  return true;
}

bool PlatformEncoder::encode(std::span<const uint8_t> i420, int64_t ms, std::vector<Packet>& out,
                             std::string& error) {
  if (!impl_) {
    error = "the platform encoder is not open";
    return false;
  }
  return impl_->encode(i420, ms, out, error);
}

bool PlatformEncoder::flush(std::vector<Packet>&, std::string&) {
  // Every packet came out with its picture (encode waits for it).
  return true;
}

struct PlatformDecoder::Impl {
  VTDecompressionSessionRef session = nullptr;
  CMVideoFormatDescriptionRef format = nullptr;
  std::mutex mu;
  std::vector<uint8_t>* out = nullptr;
  int* width = nullptr;
  int* height = nullptr;
  bool* got = nullptr;
  std::string error;

  ~Impl() {
    if (session) {
      VTDecompressionSessionInvalidate(session);
      CFRelease(session);
    }
    if (format) CFRelease(format);
  }

  void decoded(OSStatus status, CVImageBufferRef image) {
    std::lock_guard lk(mu);
    if (status != noErr) {
      error = failed("VideoToolbox's decode of a packet", status);
      return;
    }
    if (!image || !out) return;
    CVPixelBufferLockBaseAddress(image, kCVPixelBufferLock_ReadOnly);
    const auto w = static_cast<int>(CVPixelBufferGetWidth(image));
    const auto h = static_cast<int>(CVPixelBufferGetHeight(image));
    nv12_to_i420(static_cast<const uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(image, 0)),
                 CVPixelBufferGetBytesPerRowOfPlane(image, 0),
                 static_cast<const uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(image, 1)),
                 CVPixelBufferGetBytesPerRowOfPlane(image, 1), w, h, *out);
    CVPixelBufferUnlockBaseAddress(image, kCVPixelBufferLock_ReadOnly);
    *width = w;
    *height = h;
    *got = true;
  }
};

PlatformDecoder::PlatformDecoder() = default;
PlatformDecoder::~PlatformDecoder() = default;

bool PlatformDecoder::open(PlatformCodec c, std::span<const uint8_t> config, int width, int height,
                           std::string& error) {
  impl_ = std::make_unique<Impl>();
  const Cf<CFDataRef> data(
      CFDataCreate(nullptr, config.data(), static_cast<CFIndex>(config.size())));
  const Cf<CFMutableDictionaryRef> atoms = dictionary();
  CFDictionarySetValue(atoms.get(), atom_key(c), data.get());
  const Cf<CFMutableDictionaryRef> ext = dictionary();
  CFDictionarySetValue(ext.get(), kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms,
                       atoms.get());
  OSStatus s =
      CMVideoFormatDescriptionCreate(nullptr, vt_type(c), width, height, ext.get(), &impl_->format);
  if (s != noErr) {
    error = failed("CoreMedia's format description from the track's record", s);
    impl_.reset();
    return false;
  }
  const Cf<CFMutableDictionaryRef> attrs = nv12_attributes(width, height);
  VTDecompressionOutputCallbackRecord cb;
  cb.decompressionOutputCallback = [](void* self, void*, OSStatus status, VTDecodeInfoFlags,
                                      CVImageBufferRef image, CMTime,
                                      CMTime) { static_cast<Impl*>(self)->decoded(status, image); };
  cb.decompressionOutputRefCon = impl_.get();
  s = VTDecompressionSessionCreate(nullptr, impl_->format, nullptr, attrs.get(), &cb,
                                   &impl_->session);
  if (s != noErr) {
    error = failed("VideoToolbox's decompression session", s);
    impl_.reset();
    return false;
  }
  return true;
}

bool PlatformDecoder::decode(std::span<const uint8_t> packet, std::vector<uint8_t>& i420,
                             int& width, int& height, bool& got, std::string& error) {
  got = false;
  if (!impl_) {
    error = "the platform decoder is not open";
    return false;
  }
  CMBlockBufferRef raw_block = nullptr;
  OSStatus s = CMBlockBufferCreateWithMemoryBlock(nullptr, nullptr, packet.size(),
                                                  kCFAllocatorDefault, nullptr, 0, packet.size(),
                                                  kCMBlockBufferAssureMemoryNowFlag, &raw_block);
  const Cf<CMBlockBufferRef> block(raw_block);
  if (s == kCMBlockBufferNoErr) {
    s = CMBlockBufferReplaceDataBytes(packet.data(), raw_block, 0, packet.size());
  }
  CMSampleBufferRef raw_sample = nullptr;
  const size_t size = packet.size();
  if (s == noErr) {
    s = CMSampleBufferCreateReady(nullptr, raw_block, impl_->format, 1, 0, nullptr, 1, &size,
                                  &raw_sample);
  }
  const Cf<CMSampleBufferRef> sample(raw_sample);
  if (s != noErr) {
    error = failed("CoreMedia's sample buffer for a packet", s);
    return false;
  }
  {
    std::lock_guard lk(impl_->mu);
    impl_->out = &i420;
    impl_->width = &width;
    impl_->height = &height;
    impl_->got = &got;
    impl_->error.clear();
  }
  s = VTDecompressionSessionDecodeFrame(impl_->session, raw_sample, 0, nullptr, nullptr);
  if (s == noErr) s = VTDecompressionSessionWaitForAsynchronousFrames(impl_->session);
  std::lock_guard lk(impl_->mu);
  impl_->out = nullptr;
  if (s != noErr) {
    error = failed("VideoToolbox's DecodeFrame", s);
    return false;
  }
  if (!impl_->error.empty()) {
    error = impl_->error;
    return false;
  }
  return true;
}

}  // namespace ge::capture

#endif
