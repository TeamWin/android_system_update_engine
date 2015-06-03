// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_PAYLOAD_GENERATOR_GRAPH_UTILS_H_
#define UPDATE_ENGINE_PAYLOAD_GENERATOR_GRAPH_UTILS_H_

#include <vector>

#include <base/macros.h>

#include "update_engine/payload_generator/graph_types.h"
#include "update_engine/update_metadata.pb.h"

// A few utility functions for graphs

namespace chromeos_update_engine {

namespace graph_utils {

// Returns the number of blocks represented by all extents in the edge.
uint64_t EdgeWeight(const Graph& graph, const Edge& edge);

// These add a read-before dependency from graph[src] -> graph[dst]. If the dep
// already exists, the block/s is/are added to the existing edge.
void AddReadBeforeDep(Vertex* src,
                      Vertex::Index dst,
                      uint64_t block);
void AddReadBeforeDepExtents(Vertex* src,
                             Vertex::Index dst,
                             const std::vector<Extent>& extents);

void DropWriteBeforeDeps(Vertex::EdgeMap* edge_map);

// For each node N in graph, drop all edges N->|index|.
void DropIncomingEdgesTo(Graph* graph, Vertex::Index index);

void DumpGraph(const Graph& graph);

}  // namespace graph_utils

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_PAYLOAD_GENERATOR_GRAPH_UTILS_H_
