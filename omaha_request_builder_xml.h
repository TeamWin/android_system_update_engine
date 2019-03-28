//
// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef UPDATE_ENGINE_OMAHA_REQUEST_BUILDER_XML_H_
#define UPDATE_ENGINE_OMAHA_REQUEST_BUILDER_XML_H_

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include <brillo/secure_blob.h>
#include <curl/curl.h>

#include "update_engine/common/action.h"
#include "update_engine/common/http_fetcher.h"
#include "update_engine/omaha_response.h"
#include "update_engine/system_state.h"

// TODO(ahassani): Make the xml builder into a class of its own so we don't have
// to pass all these parameters around.

namespace chromeos_update_engine {

extern const int kNeverPinged;

// This struct encapsulates the Omaha event information. For a
// complete list of defined event types and results, see
// http://code.google.com/p/omaha/wiki/ServerProtocol#event
struct OmahaEvent {
  // The Type values correspond to EVENT_TYPE values of Omaha.
  enum Type {
    kTypeUnknown = 0,
    kTypeDownloadComplete = 1,
    kTypeInstallComplete = 2,
    kTypeUpdateComplete = 3,
    kTypeUpdateDownloadStarted = 13,
    kTypeUpdateDownloadFinished = 14,
    // Chromium OS reserved type sent after the first reboot following an update
    // completed.
    kTypeRebootedAfterUpdate = 54,
  };

  // The Result values correspond to EVENT_RESULT values of Omaha.
  enum Result {
    kResultError = 0,
    kResultSuccess = 1,
    kResultUpdateDeferred = 9,  // When we ignore/defer updates due to policy.
  };

  OmahaEvent()
      : type(kTypeUnknown),
        result(kResultError),
        error_code(ErrorCode::kError) {}
  explicit OmahaEvent(Type in_type)
      : type(in_type),
        result(kResultSuccess),
        error_code(ErrorCode::kSuccess) {}
  OmahaEvent(Type in_type, Result in_result, ErrorCode in_error_code)
      : type(in_type), result(in_result), error_code(in_error_code) {}

  Type type;
  Result result;
  ErrorCode error_code;
};

struct OmahaAppData {
  std::string id;
  std::string version;
  std::string product_components;
};

// Encodes XML entities in a given string. Input must be ASCII-7 valid. If
// the input is invalid, the default value is used instead.
std::string XmlEncodeWithDefault(const std::string& input,
                                 const std::string& default_value);

// Escapes text so it can be included as character data and attribute
// values. The |input| string must be valid ASCII-7, no UTF-8 supported.
// Returns whether the |input| was valid and escaped properly in |output|.
bool XmlEncode(const std::string& input, std::string* output);

// Returns an XML ping element attribute assignment with attribute
// |name| and value |ping_days| if |ping_days| has a value that needs
// to be sent, or an empty string otherwise.
std::string GetPingAttribute(const std::string& name, int ping_days);

// Returns an XML ping element if any of the elapsed days need to be
// sent, or an empty string otherwise.
std::string GetPingXml(int ping_active_days, int ping_roll_call_days);

// Returns an XML that goes into the body of the <app> element of the Omaha
// request based on the given parameters.
std::string GetAppBody(const OmahaEvent* event,
                       OmahaRequestParams* params,
                       bool ping_only,
                       bool include_ping,
                       bool skip_updatecheck,
                       int ping_active_days,
                       int ping_roll_call_days,
                       PrefsInterface* prefs);

// Returns the cohort* argument to include in the <app> tag for the passed
// |arg_name| and |prefs_key|, if any. The return value is suitable to
// concatenate to the list of arguments and includes a space at the end.
std::string GetCohortArgXml(PrefsInterface* prefs,
                            const std::string arg_name,
                            const std::string prefs_key);

bool IsValidComponentID(const std::string& id);

// Returns an XML that corresponds to the entire <app> node of the Omaha
// request based on the given parameters.
std::string GetAppXml(const OmahaEvent* event,
                      OmahaRequestParams* params,
                      const OmahaAppData& app_data,
                      bool ping_only,
                      bool include_ping,
                      bool skip_updatecheck,
                      int ping_active_days,
                      int ping_roll_call_days,
                      int install_date_in_days,
                      SystemState* system_state);

// Returns an XML that corresponds to the entire <os> node of the Omaha
// request based on the given parameters.
std::string GetOsXml(OmahaRequestParams* params);

// Returns an XML that corresponds to the entire Omaha request based on the
// given parameters.
std::string GetRequestXml(const OmahaEvent* event,
                          OmahaRequestParams* params,
                          bool ping_only,
                          bool include_ping,
                          int ping_active_days,
                          int ping_roll_call_days,
                          int install_date_in_days,
                          SystemState* system_state);

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_OMAHA_REQUEST_BUILDER_XML_H_
