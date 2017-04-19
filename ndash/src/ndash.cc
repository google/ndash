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

#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "dash_thread.h"
#include "ndash.h"

struct ndash_handle {
  ndash::DashThread* dash_thread;
};

namespace {
const char kLoggingLevel[] = "log-level";
base::AtExitManager* exit_manager;
bool checked_switches = false;
}  // namespace

ndash_handle* ndash_create(DashPlayerCallbacks* callbacks, void* context) {
  if (!exit_manager)
    exit_manager = new base::AtExitManager;

  if (!checked_switches) {
    checked_switches = true;
    // Place switches in /tmp/dash_args to override log level or vlog level
    // Example: --log-level=0 --v=2
    std::ifstream infile("/tmp/dash_args");
    int ac = 0;
    std::vector<const char*> arg_ptrs;
    std::vector<std::string> args;
    if (infile.is_open()) {
      std::string dash_args;
      std::getline(infile, dash_args);

      // Construct an args array from dash_args using space as a separator.
      args.push_back("mcnmp_server");
      arg_ptrs.push_back(args.back().c_str());

      size_t i;
      do {
        i = dash_args.find(" ");
        std::string arg;
        if (i != std::string::npos) {
          arg = dash_args.substr(0, i);
        } else {
          arg = dash_args;
        }
        args.push_back(arg);
        arg_ptrs.push_back(args.back().c_str());
        dash_args = dash_args.substr(i + 1);
      } while (i != std::string::npos);

      ac = arg_ptrs.size();
    }

    base::CommandLine::Init(ac, arg_ptrs.data());
    logging::LoggingSettings settings;
    logging::InitLogging(settings);

    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();

    if (command_line && command_line->HasSwitch(kLoggingLevel)) {
      std::string log_level = command_line->GetSwitchValueASCII(kLoggingLevel);
      int level = 0;
      if (base::StringToInt(log_level, &level) && level >= 0 &&
          level < logging::LOG_NUM_SEVERITIES) {
        logging::SetMinLogLevel(level);
      } else {
        LOG(WARNING) << "Bad log level: " << log_level;
      }
    }
  }

  ndash::DashThread* player = new ndash::DashThread("dash_thread", context);
  player->SetPlayerCallbacks(callbacks);
  player->Start();
  return new ndash_handle{player};
}

void ndash_set_callbacks(struct ndash_handle* handle,
                         DashPlayerCallbacks* callbacks) {
  if (handle && handle->dash_thread) {
    handle->dash_thread->SetPlayerCallbacks(callbacks);
  }
}

NDASH_EXPORT void ndash_set_context(struct ndash_handle* handle,
                                    void* context) {
  if (handle && handle->dash_thread) {
    handle->dash_thread->SetPlayerCallbackContext(context);
  }
}

void ndash_destroy(ndash_handle* handle) {
  if (handle && handle->dash_thread) {
    delete handle->dash_thread;
    delete handle;
  }

  if (exit_manager) {
    delete exit_manager;
    exit_manager = nullptr;
  }
}

int ndash_load(ndash_handle* handle,
               const char* url,
               // TODO(rdaum): Switch to MediaTime*?
               float initial_time_sec) {
  if (handle && handle->dash_thread) {
    return handle->dash_thread->Load(url, initial_time_sec) ? 0 : -1;
  }
  return 1;
}

void ndash_unload(ndash_handle* handle) {
  if (handle && handle->dash_thread) {
    handle->dash_thread->Unload();
  }
}

int ndash_get_audio_codec_settings(ndash_handle* handle,
                                   DashAudioCodecSettings* codec_settings) {
  if (handle && handle->dash_thread) {
    return ndash::DashThread::GetAudioCodecSettings(handle->dash_thread,
                                                    codec_settings);
  }
  return 1;
}

int ndash_get_video_codec_settings(ndash_handle* handle,
                                   DashVideoCodecSettings* codec_settings) {
  if (handle && handle->dash_thread) {
    return ndash::DashThread::GetVideoCodecSettings(handle->dash_thread,
                                                    codec_settings);
  }
  return 1;
}

int ndash_is_eos(ndash_handle* handle) {
  if (handle && handle->dash_thread) {
    return handle->dash_thread->IsEOS();
  }
  return 1;
}

int ndash_set_playback_rate(struct ndash_handle* handle, float rate) {
  if (handle && handle->dash_thread) {
    handle->dash_thread->SetPlaybackRate(rate);
  }
  return 1;
}

int ndash_seek(struct ndash_handle* handle, MediaTimeMs time) {
  if (handle && handle->dash_thread) {
    return ndash::DashThread::Seek(handle->dash_thread, time);
  }
  return 1;
}

int ndash_copy_frame(struct ndash_handle* handle,
                     void* buffer,
                     size_t buffer_len,
                     struct DashFrameInfo* frame_info) {
  if (handle && handle->dash_thread) {
    return ndash::DashThread::CopyFrame(handle->dash_thread, buffer, buffer_len,
                                        frame_info);
  }
  return -1;
}

int ndash_makeLicenseRequest(struct ndash_handle* handle,
                             const char* message_key_blob,
                             size_t message_key_blob_len,
                             char** license,
                             size_t* license_len) {
  if (handle && handle->dash_thread) {
    std::string key_blob(message_key_blob, message_key_blob_len);
    std::string response;
    if (handle->dash_thread->MakeLicenseRequest(key_blob, &response)) {
      *license = static_cast<char*>(malloc(response.size()));
      *license_len = response.size();
      memcpy(*license, response.c_str(), response.length());
      return 0;
    }
  }
  return -1;
}

void ndash_report_playback_state(ndash_handle* handle, DashStreamState state) {
  if (handle && handle->dash_thread) {
    handle->dash_thread->ReportPlaybackState(state);
  }
}

void ndash_report_playback_error(ndash_handle* handle,
                                 DashPlaybackErrorCode code,
                                 const char* details,
                                 int is_fatal) {
  if (handle && handle->dash_thread) {
    handle->dash_thread->ReportPlaybackError(code, details, is_fatal != 0);
  }
}

int ndash_set_attribute(ndash_handle* handle,
                        const char* attribute_name,
                        const char* attribute_value) {
  if (handle && handle->dash_thread) {
    std::string attr_name(attribute_name);
    std::string attr_value(attribute_value);
    return handle->dash_thread->SetAttribute(attribute_name, attribute_value)
               ? 0
               : -1;
  }
  return -1;
}

int ndash_get_cc_codec_settings(ndash_handle* handle,
                                DashCCCodecSettings* codec_settings) {
  if (handle && handle->dash_thread) {
    return ndash::DashThread::GetCCCodecSettings(handle->dash_thread,
                                                 codec_settings);
  }
  return -1;
}

MediaTimeMs ndash_get_first_time(ndash_handle* handle) {
  if (handle && handle->dash_thread) {
    return ndash::DashThread::GetFirstTime(handle->dash_thread);
  }
  return -1;
}

MediaDurationMs ndash_get_duration(ndash_handle* handle) {
  if (handle && handle->dash_thread) {
    return ndash::DashThread::GetDurationMs(handle->dash_thread);
  }
  return -1;
}
