// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mp4/media_log.h"

#include <iostream>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/json/json_writer.h"
#include "base/values.h"

namespace media {

// A count of all MediaLogs created in the current process. Used to generate
// unique IDs.
static base::StaticAtomicSequenceNumber g_media_log_count;

std::string MediaLog::MediaLogLevelToString(MediaLogLevel level) {
  switch (level) {
    case MEDIALOG_ERROR:
      return "error";
    case MEDIALOG_INFO:
      return "info";
    case MEDIALOG_DEBUG:
      return "debug";
  }
  NOTREACHED();
  return NULL;
}

MediaLogEvent::Type MediaLog::MediaLogLevelToEventType(MediaLogLevel level) {
  switch (level) {
    case MEDIALOG_ERROR:
      return MediaLogEvent::MEDIA_ERROR_LOG_ENTRY;
    case MEDIALOG_INFO:
      return MediaLogEvent::MEDIA_INFO_LOG_ENTRY;
    case MEDIALOG_DEBUG:
      return MediaLogEvent::MEDIA_DEBUG_LOG_ENTRY;
  }
  NOTREACHED();
  return MediaLogEvent::MEDIA_ERROR_LOG_ENTRY;
}

std::string MediaLog::EventTypeToString(MediaLogEvent::Type type) {
  switch (type) {
    case MediaLogEvent::WEBMEDIAPLAYER_CREATED:
      return "WEBMEDIAPLAYER_CREATED";
    case MediaLogEvent::WEBMEDIAPLAYER_DESTROYED:
      return "WEBMEDIAPLAYER_DESTROYED";
    case MediaLogEvent::LOAD:
      return "LOAD";
    case MediaLogEvent::SEEK:
      return "SEEK";
    case MediaLogEvent::PLAY:
      return "PLAY";
    case MediaLogEvent::PAUSE:
      return "PAUSE";
    case MediaLogEvent::VIDEO_SIZE_SET:
      return "VIDEO_SIZE_SET";
    case MediaLogEvent::ENDED:
      return "ENDED";
    case MediaLogEvent::TEXT_ENDED:
      return "TEXT_ENDED";
    case MediaLogEvent::BUFFERED_EXTENTS_CHANGED:
      return "BUFFERED_EXTENTS_CHANGED";
    case MediaLogEvent::MEDIA_ERROR_LOG_ENTRY:
      return "MEDIA_ERROR_LOG_ENTRY";
    case MediaLogEvent::MEDIA_INFO_LOG_ENTRY:
      return "MEDIA_INFO_LOG_ENTRY";
    case MediaLogEvent::MEDIA_DEBUG_LOG_ENTRY:
      return "MEDIA_DEBUG_LOG_ENTRY";
    case MediaLogEvent::PROPERTY_CHANGE:
      return "PROPERTY_CHANGE";
  }
  NOTREACHED();
  return NULL;
}

std::string MediaLog::MediaEventToLogString(const MediaLogEvent& event) {
  std::string params_json;
  base::JSONWriter::Write(event.params, &params_json);
  return EventTypeToString(event.type) + " " + params_json;
}

MediaLog::MediaLog() : id_(g_media_log_count.GetNext()) {}

MediaLog::~MediaLog() {}

void MediaLog::AddEvent(std::unique_ptr<MediaLogEvent> event) {
  std::cerr << MediaEventToLogString(*event) << std::endl;
}

std::string MediaLog::GetLastErrorMessage() {
  return "";
}

std::unique_ptr<MediaLogEvent> MediaLog::CreateEvent(MediaLogEvent::Type type) {
  std::unique_ptr<MediaLogEvent> event(new MediaLogEvent);
  event->id = id_;
  event->type = type;
  event->time = base::TimeTicks::Now();
  return event;
}

std::unique_ptr<MediaLogEvent> MediaLog::CreateBooleanEvent(
    MediaLogEvent::Type type,
    const std::string& property,
    bool value) {
  std::unique_ptr<MediaLogEvent> event(CreateEvent(type));
  event->params.SetBoolean(property, value);
  return event;
}

std::unique_ptr<MediaLogEvent> MediaLog::CreateStringEvent(
    MediaLogEvent::Type type,
    const std::string& property,
    const std::string& value) {
  std::unique_ptr<MediaLogEvent> event(CreateEvent(type));
  event->params.SetString(property, value);
  return event;
}

std::unique_ptr<MediaLogEvent> MediaLog::CreateTimeEvent(
    MediaLogEvent::Type type,
    const std::string& property,
    base::TimeDelta value) {
  std::unique_ptr<MediaLogEvent> event(CreateEvent(type));
  if (value.is_max())
    event->params.SetString(property, "unknown");
  else
    event->params.SetDouble(property, value.InSecondsF());
  return event;
}

std::unique_ptr<MediaLogEvent> MediaLog::CreateLoadEvent(
    const std::string& url) {
  std::unique_ptr<MediaLogEvent> event(CreateEvent(MediaLogEvent::LOAD));
  event->params.SetString("url", url);
  return event;
}

std::unique_ptr<MediaLogEvent> MediaLog::CreateSeekEvent(float seconds) {
  std::unique_ptr<MediaLogEvent> event(CreateEvent(MediaLogEvent::SEEK));
  event->params.SetDouble("seek_target", seconds);
  return event;
}

std::unique_ptr<MediaLogEvent> MediaLog::CreateVideoSizeSetEvent(
    size_t width,
    size_t height) {
  std::unique_ptr<MediaLogEvent> event(
      CreateEvent(MediaLogEvent::VIDEO_SIZE_SET));
  event->params.SetInteger("width", width);
  event->params.SetInteger("height", height);
  return event;
}

std::unique_ptr<MediaLogEvent> MediaLog::CreateBufferedExtentsChangedEvent(
    int64_t start,
    int64_t current,
    int64_t end) {
  std::unique_ptr<MediaLogEvent> event(
      CreateEvent(MediaLogEvent::BUFFERED_EXTENTS_CHANGED));
  // These values are headed to JS where there is no int64_t so we use a double
  // and accept loss of precision above 2^53 bytes (8 Exabytes).
  event->params.SetDouble("buffer_start", start);
  event->params.SetDouble("buffer_current", current);
  event->params.SetDouble("buffer_end", end);
  return event;
}

void MediaLog::AddLogEvent(MediaLogLevel level, const std::string& message) {
  std::unique_ptr<MediaLogEvent> event(
      CreateEvent(MediaLogLevelToEventType(level)));
  event->params.SetString(MediaLogLevelToString(level), message);
  AddEvent(std::move(event));
}

void MediaLog::SetStringProperty(const std::string& key,
                                 const std::string& value) {
  std::unique_ptr<MediaLogEvent> event(
      CreateEvent(MediaLogEvent::PROPERTY_CHANGE));
  event->params.SetString(key, value);
  AddEvent(std::move(event));
}

void MediaLog::SetDoubleProperty(const std::string& key, double value) {
  std::unique_ptr<MediaLogEvent> event(
      CreateEvent(MediaLogEvent::PROPERTY_CHANGE));
  event->params.SetDouble(key, value);
  AddEvent(std::move(event));
}

void MediaLog::SetBooleanProperty(const std::string& key, bool value) {
  std::unique_ptr<MediaLogEvent> event(
      CreateEvent(MediaLogEvent::PROPERTY_CHANGE));
  event->params.SetBoolean(key, value);
  AddEvent(std::move(event));
}

LogHelper::LogHelper(MediaLog::MediaLogLevel level,
                     const scoped_refptr<MediaLog>& media_log)
    : level_(level), media_log_(media_log) {
  DCHECK(media_log_.get());
}

LogHelper::~LogHelper() {
  media_log_->AddLogEvent(level_, stream_.str());
}

}  // namespace media
