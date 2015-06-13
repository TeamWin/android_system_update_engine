// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_PAYLOAD_GENERATOR_DELTA_DIFF_GENERATOR_H_
#define UPDATE_ENGINE_PAYLOAD_GENERATOR_DELTA_DIFF_GENERATOR_H_

#include <string>

#include "update_engine/payload_generator/payload_generation_config.h"

// There is one function in DeltaDiffGenerator of importance to users
// of the class: GenerateDeltaUpdateFile(). Before calling it,
// the old and new images must be mounted. Call GenerateDeltaUpdateFile()
// with both the mount-points of the images in addition to the paths of
// the images (both old and new). A delta from old to new will be
// generated and stored in output_path.

namespace chromeos_update_engine {

extern const size_t kBlockSize;
extern const size_t kRootFSPartitionSize;

// The |config| describes the payload generation request, describing both
// old and new images for delta payloads and only the new image for full
// payloads.
// For delta payloads, the images should be already mounted read-only at
// the respective rootfs_mountpt.
// |private_key_path| points to a private key used to sign the update.
// Pass empty string to not sign the update.
// |output_path| is the filename where the delta update should be written.
// Returns true on success. Also writes the size of the metadata into
// |metadata_size|.
bool GenerateUpdatePayloadFile(const PayloadGenerationConfig& config,
                               const std::string& output_path,
                               const std::string& private_key_path,
                               uint64_t* metadata_size);


};  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_PAYLOAD_GENERATOR_DELTA_DIFF_GENERATOR_H_
