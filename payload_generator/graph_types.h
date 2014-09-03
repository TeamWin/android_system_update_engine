// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_PAYLOAD_GENERATOR_GRAPH_TYPES_H_
#define UPDATE_ENGINE_PAYLOAD_GENERATOR_GRAPH_TYPES_H_

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <base/macros.h>

#include "update_engine/update_metadata.pb.h"

// A few classes that help in generating delta images use these types
// for the graph work.

namespace chromeos_update_engine {

bool operator==(const Extent& a, const Extent& b);

struct EdgeProperties {
  // Read-before extents. I.e., blocks in |extents| must be read by the
  // node pointed to before the pointing node runs (presumably b/c it
  // overwrites these blocks).
  std::vector<Extent> extents;

  // Write before extents. I.e., blocks in |write_extents| must be written
  // by the node pointed to before the pointing node runs (presumably
  // b/c it reads the data written by the other node).
  std::vector<Extent> write_extents;

  bool operator==(const EdgeProperties& that) const {
    return extents == that.extents && write_extents == that.write_extents;
  }
};

struct Vertex {
  Vertex() :
      valid(true),
      index(-1),
      lowlink(-1),
      chunk_offset(0),
      chunk_size(-1) {}
  bool valid;

  typedef std::map<std::vector<Vertex>::size_type, EdgeProperties> EdgeMap;
  EdgeMap out_edges;

  // We sometimes wish to consider a subgraph of a graph. A subgraph would have
  // a subset of the vertices from the graph and a subset of the edges.
  // When considering this vertex within a subgraph, subgraph_edges stores
  // the out-edges.
  typedef std::set<std::vector<Vertex>::size_type> SubgraphEdgeMap;
  SubgraphEdgeMap subgraph_edges;

  // For Tarjan's algorithm:
  std::vector<Vertex>::size_type index;
  std::vector<Vertex>::size_type lowlink;

  // Other Vertex properties:
  DeltaArchiveManifest_InstallOperation op;
  std::string file_name;
  off_t chunk_offset;
  off_t chunk_size;

  typedef std::vector<Vertex>::size_type Index;
  static const Vertex::Index kInvalidIndex = -1;
};

typedef std::vector<Vertex> Graph;

typedef std::pair<Vertex::Index, Vertex::Index> Edge;

const uint64_t kTempBlockStart = 1ULL << 60;
COMPILE_ASSERT(kTempBlockStart != 0, kTempBlockStart_invalid);

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_PAYLOAD_GENERATOR_GRAPH_TYPES_H_
