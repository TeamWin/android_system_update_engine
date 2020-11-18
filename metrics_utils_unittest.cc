//
// Copyright (C) 2015 The Android Open Source Project
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

#include "update_engine/metrics_utils.h"

#include <gtest/gtest.h>

namespace chromeos_update_engine {
namespace metrics_utils {

class MetricsUtilsTest : public ::testing::Test {};

TEST(MetricsUtilsTest, GetConnectionType) {
  // Check that expected combinations map to the right value.
  EXPECT_EQ(metrics::ConnectionType::kUnknown,
            GetConnectionType(ConnectionType::kUnknown,
                              ConnectionTethering::kUnknown));
  EXPECT_EQ(metrics::ConnectionType::kDisconnected,
            GetConnectionType(ConnectionType::kDisconnected,
                              ConnectionTethering::kUnknown));
  EXPECT_EQ(metrics::ConnectionType::kEthernet,
            GetConnectionType(ConnectionType::kEthernet,
                              ConnectionTethering::kUnknown));
  EXPECT_EQ(
      metrics::ConnectionType::kWifi,
      GetConnectionType(ConnectionType::kWifi, ConnectionTethering::kUnknown));
  EXPECT_EQ(metrics::ConnectionType::kCellular,
            GetConnectionType(ConnectionType::kCellular,
                              ConnectionTethering::kUnknown));
  EXPECT_EQ(metrics::ConnectionType::kTetheredEthernet,
            GetConnectionType(ConnectionType::kEthernet,
                              ConnectionTethering::kConfirmed));
  EXPECT_EQ(metrics::ConnectionType::kTetheredWifi,
            GetConnectionType(ConnectionType::kWifi,
                              ConnectionTethering::kConfirmed));

  // Ensure that we don't report tethered ethernet unless it's confirmed.
  EXPECT_EQ(metrics::ConnectionType::kEthernet,
            GetConnectionType(ConnectionType::kEthernet,
                              ConnectionTethering::kNotDetected));
  EXPECT_EQ(metrics::ConnectionType::kEthernet,
            GetConnectionType(ConnectionType::kEthernet,
                              ConnectionTethering::kSuspected));
  EXPECT_EQ(metrics::ConnectionType::kEthernet,
            GetConnectionType(ConnectionType::kEthernet,
                              ConnectionTethering::kUnknown));

  // Ditto for tethered wifi.
  EXPECT_EQ(metrics::ConnectionType::kWifi,
            GetConnectionType(ConnectionType::kWifi,
                              ConnectionTethering::kNotDetected));
  EXPECT_EQ(metrics::ConnectionType::kWifi,
            GetConnectionType(ConnectionType::kWifi,
                              ConnectionTethering::kSuspected));
  EXPECT_EQ(
      metrics::ConnectionType::kWifi,
      GetConnectionType(ConnectionType::kWifi, ConnectionTethering::kUnknown));
}

}  // namespace metrics_utils
}  // namespace chromeos_update_engine
