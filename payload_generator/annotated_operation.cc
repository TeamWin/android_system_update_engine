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

bool AnnotatedOperation::SetOperationBlob(chromeos::Blob* blob,
                                          BlobFileWriter* blob_file) {
  op.set_data_length(blob->size());
  off_t data_offset = blob_file->StoreBlob(*blob);
  if (data_offset == -1)
    return false;
  op.set_data_offset(data_offset);
  return true;
}

string InstallOperationTypeName(InstallOperation_Type op_type) {
  switch (op_type) {
    case InstallOperation::BSDIFF:
      return "BSDIFF";
    case InstallOperation::MOVE:
      return "MOVE";
    case InstallOperation::REPLACE:
      return "REPLACE";
    case InstallOperation::REPLACE_BZ:
      return "REPLACE_BZ";
    case InstallOperation::SOURCE_COPY:
      return "SOURCE_COPY";
    case InstallOperation::SOURCE_BSDIFF:
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
