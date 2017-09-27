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

#ifndef UPDATE_ENGINE_PAYLOAD_GENERATOR_DEFLATE_UTILS_H_
#define UPDATE_ENGINE_PAYLOAD_GENERATOR_DEFLATE_UTILS_H_

#include <vector>

#include "update_engine/payload_generator/filesystem_interface.h"
#include "update_engine/payload_generator/payload_generation_config.h"

namespace chromeos_update_engine {
namespace deflate_utils {

// Spreads all extents in |over_extents| over |base_extents|. Here we assume the
// extents are non-overlapping.
//
// |base_extents|:
// |               -----------------------        ------         --------------
// |over_extents|:
// |  ==========  ====    ==========  ======
// |over_extents| is transforms to:
// |                 ==========  ====    =        ======         ===  ======
//
bool ShiftExtentsOverExtents(const std::vector<Extent>& base_extents,
                             std::vector<Extent>* over_extents);

// Gets the files from the partition and processes all its files. Processing
// includes:
//  - splitting large Squashfs containers into its smaller files.
bool PreprocessParitionFiles(const PartitionConfig& part,
                             std::vector<FilesystemInterface::File>* result);

}  // namespace deflate_utils
}  // namespace chromeos_update_engine
#endif  // UPDATE_ENGINE_PAYLOAD_GENERATOR_DEFLATE_UTILS_H_
