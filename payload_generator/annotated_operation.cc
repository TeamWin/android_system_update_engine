// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/payload_generator/annotated_operation.h"

#include <base/format_macros.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/stringprintf.h>

#include "update_engine/utils.h"

using std::string;

namespace chromeos_update_engine {

namespace {
// Output the list of extents as (start_block, num_blocks) in the passed output
// stream.
void OutputExtents(std::ostream* os,
                   const google::protobuf::RepeatedPtrField<Extent>& extents) {
  for (const auto& extent : extents) {
    *os << " (" << extent.start_block() << ", " << extent.num_blocks() << ")";
  }
}
}  // namespace

bool AnnotatedOperation::SetOperationBlob(chromeos::Blob* blob, int data_fd,
                                          off_t* data_file_size) {
  TEST_AND_RETURN_FALSE(utils::PWriteAll(data_fd,
                                         blob->data(),
                                         blob->size(),
                                         *data_file_size));
  op.set_data_length(blob->size());
  op.set_data_offset(*data_file_size);
  *data_file_size += blob->size();
  return true;
}

string InstallOperationTypeName(
    DeltaArchiveManifest_InstallOperation_Type op_type) {
  switch (op_type) {
    case DeltaArchiveManifest_InstallOperation_Type_BSDIFF:
      return "BSDIFF";
    case DeltaArchiveManifest_InstallOperation_Type_MOVE:
      return "MOVE";
    case DeltaArchiveManifest_InstallOperation_Type_REPLACE:
      return "REPLACE";
    case DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ:
      return "REPLACE_BZ";
    case DeltaArchiveManifest_InstallOperation_Type_SOURCE_COPY:
      return "SOURCE_COPY";
    case DeltaArchiveManifest_InstallOperation_Type_SOURCE_BSDIFF:
      return "SOURCE_BSDIFF";
  }
  return "UNK";
}

std::ostream& operator<<(std::ostream& os, const AnnotatedOperation& aop) {
  // For example, this prints:
  // REPLACE_BZ 500 @3000
  //   name: /foo/bar
  //    dst: (123, 3) (127, 2)
  os << InstallOperationTypeName(aop.op.type()) << " "  << aop.op.data_length();
  if (aop.op.data_length() > 0)
    os << " @" << aop.op.data_offset();
  if (!aop.name.empty()) {
    os << std::endl << "  name: " << aop.name;
  }
  if (aop.op.src_extents_size() != 0) {
    os << std::endl << "   src:";
    OutputExtents(&os, aop.op.src_extents());
  }
  if (aop.op.dst_extents_size() != 0) {
    os << std::endl << "   dst:";
    OutputExtents(&os, aop.op.dst_extents());
  }
  return os;
}

}  // namespace chromeos_update_engine
