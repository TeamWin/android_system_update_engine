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

#include "update_engine/cros/omaha_request_builder_xml.h"

#include <inttypes.h>

#include <string>

#include <base/guid.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <base/time/time.h>

#include "update_engine/common/constants.h"
#include "update_engine/common/system_state.h"
#include "update_engine/common/utils.h"
#include "update_engine/cros/omaha_request_params.h"

using std::string;

namespace chromeos_update_engine {

const char kNoVersion[] = "0.0.0.0";
const int kPingNeverPinged = -1;
const int kPingUnknownValue = -2;
const int kPingActiveValue = 1;
const int kPingInactiveValue = 0;

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

string OmahaRequestBuilderXml::GetPing() const {
  // Returns an XML ping element attribute assignment with attribute
  // |name| and value |ping_days| if |ping_days| has a value that needs
  // to be sent, or an empty string otherwise.
  auto GetPingAttribute = [](const char* name, int ping_days) -> string {
    if (ping_days > 0 || ping_days == kPingNeverPinged)
      return base::StringPrintf(" %s=\"%d\"", name, ping_days);
    return "";
  };

  string ping_active = GetPingAttribute("a", ping_active_days_);
  string ping_roll_call = GetPingAttribute("r", ping_roll_call_days_);
  if (!ping_active.empty() || !ping_roll_call.empty()) {
    return base::StringPrintf("        <ping active=\"1\"%s%s></ping>\n",
                              ping_active.c_str(),
                              ping_roll_call.c_str());
  }
  return "";
}

string OmahaRequestBuilderXml::GetPingDateBased(
    const OmahaRequestParams::AppParams& app_params) const {
  if (!app_params.send_ping)
    return "";
  string ping_active = "";
  string ping_ad = "";
  if (app_params.ping_active == kPingActiveValue) {
    ping_active =
        base::StringPrintf(" active=\"%" PRId64 "\"", app_params.ping_active);
    ping_ad = base::StringPrintf(" ad=\"%" PRId64 "\"",
                                 app_params.ping_date_last_active);
  }

  string ping_rd = base::StringPrintf(" rd=\"%" PRId64 "\"",
                                      app_params.ping_date_last_rollcall);

  return base::StringPrintf("        <ping%s%s%s></ping>\n",
                            ping_active.c_str(),
                            ping_ad.c_str(),
                            ping_rd.c_str());
}

string OmahaRequestBuilderXml::GetAppBody(const OmahaAppData& app_data) const {
  string app_body;
  if (event_ == nullptr) {
    if (app_data.app_params.send_ping) {
      switch (app_data.app_params.active_counting_type) {
        case OmahaRequestParams::kDayBased:
          app_body = GetPing();
          break;
        case OmahaRequestParams::kDateBased:
          app_body = GetPingDateBased(app_data.app_params);
          break;
        default:
          NOTREACHED();
      }
    }
    if (!ping_only_) {
      if (!app_data.skip_update) {
        const auto* params = SystemState::Get()->request_params();
        app_body += "        <updatecheck";
        if (!params->target_version_prefix().empty()) {
          app_body += base::StringPrintf(
              " targetversionprefix=\"%s\"",
              XmlEncodeWithDefault(params->target_version_prefix()).c_str());
          // Rollback requires target_version_prefix set.
          if (params->rollback_allowed()) {
            app_body += " rollback_allowed=\"true\"";
          }
        }
        if (!params->lts_tag().empty()) {
          app_body += base::StringPrintf(
              " ltstag=\"%s\"",
              XmlEncodeWithDefault(params->lts_tag()).c_str());
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
      auto* prefs = SystemState::Get()->prefs();
      string prev_version;
      if (!prefs->GetString(kPrefsPreviousVersion, &prev_version)) {
        prev_version = kNoVersion;
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
            XmlEncodeWithDefault(prev_version, kNoVersion).c_str());
        LOG_IF(WARNING, !prefs->SetString(kPrefsPreviousVersion, ""))
            << "Unable to reset the previous version.";
      }
    }
  } else {
    int event_result = event_->result;
    // The error code is an optional attribute so append it only if the result
    // is not success.
    string error_code;
    if (event_result != OmahaEvent::kResultSuccess) {
      error_code = base::StringPrintf(" errorcode=\"%d\"",
                                      static_cast<int>(event_->error_code));
    } else if (app_data.is_dlc && !app_data.app_params.updated) {
      // On a |OmahaEvent::kResultSuccess|, if the event is for an update
      // completion and the App is a DLC, send error for excluded DLCs as they
      // did not update.
      event_result = OmahaEvent::Result::kResultError;
      error_code = base::StringPrintf(
          " errorcode=\"%d\"",
          static_cast<int>(ErrorCode::kPackageExcludedFromUpdate));
    }
    app_body = base::StringPrintf(
        "        <event eventtype=\"%d\" eventresult=\"%d\"%s></event>\n",
        event_->type,
        event_result,
        error_code.c_str());
  }

  return app_body;
}

string OmahaRequestBuilderXml::GetCohortArg(
    const string& arg_name,
    const string& prefs_key,
    const string& override_value) const {
  string cohort_value;
  if (!override_value.empty()) {
    // |override_value| take precedence over pref value.
    cohort_value = override_value;
  } else {
    // There's nothing wrong with not having a given cohort setting, so we check
    // existence first to avoid the warning log message.
    const auto* prefs = SystemState::Get()->prefs();
    if (!prefs->Exists(prefs_key))
      return "";
    if (!prefs->GetString(prefs_key, &cohort_value) || cohort_value.empty())
      return "";
  }
  // This is a validity check to avoid sending a huge XML file back to Ohama due
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

string OmahaRequestBuilderXml::GetApp(const OmahaAppData& app_data) const {
  string app_body = GetAppBody(app_data);
  string app_versions;
  const auto* params = SystemState::Get()->request_params();

  // If we are downgrading to a more stable channel and we are allowed to do
  // powerwash, then pass 0.0.0.0 as the version. This is needed to get the
  // highest-versioned payload on the destination channel.
  if (params->ShouldPowerwash()) {
    LOG(INFO) << "Passing OS version as 0.0.0.0 as we are set to powerwash "
              << "on downgrading to the version in the more stable channel";
    app_versions = "version=\"" + string(kNoVersion) + "\" from_version=\"" +
                   XmlEncodeWithDefault(app_data.version, kNoVersion) + "\" ";
  } else {
    app_versions = "version=\"" +
                   XmlEncodeWithDefault(app_data.version, kNoVersion) + "\" ";
  }

  string download_channel = params->download_channel();
  string app_channels =
      "track=\"" + XmlEncodeWithDefault(download_channel) + "\" ";
  if (params->current_channel() != download_channel) {
    app_channels += "from_track=\"" +
                    XmlEncodeWithDefault(params->current_channel()) + "\" ";
  }

  string delta_okay_str =
      params->delta_okay() && !params->is_install() ? "true" : "false";

  // If install_date_days is not set (e.g. its value is -1 ), don't
  // include the attribute.
  string install_date_in_days_str = "";
  if (install_date_in_days_ >= 0) {
    install_date_in_days_str =
        base::StringPrintf("installdate=\"%d\" ", install_date_in_days_);
  }

  string app_cohort_args;
  string cohort_key = kPrefsOmahaCohort;
  string cohortname_key = kPrefsOmahaCohortName;
  string cohorthint_key = kPrefsOmahaCohortHint;

  // Override the cohort keys for DLC App IDs.
  const auto& dlc_apps_params = params->dlc_apps_params();
  auto itr = dlc_apps_params.find(app_data.id);
  if (itr != dlc_apps_params.end()) {
    auto dlc_id = itr->second.name;
    const auto* prefs = SystemState::Get()->prefs();
    cohort_key =
        prefs->CreateSubKey({kDlcPrefsSubDir, dlc_id, kPrefsOmahaCohort});
    cohortname_key =
        prefs->CreateSubKey({kDlcPrefsSubDir, dlc_id, kPrefsOmahaCohortName});
    cohorthint_key =
        prefs->CreateSubKey({kDlcPrefsSubDir, dlc_id, kPrefsOmahaCohortHint});
  }

  app_cohort_args += GetCohortArg("cohort", cohort_key);
  app_cohort_args += GetCohortArg("cohortname", cohortname_key);
  // Policy provided value overrides pref.
  app_cohort_args +=
      GetCohortArg("cohorthint",
                   cohorthint_key,
                   params->autoupdate_token() /* override_value */);

  string fingerprint_arg;
  if (!params->os_build_fingerprint().empty()) {
    fingerprint_arg = "fingerprint=\"" +
                      XmlEncodeWithDefault(params->os_build_fingerprint()) +
                      "\" ";
  }

  string buildtype_arg;
  if (!params->os_build_type().empty()) {
    buildtype_arg = "os_build_type=\"" +
                    XmlEncodeWithDefault(params->os_build_type()) + "\" ";
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
                               XmlEncodeWithDefault(version).c_str());
      }
    } else {
      LOG(ERROR) << "Failed to parse product_components:\n"
                 << app_data.product_components;
    }
  }

  string requisition_arg;
  if (!params->device_requisition().empty()) {
    requisition_arg = "requisition=\"" +
                      XmlEncodeWithDefault(params->device_requisition()) +
                      "\" ";
  }

  // clang-format off
  string app_xml = "    <app "
      "appid=\"" + XmlEncodeWithDefault(app_data.id) + "\" " +
      app_cohort_args +
      app_versions +
      app_channels +
      product_components_args +
      fingerprint_arg +
      buildtype_arg +
      "board=\"" + XmlEncodeWithDefault(params->os_board()) + "\" " +
      "hardware_class=\"" + XmlEncodeWithDefault(params->hwid()) + "\" " +
      "delta_okay=\"" + delta_okay_str + "\" " +
      install_date_in_days_str +

      // DLC excluded for installs and updates.
      (app_data.is_dlc ? "" :
      "lang=\"" + XmlEncodeWithDefault(params->app_lang(), "en-US") + "\" " +
      requisition_arg) +

      ">\n" +
         app_body +
      "    </app>\n";
  // clang-format on
  return app_xml;
}

string OmahaRequestBuilderXml::GetOs() const {
  const auto* params = SystemState::Get()->request_params();
  string os_xml =
      "    <os "
      "version=\"" +
      XmlEncodeWithDefault(params->os_version()) + "\" " + "platform=\"" +
      XmlEncodeWithDefault(params->os_platform()) + "\" " + "sp=\"" +
      XmlEncodeWithDefault(params->os_sp()) +
      "\">"
      "</os>\n";
  return os_xml;
}

string OmahaRequestBuilderXml::GetRequest() const {
  const auto* params = SystemState::Get()->request_params();
  string os_xml = GetOs();
  string app_xml = GetApps();

  string request_xml = base::StringPrintf(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<request requestid=\"%s\" sessionid=\"%s\""
      " protocol=\"3.0\" updater=\"%s\" updaterversion=\"%s\""
      " installsource=\"%s\" ismachine=\"1\">\n%s%s</request>\n",
      base::GenerateGUID().c_str() /* requestid */,
      session_id_.c_str(),
      constants::kOmahaUpdaterID,
      kOmahaUpdaterVersion,
      params->interactive() ? "ondemandupdate" : "scheduler",
      os_xml.c_str(),
      app_xml.c_str());

  return request_xml;
}

string OmahaRequestBuilderXml::GetApps() const {
  const auto* params = SystemState::Get()->request_params();
  string app_xml = "";
  OmahaAppData product_app = {
      .id = params->GetAppId(),
      .version = params->app_version(),
      .product_components = params->product_components(),
      // Skips updatecheck for platform app in case of an install operation.
      .skip_update = params->is_install(),
      .is_dlc = false,

      .app_params = {.active_counting_type = OmahaRequestParams::kDayBased,
                     .send_ping = include_ping_}};
  app_xml += GetApp(product_app);
  for (const auto& it : params->dlc_apps_params()) {
    OmahaAppData dlc_app_data = {
        .id = it.first,
        .version = params->is_install() ? kNoVersion : params->app_version(),
        .skip_update = false,
        .is_dlc = true,
        .app_params = it.second};
    app_xml += GetApp(dlc_app_data);
  }
  return app_xml;
}

}  // namespace chromeos_update_engine
