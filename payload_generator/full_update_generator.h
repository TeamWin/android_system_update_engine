// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_PAYLOAD_GENERATOR_FULL_UPDATE_GENERATOR_H_
#define UPDATE_ENGINE_PAYLOAD_GENERATOR_FULL_UPDATE_GENERATOR_H_

#include <string>
#include <vector>

#include "update_engine/payload_generator/graph_types.h"
#include "update_engine/payload_generator/payload_generation_config.h"

namespace chromeos_update_engine {

class FullUpdateGenerator {
 public:
  // Creates a full update for the target image defined in |config|. |config|
  // must be a valid payload generation configuration for a full payload.
  // Populates |graph|, |kernel_ops|, and |final_order|, with data about the
  // update operations, and writes relevant data to |data_file_fd|, updating
  // |data_file_size| as it does.
  static bool Run(
      const PayloadGenerationConfig& config,
      int data_file_fd,
      off_t* data_file_size,
      Graph* graph,
      std::vector<DeltaArchiveManifest_InstallOperation>* kernel_ops,
      std::vector<Vertex::Index>* final_order);

 private:
  // This should never be constructed.
  DISALLOW_IMPLICIT_CONSTRUCTORS(FullUpdateGenerator);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_PAYLOAD_GENERATOR_FULL_UPDATE_GENERATOR_H_
