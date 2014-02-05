// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <list>
#include <map>
#include <string>

#include "update_engine/policy_manager/boxed_value.h"
#include "update_engine/policy_manager/pmtest_utils.h"

using std::list;
using std::map;
using std::string;

namespace chromeos_policy_manager {

// The DeleterMarker flags a bool variable when the class is destroyed.
class DeleterMarker {
 public:
  DeleterMarker(bool* marker) : marker_(marker) { *marker_ = false; }

  ~DeleterMarker() { *marker_ = true; }

 private:
  // Pointer to the bool marker.
  bool* marker_;
};

TEST(PmBoxedValueTest, Deleted) {
  bool marker = true;
  const DeleterMarker* deleter_marker = new DeleterMarker(&marker);

  EXPECT_FALSE(marker);
  BoxedValue* box = new BoxedValue(deleter_marker);
  EXPECT_FALSE(marker);
  delete box;
  EXPECT_TRUE(marker);
}

TEST(PmBoxedValueTest, MoveConstructor) {
  bool marker = true;
  const DeleterMarker* deleter_marker = new DeleterMarker(&marker);

  BoxedValue* box = new BoxedValue(deleter_marker);
  BoxedValue* new_box = new BoxedValue(std::move(*box));
  // box is now undefined but valid.
  delete box;
  EXPECT_FALSE(marker);
  // The deleter_marker gets deleted at this point.
  delete new_box;
  EXPECT_TRUE(marker);
}

TEST(PmBoxedValueTest, MixedList) {
  list<BoxedValue> lst;
  // This is mostly a compile test.
  lst.emplace_back(new const int(42));
  lst.emplace_back(new const string("Hello world!"));
  bool marker;
  lst.emplace_back(new const DeleterMarker(&marker));
  EXPECT_FALSE(marker);
  lst.clear();
  EXPECT_TRUE(marker);
}

TEST(PmBoxedValueTest, MixedMap) {
  map<int, BoxedValue> m;
  m.emplace(42, std::move(BoxedValue(new const string("Hola mundo!"))));

  auto it = m.find(42);
  ASSERT_NE(it, m.end());
  PMTEST_EXPECT_NOT_NULL(it->second.value());
  PMTEST_EXPECT_NULL(m[33].value());
}

}  // namespace chromeos_policy_manager
