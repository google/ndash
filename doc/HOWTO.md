Using libndash
==============

## Summary

The overall structure of a media player written on top of libndash is as follows (some arguments omitted for brevity):

### Player's thread:

    ndash_create()
    ndash_set_attribute() (as necessary)
    ndash_load(url)

    ndash_get_video_codec_settings()
    ndash_get_audio_codec_settings()
    ndash_get_cc_codec_settings()

    while (!ndash_is_eos())
        upon user key detect:
            ndash_seek()/ndash_set_playback_rate()/break
        ndash_copy_frame()
        render frame (or enqueue frame for rendering)

    ndash_unload()
    ndash_destroy()

### Called by ndash's event thread:

The player should implement the following callbacks defined by ndash.h:

    DashPlayerGetMediaTimeFunc()
    DashPlayerFlushFunc()
    DashPlayerFetchLicenseFunc()
    DashPlayerOpenCdmSessionFunc()
    DashPlayerCloseCdmSessionFunc()

## Function Details

`ndash_create()`

   This will provide a handle to a new ndash instance. Although it is possible to re-use an instance for different media, it is recommended you create a new instance for each media stream you want to play.  This will create ndash's main event loop. However, no callbacks will be invoked until after you call ndash_load().

`ndash_set_attribute()`

   Calling this function lets you configure some parameters governing ndash's
behavior.

`ndash_load(url)`

   This points the ndash instance to a DASH manifest, typically hosted by an HTTP server.  When this call returns, libndash will have downloaded the manifest, parsed it and begun to download segments. It will have the video/audio/cc codec settings ready for you to discover.

`ndash_get_video_codec_settings()`
`ndash_get_audio_codec_settings()`
`ndash_get_cc_codec_settings()`

   Use these calls to discover the video/audio/cc codec settings necessary to initialize your decoders.

`ndash_seek()`

   Use this call to seek the media time to a given time.  This value should not exceed the duration (from ndash_duration()), nor should it fall below 0. All times provided by libndash and sent to libndash should be relative to a media start time of 0 (The master timeline of the media is always shifted down to start at 0 by the value returned by ndash_get_first_time).

   If the seek requests was accepted, the player can expect a DashPlayerFlushFunc() callback. Seek requests that are too close to the current position are ignored as are seeks outside the available range of the media.  It is important to note that a seek request may cancel in-flight HTTP requests and delay the seek completion until after those requests have been successfully canceled.

`ndash_set_playback_rate()`

   Use this call to change the playback rate.  Both positive and negative values are acceptable.  However, |rates| above 4 will typically require special trick tracks with maxPlayoutRate attributes at or above the rates you wish to play. These tracks are typically generated with only I-frames. See tools/streamgen for an example. Currently, any rate other than 1 will turn off the audio track but this may change in the future (to support a time-stretch/time-crunch feature for example).

   It is typically not possible to play regular video tracks (maxPlayoutRate=1) at high rates since a) there are too many frames to render that quickly and b) the HTTP fetcher cannot fetch segments fast enough from the source to keep up with those rates. This is why special trick tracks must be created whereby key-frames are extracted with long durations. Each segment of a trick track represents longer and longer periods as the maxPlayoutRate increases. Trick tracks can be generated such that when played at their  maxPlayoutRate, approximately 30fps are rendered.  However, this is not a requirement.

   If a playback rate change is accepted, the player can expect a DashPlayerFlushFunc() call. It is important to note that a rate change request may cancel in-flight HTTP requests and delay the rate change's completion until after those requests have been successfully canceled.

`ndash_is_eos()`

   Use this method to detect when libndash has reached the end of the stream.

`ndash_copy_frame()`

   libndash will fetch segments, parse them and produce audio/video frames that may be dequeued using this function. Audio/video/cc frames are interleaved and libndash does its best to ensure your audio/video/cc queues are not starved for data. However, it is possible calling this method returns no frame data.

   When both audio/video tracks are available, it is expected the audio frame PTS values determine the current decoder position.  Otherwise, the video frame PTS values can be used. The player should make sure it only responds to DashPlayerGetMediaTimeFunc callbacks with valid decoder positions.  That is, only return values from successfully decoded frames.

   In practice, it is most likely the case that the player will have an intermediate buffer or queue between the ndash_CopyFrame and the render step.  The DashPlayerFlush() callback should clear out those intermediate structures and any calls received by DashPlayerGetMediaTimeFunc implementation should return -1 until at least one valid PTS is discovered from a decoded frame.

   PTS values specified in the FrameInfo structure are in the time-base specified (typically 90khz for DASH streams). The PTS values from the raw media have been translated into a master timeline and have been shifted so the first frame is at or near 0. Currently, FrameInfo does not currently provide the raw PTS value parsed from the frame.

`ndash_unload()`

   Calling this method will cancel any outstanding HTTP segment fetch requests and release most resources.

`ndash_destroy()`

   Destroys the ndash instance. It can no longer be used after this method is called.

## DashPlayerCallbacks Details

`DashPlayerGetMediaTimeFunc()`

   The player should report a value (in milliseconds) that represents the time closest to the most recent frame actually rendered.  libndash uses this value to determine whether it should apply read-ahead logic in order to keep its frame queues full.  It is important the player not report 'old' values so as to clobber a new position libndash may have set as the result of a seek or rate change.  If a flush callback is invoked, the player should hold off on reporting any values until after it has successfully decoded the next frame.

`DashPlayerFlushFunc()`

   This callback is invoked by libndash to request any intermediate buffers/queues that the rendering loop may be operating on should be emptied.  That is, libndash expects the next frame it delivers via ndash_copy_frame() to be the one that is rendered next.

`DashPlayerOpenCdmSessionFunc()`

   When libndash discovers a new PSSH box for which it knows there is no open CDM session, this callback is invoked.  The player should forward this on to the CDM to create a session.

`DashPlayerCloseCdmSessionFunc()`

   Called by libndash to close a previously opened session with the CDM.

`DashPlayerFetchLicenseFunc()`

   This callback is invoked by libndash to indicate it requires a playback license. It will provide the raw PSSH data and session id it previously opened with the CDM via ndashOpenCdmSessionFunc.  In processing the request to fetch a license, the player's CDM may use the convenience function `ndash_make_license_request` which performs a synchronous HTTP fetch to the license server url (set by ndash_set_attribute).

   libndash will request a license as soon as it encounters a PSSH for which it knows there is no open CDM session.  However, parsing and producing frames will not be delayed by processing that request unless frames actually require decrypting.  This allows license fetches to be processed concurrently while the first few seconds of un-encrypted media can still be played.  Only when the first encrypted frame is encountered will libndash block and wait for a license request to complete.

# Miscellaneous

Approximately every 400 milliseconds, the main thread will wake up and perform housekeeping tasks that include:

1. asking the client what it's current decoder position is (via DashPlayerGetMediaTimeFunc)
2. deciding whether to fetch more segments to keep its frame queues full
3. apply adaptive bit-rate logic to select which representations should produce media

Most client requests are serviced by ndash's main event loop, not the player's calling thread. Internally, HTTP fetch operations and parsing are run on dedicated I/O threads which may be interrupted (by seek, rate change, unload calls for example).

