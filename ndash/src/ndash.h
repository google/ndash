/*
 * Copyright 2017 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef NDASH_H_
#define NDASH_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int64_t MediaTimeMs;
typedef int64_t MediaDurationMs;
typedef int64_t MediaTimePts;      // In units of frame_info.timebase.
typedef int64_t MediaDurationPts;  // In units of frame_info.timebase.

struct ndash_handle;

#define NDASH_EXPORT __attribute__((visibility("default")))

/*
 * Player callbacks.
 */

// Should return a time (ms) as close as possible to the PTS value that was
// specified by CopyFrame for the frame currently being rendered (either
// audio/video) by the decoder.  DashThread will use this value to know when to
// begin/end it's read-ahead operations to keep its frame buffer full.  Note
// the PTS values set in FrameInfo by CopyFrame() are always relative to a
// start time of 0. This is to ensure the 90khz PTS values first reported will
// always fit in 33 bits. It is the responsibility of the decoder to detect PTS
// rollover and adjust this time accordingly.  That is, this value should NOT
// rollover, it should always be monotonically increasing (when playing
// forward).
// The callee may return -1 to indicate it does not yet have a valid PTS. This
// is useful in the case where it was called before the player's decoder had a
// chance to decode any frame yet.
typedef MediaTimeMs (*DashPlayerGetMediaTimeFunc)(void* context);

// Flush the client player's byte stream.
typedef void (*DashPlayerFlushFunc)(void* context);

typedef enum { DASH_CDM_SUCCESS, DASH_CDM_FAILURE } DashCdmStatus;

// Open a new CDM session.
// session_id - Points to the newly created session id.
typedef DashCdmStatus (*DashPlayerOpenCdmSessionFunc)(void* context,
                                                      char** session_id,
                                                      size_t* len);

// Close a CDM session.
typedef DashCdmStatus (*DashPlayerCloseCdmSessionFunc)(void* context,
                                                       const char* session_id,
                                                       size_t len);

// Start a license request for the given pssh data.
typedef DashCdmStatus (*DashPlayerFetchLicenseFunc)(void* context,
                                                    const char* session_id,
                                                    size_t session_id_len,
                                                    const char* pssh,
                                                    size_t pssh_len);

typedef struct {
  DashPlayerGetMediaTimeFunc get_media_time_ms_func;
  DashPlayerFlushFunc decoder_flush_func;
  DashPlayerFetchLicenseFunc fetch_license_func;
  DashPlayerOpenCdmSessionFunc open_cdm_session_func;
  DashPlayerCloseCdmSessionFunc close_cdm_session_func;
} DashPlayerCallbacks;

// Returns a handle to a new DASH player, setting the player callbacks and
// context argument at the same time.
NDASH_EXPORT struct ndash_handle* ndash_create(DashPlayerCallbacks* callbacks,
                                               void* context);

// Update the callbacks.
NDASH_EXPORT void ndash_set_callbacks(struct ndash_handle* handle,
                                      DashPlayerCallbacks* callbacks);

// Update the context argument passed to callback invocations.
NDASH_EXPORT void ndash_set_context(struct ndash_handle* handle, void* context);

// Destroy a DASH Player instance.
NDASH_EXPORT void ndash_destroy(struct ndash_handle* handle);

// Play the requested media beginning at initialTimeSec.
// Returns 0 for success. Otherwise a failure occurred.
NDASH_EXPORT int ndash_load(struct ndash_handle* handle,
                            const char* url,
                            // TODO(rdaum): Switch to MediaTime*?
                            float initial_time_sec);

// Unload the player, releasing all resources held.
NDASH_EXPORT void ndash_unload(struct ndash_handle* handle);

typedef enum {
  DASH_VIDEO_UNSUPPORTED,
  DASH_VIDEO_NONE,
  DASH_VIDEO_H264,
  // TODO(rdaum): Add other codecs as they become available.,
} DashVideoCodec;

typedef struct {
  DashVideoCodec video_codec;
  int width;
  int height;
} DashVideoCodecSettings;

// Populates codec_settings with the video codec settings.
// Returns 0 on success.
NDASH_EXPORT int ndash_get_video_codec_settings(
    struct ndash_handle* handle,
    DashVideoCodecSettings* codec_settings);

typedef enum {
  DASH_AUDIO_UNSUPPORTED,
  DASH_AUDIO_NONE,
  DASH_AUDIO_MPEG_LAYER123,
  DASH_AUDIO_AAC,
  DASH_AUDIO_AC3,
  DASH_AUDIO_DTS,
  DASH_AUDIO_EAC3,
  // TODO(rdaum): Add other codecs as they become available.
} DashAudioCodec;

typedef enum {
  kSampleFormatUnknown,
  kSampleFormatU8,         // Unsigned 8-bit center 128.
  kSampleFormatS16,        // Signed 16-bit.
  kSampleFormatS32,        // Signed 32-bit.
  kSampleFormatF32,        // Float 32-bit.
  kSampleFormatPlanarS16,  // Signed 16-bit planar.
  kSampleFormatPlanarF32,  // Float 32-bit planar.
  kSampleFormatPlanarS32,  // Signed 32-bit planar.
  kSampleFormatS24,        // Signed 24-bit.
} DashSampleFormat;

typedef enum {
  kChannelLayoutNone,
  kChannelLayoutUnsupported,
  kChannelLayoutMono,
  kChannelLayoutStereo,
  kChannelLayout2_1,
  kChannelLayoutSurround,
  kChannelLayout4_0,
  kChannelLayout2_2,
  kChannelLayoutQuad,
  kChannelLayout5_0,
  kChannelLayout5_1,
  kChannelLayout5_0_Back,
  kChannelLayout5_1_Back,
  kChannelLayout7_0,
  kChannelLayout7_1,
  kChannelLayout7_1_Wide,
  kChannelLayoutStereoDownmix,
  kChannelLayout2Point1,
  kChannelLayout3_1,
  kChannelLayout4_1,
  kChannelLayout6_0,
  kChannelLayout6_0_Front,
  kChannelLayoutHexagonal,
  kChannelLayout6_1,
  kChannelLayout6_1_Back,
  kChannelLayout6_1_Front,
  kChannelLayout7_0_Front,
  kChannelLayout7_1_WideBack,
  kChannelLayoutOctagonal,
  kChannelLayoutDiscrete,
  kChannelLayoutStereoAndKeyboardMic,
  kChannelLayout4_1_QuadSide,
} DashChannelLayout;

typedef struct {
  DashAudioCodec audio_codec;
  int num_channels;
  DashChannelLayout channel_layout;
  DashSampleFormat sample_format;
  int bps;
  int sample_rate;
  int bitrate;
  int blockalign;
} DashAudioCodecSettings;

// Populates codec_settings with the audio codec settings.
// Returns 0 on success.
NDASH_EXPORT int ndash_get_audio_codec_settings(
    struct ndash_handle* handle,
    DashAudioCodecSettings* codec_settings);

typedef enum {
  DASH_CC_UNSUPPORTED,
  DASH_CC_NONE,
  DASH_CC_WEBVTT,  // Unsupported at this time.
  DASH_CC_RAWCC
} DashCCCodec;

struct DashCCCodecSettings {
  DashCCCodec cc_codec;
};

// Populates codec_settings with the close caption codec settings.
// Returns 0 on success.
NDASH_EXPORT int ndash_get_cc_codec_settings(
    struct ndash_handle* handle,
    DashCCCodecSettings* codec_settings);

typedef enum {
  DASH_FRAME_TYPE_INVALID,
  DASH_FRAME_TYPE_VIDEO,
  DASH_FRAME_TYPE_AUDIO,
  DASH_FRAME_TYPE_CC,
} DashFrameType;

// Bits for DashFrameInfo.flags field

// Indicates this is the first fragment copied from the frame data.
#define DASH_FRAME_INFO_FLAG_FIRST_FRAGMENT 1
// Indicates this is the last fragment copied from the.
#define DASH_FRAME_INFO_FLAG_LAST_FRAGMENT 2
// Indicates that the PTS is available for the frame.
// TODO(rdaum): Switch has_pts uses to this.
#define DASH_FRAME_INFO_FLAG_HAS_PTS 4

struct DashFrameInfo {
  DashFrameType type;
  uint32_t flags;
  MediaTimePts pts;
  MediaDurationPts duration;

  int frame_len;  // Frame size in bytes.

  const char* key_id;  // Crypto key id.
  size_t key_id_len;   // Length of the crypto key id.
  const char* iv;      // Crypto initialization vector.
  size_t iv_len;       // Length of the initialization vector.

  size_t subsample_count;  // Number of subsamples.

  // Arrays of subsample_count size.
  // Note: In each subsample, clear bytes precede encrypted bytes.
  const int* clear_bytes;  // Number of clear bytes in each subsample.
  const int* enc_bytes;    // Number of encrypted bytes in each subsample.

  size_t width;
  size_t height;
  // TODO(rdaum): Add timebase and timeline position.
};

// Copy the bytes for a frame from a loaded player, populating 'buffer' up to
// 'bufferlen', with the associated frame information set into the DashFrameInfo
// at in 'frame_info'.
// Returns the number of bytes copied into the buffer from this call.  Returns
// -1 if no frame was available. Note the PTS values set in FrameInfo are always
// relative to a start time of 0. This is to ensure the 90khz PTS values first
// reported will always fit in 33 bits. It is the responsibility of the decoder
// to detect PTS rollover and adjust the media time reported back to DashThread
// via DecoderGetMediaTimeFunc callback.
NDASH_EXPORT int ndash_copy_frame(struct ndash_handle* handle,
                                  void* buffer,
                                  size_t buffer_len,
                                  struct DashFrameInfo* frame_info);

// Get the first media time (milliseconds) available in the stream.
// This value is subtracted from pts values before returned by ndash and
// added to values passed into ndash.  It effectively shifts the master time
// line down so that media appears to start at time 0 even though the
// first period may not. This is done to guarantee the decoder has the most
// play time possible before having to deal with pts rollover.
NDASH_EXPORT MediaTimeMs ndash_get_first_time(struct ndash_handle* handle);

// Get the duration (milliseconds) available from the stream.
NDASH_EXPORT MediaDurationMs ndash_get_duration(struct ndash_handle* handle);

// Returns 1 if the player is at the end of the stream.
NDASH_EXPORT int ndash_is_eos(struct ndash_handle* handle);

// Seek to position 'time' (milliseconds) in the player stream.
// Returns 0 on success.
// TODO(rdaum): Must change the return code inside dash_thread.cc to be 0 for
// success to be consistent with ndash_load.
NDASH_EXPORT int ndash_seek(struct ndash_handle* handle, MediaTimeMs time);

NDASH_EXPORT int ndash_set_playback_rate(struct ndash_handle* handle,
                                         float rate);

// Synchronously obtain a playback license from the license server.
// message_key_blob : The key message blob constructed by the CDM
// message_key_blob_len : The number of bytes in the message blob.
// license : Upon success, will point to the returned license data. It is up
//           to the caller to release the memory using free().
// license_len : Upon success, will be the length of the license data.
// Returns 0 on success, any other value indicates error and the contents of
// license or license_len is undefined.
NDASH_EXPORT int ndash_make_license_request(struct ndash_handle* handle,
                                            void* message_key_blob,
                                            size_t message_key_blob_len,
                                            void** license,
                                            size_t* license_len);

typedef enum {
  DASH_STREAM_STATE_BUFFERING,
  DASH_STREAM_STATE_PLAYING,
  DASH_STREAM_STATE_PAUSED,
  DASH_STREAM_STATE_SEEKING
} DashStreamState;

NDASH_EXPORT void ndash_report_playback_state(struct ndash_handle* handle,
                                              DashStreamState state);

// Potential client playback errors.
typedef enum {
  DASH_VIDEO_UNKNOWN_ERROR,
  DASH_VIDEO_MEDIA_PLAYER_AUDIO_INIT_ERROR,
  DASH_VIDEO_MEDIA_PLAYER_VIDEO_INIT_ERROR,
  DASH_VIDEO_MEDIA_PLAYER_PLAYBACK_ERROR,
  DASH_VIDEO_MEDIA_DRM_ERROR,
} DashPlaybackErrorCode;

NDASH_EXPORT void ndash_report_playback_error(struct ndash_handle* handle,
                                              DashPlaybackErrorCode code,
                                              const char* details_msg,
                                              int is_fatal);

// TODO(rdaum): Document available attributes and semantics.
// Returns 0 on success, otherwise a failure occurred.
NDASH_EXPORT int ndash_set_attribute(struct ndash_handle* handle,
                                     const char* attribute_name,
                                     const char* attribute_value);

#ifdef __cplusplus
}
#endif

#endif  // NDASH_H_
