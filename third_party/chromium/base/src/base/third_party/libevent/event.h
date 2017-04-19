// The Chromium build contains its own checkout of libevent. This stub is used
// when building the Chrome OS or Android libchrome package to instead use the
// system headers.
#if defined(__ANDROID__) || defined(__ANDROID_HOST__)
#include <event2/event.h>
#include <event2/event_compat.h>
#include <event2/event_struct.h>
#else
#include <event.h>
#endif
