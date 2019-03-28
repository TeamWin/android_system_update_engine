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

#include "update_engine/omaha_request_builder_xml.h"

#include <inttypes.h>

#include <string>

#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <base/time/time.h>

#include "update_engine/common/constants.h"
#include "update_engine/common/prefs_interface.h"
#include "update_engine/common/utils.h"
#include "update_engine/omaha_request_params.h"

using std::string;

namespace chromeos_update_engine {

const int kNeverPinged = -1;

bool XmlEncode(const string& input, string* output) {
  if (std::find_if(input.begin(), input.end(), [](const char c) {
        return c & 0x80;
      }) != input.end()) {
    LOG(WARNING) << "Invalid ASCII-7 string passed to the XML encoder:";
    utils::HexDumpString(input);
    return false;
  }
  output->clear();
  // We need at least input.size() space in the output, but the code below will
  // handle it if we need more.
  output->reserve(input.size());
  for (char c : input) {
    switch (c) {
      case '\"':
        output->append("&quot;");
        break;
      case '\'':
        output->append("&apos;");
        break;
      case '&':
        output->append("&amp;");
        break;
      case '<':
        output->append("&lt;");
        break;
      case '>':
        output->append("&gt;");
        break;
      default:
        output->push_back(c);
    }
  }
  return true;
}

string XmlEncodeWithDefault(const string& input, const string& default_value) {
  string output;
  if (XmlEncode(input, &output))
    return output;
  return default_value;
}

string GetPingAttribute(const string& name, int ping_days) {
  if (ping_days > 0 || ping_days == kNeverPinged)
    return base::StringPrintf(" %s=\"%d\"", name.c_str(), ping_days);
  return "";
}

string GetPingXml(int ping_active_days, int ping_roll_call_days) {
  string ping_active = GetPingAttribute("a", ping_active_days);
  string ping_roll_call = GetPingAttribute("r", ping_roll_call_days);
  if (!ping_active.empty() || !ping_roll_call.empty()) {
    return base::StringPrintf("        <ping active=\"1\"%s%s></ping>\n",
                              ping_active.c_str(),
                              ping_roll_call.c_str());
  }
  return "";
}

string GetAppBody(const OmahaEvent* event,
                  OmahaRequestParams* params,
                  bool ping_only,
                  bool include_ping,
                  bool skip_updatecheck,
                  int ping_active_days,
                  int ping_roll_call_days,
                  PrefsInterface* prefs) {
  string app_body;
  if (event == nullptr) {
    if (include_ping)
      app_body = GetPingXml(ping_active_days, ping_roll_call_days);
    if (!ping_only) {
      if (!skip_updatecheck) {
        app_body += "        <updatecheck";
        if (!params->target_version_prefix().empty()) {
          app_body += base::StringPrintf(
              " targetversionprefix=\"%s\"",
              XmlEncodeWithDefault(params->target_version_prefix(), "")
                  .c_str());
          // Rollback requires target_version_prefix set.
          if (params->rollback_allowed()) {
            app_body += " rollback_allowed=\"true\"";
          }
        }
        app_body += "></updatecheck>\n";
      }

      // If this is the first update check after a reboot following a previous
      // update, generate an event containing the previous version number. If
      // the previous version preference file doesn't exist the event is still
      // generated with a previous version of 0.0.0.0 -- this is relevant for
      // older clients or new installs. The previous version event is not sent
      // for ping-only requests because they come before the client has
      // rebooted. The previous version event is also not sent if it was already
      // sent for this new version with a previous updatecheck.
      string prev_version;
      if (!prefs->GetString(kPrefsPreviousVersion, &prev_version)) {
        prev_version = "0.0.0.0";
      }
      // We only store a non-empty previous version value after a successful
      // update in the previous boot. After reporting it back to the server,
      // we clear the previous version value so it doesn't get reported again.
      if (!prev_version.empty()) {
        app_body += base::StringPrintf(
            "        <event eventtype=\"%d\" eventresult=\"%d\" "
            "previousversion=\"%s\"></event>\n",
            OmahaEvent::kTypeRebootedAfterUpdate,
            OmahaEvent::kResultSuccess,
            XmlEncodeWithDefault(prev_version, "0.0.0.0").c_str());
        LOG_IF(WARNING, !prefs->SetString(kPrefsPreviousVersion, ""))
            << "Unable to reset the previous version.";
      }
    }
  } else {
    // The error code is an optional attribute so append it only if the result
    // is not success.
    string error_code;
    if (event->result != OmahaEvent::kResultSuccess) {
      error_code = base::StringPrintf(" errorcode=\"%d\"",
                                      static_cast<int>(event->error_code));
    }
    app_body = base::StringPrintf(
        "        <event eventtype=\"%d\" eventresult=\"%d\"%s></event>\n",
        event->type,
        event->result,
        error_code.c_str());
  }

  return app_body;
}

string GetCohortArgXml(PrefsInterface* prefs,
                       const string arg_name,
                       const string prefs_key) {
  // There's nothing wrong with not having a given cohort setting, so we check
  // existence first to avoid the warning log message.
  if (!prefs->Exists(prefs_key))
    return "";
  string cohort_value;
  if (!prefs->GetString(prefs_key, &cohort_value) || cohort_value.empty())
    return "";
  // This is a sanity check to avoid sending a huge XML file back to Ohama due
  // to a compromised stateful partition making the update check fail in low
  // network environments envent after a reboot.
  if (cohort_value.size() > 1024) {
    LOG(WARNING) << "The omaha cohort setting " << arg_name
                 << " has a too big value, which must be an error or an "
                    "attacker trying to inhibit updates.";
    return "";
  }

  string escaped_xml_value;
  if (!XmlEncode(cohort_value, &escaped_xml_value)) {
    LOG(WARNING) << "The omaha cohort setting " << arg_name
                 << " is ASCII-7 invalid, ignoring it.";
    return "";
  }

  return base::StringPrintf(
      "%s=\"%s\" ", arg_name.c_str(), escaped_xml_value.c_str());
}

bool IsValidComponentID(const string& id) {
  for (char c : id) {
    if (!isalnum(c) && c != '-' && c != '_' && c != '.')
      return false;
  }
  return true;
}

string GetAppXml(const OmahaEvent* event,
                 OmahaRequestParams* params,
                 const OmahaAppData& app_data,
                 bool ping_only,
                 bool include_ping,
                 bool skip_updatecheck,
                 int ping_active_days,
                 int ping_roll_call_days,
                 int install_date_in_days,
                 SystemState* system_state) {
  string app_body = GetAppBody(event,
                               params,
                               ping_only,
                               include_ping,
                               skip_updatecheck,
                               ping_active_days,
                               ping_roll_call_days,
                               system_state->prefs());
  string app_versions;

  // If we are downgrading to a more stable channel and we are allowed to do
  // powerwash, then pass 0.0.0.0 as the version. This is needed to get the
  // highest-versioned payload on the destination channel.
  if (params->ShouldPowerwash()) {
    LOG(INFO) << "Passing OS version as 0.0.0.0 as we are set to powerwash "
              << "on downgrading to the version in the more stable channel";
    app_versions = "version=\"0.0.0.0\" from_version=\"" +
                   XmlEncodeWithDefault(app_data.version, "0.0.0.0") + "\" ";
  } else {
    app_versions = "version=\"" +
                   XmlEncodeWithDefault(app_data.version, "0.0.0.0") + "\" ";
  }

  string download_channel = params->download_channel();
  string app_channels =
      "track=\"" + XmlEncodeWithDefault(download_channel, "") + "\" ";
  if (params->current_channel() != download_channel) {
    app_channels += "from_track=\"" +
                    XmlEncodeWithDefault(params->current_channel(), "") + "\" ";
  }

  string delta_okay_str = params->delta_okay() ? "true" : "false";

  // If install_date_days is not set (e.g. its value is -1 ), don't
  // include the attribute.
  string install_date_in_days_str = "";
  if (install_date_in_days >= 0) {
    install_date_in_days_str =
        base::StringPrintf("installdate=\"%d\" ", install_date_in_days);
  }

  string app_cohort_args;
  app_cohort_args +=
      GetCohortArgXml(system_state->prefs(), "cohort", kPrefsOmahaCohort);
  app_cohort_args += GetCohortArgXml(
      system_state->prefs(), "cohorthint", kPrefsOmahaCohortHint);
  app_cohort_args += GetCohortArgXml(
      system_state->prefs(), "cohortname", kPrefsOmahaCohortName);

  string fingerprint_arg;
  if (!params->os_build_fingerprint().empty()) {
    fingerprint_arg = "fingerprint=\"" +
                      XmlEncodeWithDefault(params->os_build_fingerprint(), "") +
                      "\" ";
  }

  string buildtype_arg;
  if (!params->os_build_type().empty()) {
    buildtype_arg = "os_build_type=\"" +
                    XmlEncodeWithDefault(params->os_build_type(), "") + "\" ";
  }

  string product_components_args;
  if (!params->ShouldPowerwash() && !app_data.product_components.empty()) {
    brillo::KeyValueStore store;
    if (store.LoadFromString(app_data.product_components)) {
      for (const string& key : store.GetKeys()) {
        if (!IsValidComponentID(key)) {
          LOG(ERROR) << "Invalid component id: " << key;
          continue;
        }
        string version;
        if (!store.GetString(key, &version)) {
          LOG(ERROR) << "Failed to get version for " << key
                     << " in product_components.";
          continue;
        }
        product_components_args +=
            base::StringPrintf("_%s.version=\"%s\" ",
                               key.c_str(),
                               XmlEncodeWithDefault(version, "").c_str());
      }
    } else {
      LOG(ERROR) << "Failed to parse product_components:\n"
                 << app_data.product_components;
    }
  }

  // clang-format off
  string app_xml = "    <app "
      "appid=\"" + XmlEncodeWithDefault(app_data.id, "") + "\" " +
      app_cohort_args +
      app_versions +
      app_channels +
      product_components_args +
      fingerprint_arg +
      buildtype_arg +
      "lang=\"" + XmlEncodeWithDefault(params->app_lang(), "en-US") + "\" " +
      "board=\"" + XmlEncodeWithDefault(params->os_board(), "") + "\" " +
      "hardware_class=\"" + XmlEncodeWithDefault(params->hwid(), "") + "\" " +
      "delta_okay=\"" + delta_okay_str + "\" "
      "fw_version=\"" + XmlEncodeWithDefault(params->fw_version(), "") + "\" " +
      "ec_version=\"" + XmlEncodeWithDefault(params->ec_version(), "") + "\" " +
      install_date_in_days_str +
      ">\n" +
         app_body +
      "    </app>\n";
  // clang-format on
  return app_xml;
}

string GetOsXml(OmahaRequestParams* params) {
  string os_xml =
      "    <os "
      "version=\"" +
      XmlEncodeWithDefault(params->os_version(), "") + "\" " + "platform=\"" +
      XmlEncodeWithDefault(params->os_platform(), "") + "\" " + "sp=\"" +
      XmlEncodeWithDefault(params->os_sp(), "") +
      "\">"
      "</os>\n";
  return os_xml;
}

string GetRequestXml(const OmahaEvent* event,
                     OmahaRequestParams* params,
                     bool ping_only,
                     bool include_ping,
                     int ping_active_days,
                     int ping_roll_call_days,
                     int install_date_in_days,
                     SystemState* system_state) {
  string os_xml = GetOsXml(params);
  OmahaAppData product_app = {
      .id = params->GetAppId(),
      .version = params->app_version(),
      .product_components = params->product_components()};
  // Skips updatecheck for platform app in case of an install operation.
  string app_xml = GetAppXml(event,
                             params,
                             product_app,
                             ping_only,
                             include_ping,
                             params->is_install(), /* skip_updatecheck */
                             ping_active_days,
                             ping_roll_call_days,
                             install_date_in_days,
                             system_state);
  if (!params->system_app_id().empty()) {
    OmahaAppData system_app = {.id = params->system_app_id(),
                               .version = params->system_version()};
    app_xml += GetAppXml(event,
                         params,
                         system_app,
                         ping_only,
                         include_ping,
                         false, /* skip_updatecheck */
                         ping_active_days,
                         ping_roll_call_days,
                         install_date_in_days,
                         system_state);
  }
  // Create APP ID according to |dlc_module_id| (sticking the current AppID to
  // the DLC module ID with an underscode).
  for (const auto& dlc_module_id : params->dlc_module_ids()) {
    OmahaAppData dlc_module_app = {
        .id = params->GetAppId() + "_" + dlc_module_id,
        .version = params->app_version()};
    app_xml += GetAppXml(event,
                         params,
                         dlc_module_app,
                         ping_only,
                         include_ping,
                         false, /* skip_updatecheck */
                         ping_active_days,
                         ping_roll_call_days,
                         install_date_in_days,
                         system_state);
  }

  string request_xml = base::StringPrintf(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<request protocol=\"3.0\" updater=\"%s\" updaterversion=\"%s\""
      " installsource=\"%s\" ismachine=\"1\">\n%s%s</request>\n",
      constants::kOmahaUpdaterID,
      kOmahaUpdaterVersion,
      params->interactive() ? "ondemandupdate" : "scheduler",
      os_xml.c_str(),
      app_xml.c_str());

  return request_xml;
}

}  // namespace chromeos_update_engine
