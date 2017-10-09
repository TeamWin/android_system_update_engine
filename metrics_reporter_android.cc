//
// Copyright (C) 2017 The Android Open Source Project
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

#include "update_engine/metrics_reporter_android.h"

#ifndef _UE_SIDELOAD
#include <metricslogger/metrics_logger.h>
#endif  // _UE_SIDELOAD

namespace chromeos_update_engine {

namespace metrics {

#ifndef _UE_SIDELOAD
const char kMetricsUpdateEngineErrorCode[] = "ota_update_engine_error_code";
#endif
}  // namespace metrics

void MetricsReporterAndroid::ReportUpdateAttemptMetrics(
    SystemState* /* system_state */,
    int /* attempt_number */,
    PayloadType /* payload_type */,
    base::TimeDelta /* duration */,
    base::TimeDelta /* duration_uptime */,
    int64_t /* payload_size */,
    metrics::AttemptResult /* attempt_result */,
    ErrorCode error_code) {
// No need to log histogram under sideload mode.
#ifndef _UE_SIDELOAD
  android::metricslogger::LogHistogram(metrics::kMetricsUpdateEngineErrorCode,
                                       static_cast<int>(error_code));
#endif
}

};  // namespace chromeos_update_engine
