//
// Copyright (C) 2014 The Android Open Source Project
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

#include "update_engine/metrics_reporter_omaha.h"

#include <string>

#include <base/logging.h>
#include <metrics/metrics_library.h>

#include "update_engine/common/clock_interface.h"
#include "update_engine/common/constants.h"
#include "update_engine/common/prefs_interface.h"
#include "update_engine/common/utils.h"
#include "update_engine/metrics_utils.h"
#include "update_engine/system_state.h"

using std::string;

namespace chromeos_update_engine {

// UpdateEngine.Daily.* metrics.
constexpr char kMetricDailyOSAgeDays[] = "UpdateEngine.Daily.OSAgeDays";

// UpdateEngine.Check.* metrics.
constexpr char kMetricCheckDownloadErrorCode[] =
    "UpdateEngine.Check.DownloadErrorCode";
constexpr char kMetricCheckReaction[] = "UpdateEngine.Check.Reaction";
constexpr char kMetricCheckResult[] = "UpdateEngine.Check.Result";
constexpr char kMetricCheckTimeSinceLastCheckMinutes[] =
    "UpdateEngine.Check.TimeSinceLastCheckMinutes";
constexpr char kMetricCheckTimeSinceLastCheckUptimeMinutes[] =
    "UpdateEngine.Check.TimeSinceLastCheckUptimeMinutes";

// UpdateEngine.Attempt.* metrics.
constexpr char kMetricAttemptNumber[] = "UpdateEngine.Attempt.Number";
constexpr char kMetricAttemptPayloadType[] = "UpdateEngine.Attempt.PayloadType";
constexpr char kMetricAttemptPayloadSizeMiB[] =
    "UpdateEngine.Attempt.PayloadSizeMiB";
constexpr char kMetricAttemptConnectionType[] =
    "UpdateEngine.Attempt.ConnectionType";
constexpr char kMetricAttemptDurationMinutes[] =
    "UpdateEngine.Attempt.DurationMinutes";
constexpr char kMetricAttemptDurationUptimeMinutes[] =
    "UpdateEngine.Attempt.DurationUptimeMinutes";
constexpr char kMetricAttemptTimeSinceLastAttemptMinutes[] =
    "UpdateEngine.Attempt.TimeSinceLastAttemptMinutes";
constexpr char kMetricAttemptTimeSinceLastAttemptUptimeMinutes[] =
    "UpdateEngine.Attempt.TimeSinceLastAttemptUptimeMinutes";
constexpr char kMetricAttemptPayloadBytesDownloadedMiB[] =
    "UpdateEngine.Attempt.PayloadBytesDownloadedMiB";
constexpr char kMetricAttemptPayloadDownloadSpeedKBps[] =
    "UpdateEngine.Attempt.PayloadDownloadSpeedKBps";
constexpr char kMetricAttemptDownloadSource[] =
    "UpdateEngine.Attempt.DownloadSource";
constexpr char kMetricAttemptResult[] = "UpdateEngine.Attempt.Result";
constexpr char kMetricAttemptInternalErrorCode[] =
    "UpdateEngine.Attempt.InternalErrorCode";
constexpr char kMetricAttemptDownloadErrorCode[] =
    "UpdateEngine.Attempt.DownloadErrorCode";

// UpdateEngine.SuccessfulUpdate.* metrics.
constexpr char kMetricSuccessfulUpdateAttemptCount[] =
    "UpdateEngine.SuccessfulUpdate.AttemptCount";
constexpr char kMetricSuccessfulUpdateBytesDownloadedMiB[] =
    "UpdateEngine.SuccessfulUpdate.BytesDownloadedMiB";
constexpr char kMetricSuccessfulUpdateDownloadOverheadPercentage[] =
    "UpdateEngine.SuccessfulUpdate.DownloadOverheadPercentage";
constexpr char kMetricSuccessfulUpdateDownloadSourcesUsed[] =
    "UpdateEngine.SuccessfulUpdate.DownloadSourcesUsed";
constexpr char kMetricSuccessfulUpdatePayloadType[] =
    "UpdateEngine.SuccessfulUpdate.PayloadType";
constexpr char kMetricSuccessfulUpdatePayloadSizeMiB[] =
    "UpdateEngine.SuccessfulUpdate.PayloadSizeMiB";
constexpr char kMetricSuccessfulUpdateRebootCount[] =
    "UpdateEngine.SuccessfulUpdate.RebootCount";
constexpr char kMetricSuccessfulUpdateTotalDurationMinutes[] =
    "UpdateEngine.SuccessfulUpdate.TotalDurationMinutes";
constexpr char kMetricSuccessfulUpdateUpdatesAbandonedCount[] =
    "UpdateEngine.SuccessfulUpdate.UpdatesAbandonedCount";
constexpr char kMetricSuccessfulUpdateUrlSwitchCount[] =
    "UpdateEngine.SuccessfulUpdate.UrlSwitchCount";

// UpdateEngine.Rollback.* metric.
constexpr char kMetricRollbackResult[] = "UpdateEngine.Rollback.Result";

// UpdateEngine.CertificateCheck.* metrics.
constexpr char kMetricCertificateCheckUpdateCheck[] =
    "UpdateEngine.CertificateCheck.UpdateCheck";
constexpr char kMetricCertificateCheckDownload[] =
    "UpdateEngine.CertificateCheck.Download";

// UpdateEngine.* metrics.
constexpr char kMetricFailedUpdateCount[] = "UpdateEngine.FailedUpdateCount";
constexpr char kMetricInstallDateProvisioningSource[] =
    "UpdateEngine.InstallDateProvisioningSource";
constexpr char kMetricTimeToRebootMinutes[] =
    "UpdateEngine.TimeToRebootMinutes";

void MetricsReporterOmaha::Initialize() {
  metrics_lib_.Init();
}

void MetricsReporterOmaha::ReportDailyMetrics(base::TimeDelta os_age) {
  string metric = kMetricDailyOSAgeDays;
  LOG(INFO) << "Uploading " << utils::FormatTimeDelta(os_age) << " for metric "
            << metric;
  metrics_lib_.SendToUMA(metric,
                         static_cast<int>(os_age.InDays()),
                         0,       // min: 0 days
                         6 * 30,  // max: 6 months (approx)
                         50);     // num_buckets
}

void MetricsReporterOmaha::ReportUpdateCheckMetrics(
    SystemState* system_state,
    metrics::CheckResult result,
    metrics::CheckReaction reaction,
    metrics::DownloadErrorCode download_error_code) {
  string metric;
  int value;
  int max_value;

  if (result != metrics::CheckResult::kUnset) {
    metric = kMetricCheckResult;
    value = static_cast<int>(result);
    max_value = static_cast<int>(metrics::CheckResult::kNumConstants) - 1;
    LOG(INFO) << "Sending " << value << " for metric " << metric << " (enum)";
    metrics_lib_.SendEnumToUMA(metric, value, max_value);
  }
  if (reaction != metrics::CheckReaction::kUnset) {
    metric = kMetricCheckReaction;
    value = static_cast<int>(reaction);
    max_value = static_cast<int>(metrics::CheckReaction::kNumConstants) - 1;
    LOG(INFO) << "Sending " << value << " for metric " << metric << " (enum)";
    metrics_lib_.SendEnumToUMA(metric, value, max_value);
  }
  if (download_error_code != metrics::DownloadErrorCode::kUnset) {
    metric = kMetricCheckDownloadErrorCode;
    value = static_cast<int>(download_error_code);
    LOG(INFO) << "Sending " << value << " for metric " << metric << " (sparse)";
    metrics_lib_.SendSparseToUMA(metric, value);
  }

  base::TimeDelta time_since_last;
  if (metrics_utils::WallclockDurationHelper(
          system_state,
          kPrefsMetricsCheckLastReportingTime,
          &time_since_last)) {
    metric = kMetricCheckTimeSinceLastCheckMinutes;
    LOG(INFO) << "Sending " << utils::FormatTimeDelta(time_since_last)
              << " for metric " << metric;
    metrics_lib_.SendToUMA(metric,
                           time_since_last.InMinutes(),
                           0,             // min: 0 min
                           30 * 24 * 60,  // max: 30 days
                           50);           // num_buckets
  }

  base::TimeDelta uptime_since_last;
  static int64_t uptime_since_last_storage = 0;
  if (metrics_utils::MonotonicDurationHelper(
          system_state, &uptime_since_last_storage, &uptime_since_last)) {
    metric = kMetricCheckTimeSinceLastCheckUptimeMinutes;
    LOG(INFO) << "Sending " << utils::FormatTimeDelta(uptime_since_last)
              << " for metric " << metric;
    metrics_lib_.SendToUMA(metric,
                           uptime_since_last.InMinutes(),
                           0,             // min: 0 min
                           30 * 24 * 60,  // max: 30 days
                           50);           // num_buckets
  }
}

void MetricsReporterOmaha::ReportAbnormallyTerminatedUpdateAttemptMetrics() {
  string metric = kMetricAttemptResult;
  metrics::AttemptResult attempt_result =
      metrics::AttemptResult::kAbnormalTermination;

  LOG(INFO) << "Uploading " << static_cast<int>(attempt_result)
            << " for metric " << metric;
  metrics_lib_.SendEnumToUMA(
      metric,
      static_cast<int>(attempt_result),
      static_cast<int>(metrics::AttemptResult::kNumConstants));
}

void MetricsReporterOmaha::ReportUpdateAttemptMetrics(
    SystemState* system_state,
    int attempt_number,
    PayloadType payload_type,
    base::TimeDelta duration,
    base::TimeDelta duration_uptime,
    int64_t payload_size,
    int64_t payload_bytes_downloaded,
    int64_t payload_download_speed_bps,
    DownloadSource download_source,
    metrics::AttemptResult attempt_result,
    ErrorCode internal_error_code,
    metrics::DownloadErrorCode payload_download_error_code,
    metrics::ConnectionType connection_type) {
  string metric = kMetricAttemptNumber;
  LOG(INFO) << "Uploading " << attempt_number << " for metric " << metric;
  metrics_lib_.SendToUMA(metric,
                         attempt_number,
                         0,    // min: 0 attempts
                         49,   // max: 49 attempts
                         50);  // num_buckets

  metric = kMetricAttemptPayloadType;
  LOG(INFO) << "Uploading " << utils::ToString(payload_type) << " for metric "
            << metric;
  metrics_lib_.SendEnumToUMA(metric, payload_type, kNumPayloadTypes);

  metric = kMetricAttemptDurationMinutes;
  LOG(INFO) << "Uploading " << utils::FormatTimeDelta(duration)
            << " for metric " << metric;
  metrics_lib_.SendToUMA(metric,
                         duration.InMinutes(),
                         0,             // min: 0 min
                         10 * 24 * 60,  // max: 10 days
                         50);           // num_buckets

  metric = kMetricAttemptDurationUptimeMinutes;
  LOG(INFO) << "Uploading " << utils::FormatTimeDelta(duration_uptime)
            << " for metric " << metric;
  metrics_lib_.SendToUMA(metric,
                         duration_uptime.InMinutes(),
                         0,             // min: 0 min
                         10 * 24 * 60,  // max: 10 days
                         50);           // num_buckets

  metric = kMetricAttemptPayloadSizeMiB;
  int64_t payload_size_mib = payload_size / kNumBytesInOneMiB;
  LOG(INFO) << "Uploading " << payload_size_mib << " for metric " << metric;
  metrics_lib_.SendToUMA(metric,
                         payload_size_mib,
                         0,     // min: 0 MiB
                         1024,  // max: 1024 MiB = 1 GiB
                         50);   // num_buckets

  metric = kMetricAttemptPayloadBytesDownloadedMiB;
  int64_t payload_bytes_downloaded_mib =
      payload_bytes_downloaded / kNumBytesInOneMiB;
  LOG(INFO) << "Uploading " << payload_bytes_downloaded_mib << " for metric "
            << metric;
  metrics_lib_.SendToUMA(metric,
                         payload_bytes_downloaded_mib,
                         0,     // min: 0 MiB
                         1024,  // max: 1024 MiB = 1 GiB
                         50);   // num_buckets

  metric = kMetricAttemptPayloadDownloadSpeedKBps;
  int64_t payload_download_speed_kbps = payload_download_speed_bps / 1000;
  LOG(INFO) << "Uploading " << payload_download_speed_kbps << " for metric "
            << metric;
  metrics_lib_.SendToUMA(metric,
                         payload_download_speed_kbps,
                         0,          // min: 0 kB/s
                         10 * 1000,  // max: 10000 kB/s = 10 MB/s
                         50);        // num_buckets

  metric = kMetricAttemptDownloadSource;
  LOG(INFO) << "Uploading " << download_source << " for metric " << metric;
  metrics_lib_.SendEnumToUMA(metric, download_source, kNumDownloadSources);

  metric = kMetricAttemptResult;
  LOG(INFO) << "Uploading " << static_cast<int>(attempt_result)
            << " for metric " << metric;
  metrics_lib_.SendEnumToUMA(
      metric,
      static_cast<int>(attempt_result),
      static_cast<int>(metrics::AttemptResult::kNumConstants));

  if (internal_error_code != ErrorCode::kSuccess) {
    metric = kMetricAttemptInternalErrorCode;
    LOG(INFO) << "Uploading " << internal_error_code << " for metric "
              << metric;
    metrics_lib_.SendEnumToUMA(metric,
                               static_cast<int>(internal_error_code),
                               static_cast<int>(ErrorCode::kUmaReportedMax));
  }

  if (payload_download_error_code != metrics::DownloadErrorCode::kUnset) {
    metric = kMetricAttemptDownloadErrorCode;
    LOG(INFO) << "Uploading " << static_cast<int>(payload_download_error_code)
              << " for metric " << metric << " (sparse)";
    metrics_lib_.SendSparseToUMA(metric,
                                 static_cast<int>(payload_download_error_code));
  }

  base::TimeDelta time_since_last;
  if (metrics_utils::WallclockDurationHelper(
          system_state,
          kPrefsMetricsAttemptLastReportingTime,
          &time_since_last)) {
    metric = kMetricAttemptTimeSinceLastAttemptMinutes;
    LOG(INFO) << "Sending " << utils::FormatTimeDelta(time_since_last)
              << " for metric " << metric;
    metrics_lib_.SendToUMA(metric,
                           time_since_last.InMinutes(),
                           0,             // min: 0 min
                           30 * 24 * 60,  // max: 30 days
                           50);           // num_buckets
  }

  static int64_t uptime_since_last_storage = 0;
  base::TimeDelta uptime_since_last;
  if (metrics_utils::MonotonicDurationHelper(
          system_state, &uptime_since_last_storage, &uptime_since_last)) {
    metric = kMetricAttemptTimeSinceLastAttemptUptimeMinutes;
    LOG(INFO) << "Sending " << utils::FormatTimeDelta(uptime_since_last)
              << " for metric " << metric;
    metrics_lib_.SendToUMA(metric,
                           uptime_since_last.InMinutes(),
                           0,             // min: 0 min
                           30 * 24 * 60,  // max: 30 days
                           50);           // num_buckets
  }

  metric = kMetricAttemptConnectionType;
  LOG(INFO) << "Uploading " << static_cast<int>(connection_type)
            << " for metric " << metric;
  metrics_lib_.SendEnumToUMA(
      metric,
      static_cast<int>(connection_type),
      static_cast<int>(metrics::ConnectionType::kNumConstants));
}

void MetricsReporterOmaha::ReportSuccessfulUpdateMetrics(
    int attempt_count,
    int updates_abandoned_count,
    PayloadType payload_type,
    int64_t payload_size,
    int64_t num_bytes_downloaded[kNumDownloadSources],
    int download_overhead_percentage,
    base::TimeDelta total_duration,
    int reboot_count,
    int url_switch_count) {
  string metric = kMetricSuccessfulUpdatePayloadSizeMiB;
  int64_t mbs = payload_size / kNumBytesInOneMiB;
  LOG(INFO) << "Uploading " << mbs << " (MiBs) for metric " << metric;
  metrics_lib_.SendToUMA(metric,
                         mbs,
                         0,     // min: 0 MiB
                         1024,  // max: 1024 MiB = 1 GiB
                         50);   // num_buckets

  int64_t total_bytes = 0;
  int download_sources_used = 0;
  for (int i = 0; i < kNumDownloadSources + 1; i++) {
    DownloadSource source = static_cast<DownloadSource>(i);

    // Only consider this download source (and send byte counts) as
    // having been used if we downloaded a non-trivial amount of bytes
    // (e.g. at least 1 MiB) that contributed to the
    // update. Otherwise we're going to end up with a lot of zero-byte
    // events in the histogram.

    metric = kMetricSuccessfulUpdateBytesDownloadedMiB;
    if (i < kNumDownloadSources) {
      metric += utils::ToString(source);
      mbs = num_bytes_downloaded[i] / kNumBytesInOneMiB;
      total_bytes += num_bytes_downloaded[i];
      if (mbs > 0)
        download_sources_used |= (1 << i);
    } else {
      mbs = total_bytes / kNumBytesInOneMiB;
    }

    if (mbs > 0) {
      LOG(INFO) << "Uploading " << mbs << " (MiBs) for metric " << metric;
      metrics_lib_.SendToUMA(metric,
                             mbs,
                             0,     // min: 0 MiB
                             1024,  // max: 1024 MiB = 1 GiB
                             50);   // num_buckets
    }
  }

  metric = kMetricSuccessfulUpdateDownloadSourcesUsed;
  LOG(INFO) << "Uploading 0x" << std::hex << download_sources_used
            << " (bit flags) for metric " << metric;
  metrics_lib_.SendToUMA(metric,
                         download_sources_used,
                         0,                               // min
                         (1 << kNumDownloadSources) - 1,  // max
                         1 << kNumDownloadSources);       // num_buckets

  metric = kMetricSuccessfulUpdateDownloadOverheadPercentage;
  LOG(INFO) << "Uploading " << download_overhead_percentage << "% for metric "
            << metric;
  metrics_lib_.SendToUMA(metric,
                         download_overhead_percentage,
                         0,     // min: 0% overhead
                         1000,  // max: 1000% overhead
                         50);   // num_buckets

  metric = kMetricSuccessfulUpdateUrlSwitchCount;
  LOG(INFO) << "Uploading " << url_switch_count << " (count) for metric "
            << metric;
  metrics_lib_.SendToUMA(metric,
                         url_switch_count,
                         0,    // min: 0 URL switches
                         49,   // max: 49 URL switches
                         50);  // num_buckets

  metric = kMetricSuccessfulUpdateTotalDurationMinutes;
  LOG(INFO) << "Uploading " << utils::FormatTimeDelta(total_duration)
            << " for metric " << metric;
  metrics_lib_.SendToUMA(metric,
                         static_cast<int>(total_duration.InMinutes()),
                         0,              // min: 0 min
                         365 * 24 * 60,  // max: 365 days ~= 1 year
                         50);            // num_buckets

  metric = kMetricSuccessfulUpdateRebootCount;
  LOG(INFO) << "Uploading reboot count of " << reboot_count << " for metric "
            << metric;
  metrics_lib_.SendToUMA(metric,
                         reboot_count,
                         0,    // min: 0 reboots
                         49,   // max: 49 reboots
                         50);  // num_buckets

  metric = kMetricSuccessfulUpdatePayloadType;
  metrics_lib_.SendEnumToUMA(metric, payload_type, kNumPayloadTypes);
  LOG(INFO) << "Uploading " << utils::ToString(payload_type) << " for metric "
            << metric;

  metric = kMetricSuccessfulUpdateAttemptCount;
  metrics_lib_.SendToUMA(metric,
                         attempt_count,
                         1,    // min: 1 attempt
                         50,   // max: 50 attempts
                         50);  // num_buckets
  LOG(INFO) << "Uploading " << attempt_count << " for metric " << metric;

  metric = kMetricSuccessfulUpdateUpdatesAbandonedCount;
  LOG(INFO) << "Uploading " << updates_abandoned_count << " (count) for metric "
            << metric;
  metrics_lib_.SendToUMA(metric,
                         updates_abandoned_count,
                         0,    // min: 0 counts
                         49,   // max: 49 counts
                         50);  // num_buckets
}

void MetricsReporterOmaha::ReportRollbackMetrics(
    metrics::RollbackResult result) {
  string metric = kMetricRollbackResult;
  int value = static_cast<int>(result);
  LOG(INFO) << "Sending " << value << " for metric " << metric << " (enum)";
  metrics_lib_.SendEnumToUMA(
      metric, value, static_cast<int>(metrics::RollbackResult::kNumConstants));
}

void MetricsReporterOmaha::ReportCertificateCheckMetrics(
    ServerToCheck server_to_check, CertificateCheckResult result) {
  string metric;
  switch (server_to_check) {
    case ServerToCheck::kUpdate:
      metric = kMetricCertificateCheckUpdateCheck;
      break;
    case ServerToCheck::kDownload:
      metric = kMetricCertificateCheckDownload;
      break;
    case ServerToCheck::kNone:
      return;
  }
  LOG(INFO) << "Uploading " << static_cast<int>(result) << " for metric "
            << metric;
  metrics_lib_.SendEnumToUMA(
      metric,
      static_cast<int>(result),
      static_cast<int>(CertificateCheckResult::kNumConstants));
}

void MetricsReporterOmaha::ReportFailedUpdateCount(int target_attempt) {
  string metric = kMetricFailedUpdateCount;
  metrics_lib_.SendToUMA(metric,
                         target_attempt,
                         1,   // min value
                         50,  // max value
                         kNumDefaultUmaBuckets);

  LOG(INFO) << "Uploading " << target_attempt << " (count) for metric "
            << metric;
}

void MetricsReporterOmaha::ReportTimeToReboot(int time_to_reboot_minutes) {
  string metric = kMetricTimeToRebootMinutes;
  metrics_lib_.SendToUMA(metric,
                         time_to_reboot_minutes,
                         0,             // min: 0 minute
                         30 * 24 * 60,  // max: 1 month (approx)
                         kNumDefaultUmaBuckets);

  LOG(INFO) << "Uploading " << time_to_reboot_minutes << " for metric "
            << metric;
}

void MetricsReporterOmaha::ReportInstallDateProvisioningSource(int source,
                                                               int max) {
  metrics_lib_.SendEnumToUMA(kMetricInstallDateProvisioningSource,
                             source,  // Sample.
                             max);
}

}  // namespace chromeos_update_engine
