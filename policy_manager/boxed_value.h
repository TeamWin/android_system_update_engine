// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_BOXED_VALUE_H
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_BOXED_VALUE_H

#include <base/basictypes.h>

namespace chromeos_policy_manager {

// BoxedValue is a class to hold pointers of a given type that deletes them when
// the instance goes out of scope, as scoped_ptr<T> does. The main difference
// with it is that the type T is not part of the class, i.e., this isn't a
// parametric class. The class has a parametric contructor that accepts a
// const T* which will define the type of the object passed on delete.
//
// It is safe to use this class in linked containers such as std::list and
// std::map but the object can't be copied. This means that you need to
// construct the BoxedValue inplace using a container method like emplace()
// or move it with std::move().
//
//   list<BoxedValue> lst;
//   lst.emplace_back(new const int(42));
//   lst.emplace_back(new const string("Hello world!"));
//
//   map<int, BoxedValue> m;
//   m.emplace(123, std::move(BoxedValue(new const string("Hola mundo!"))));
//
//   auto it = m.find(42);
//   if (it != m.end())
//     cout << "m[42] points to " << it->second.value() << endl;
//   cout << "m[33] points to " << m[33].value() << endl;
//
// Since copy and assign are not allowed, you can't create a copy of the
// BoxedValue which means that you can only use a reference to it.
//

class BoxedValue {
 public:
  // Creates an empty BoxedValue. Since the pointer can't be assigned from other
  // BoxedValues or pointers, this is only useful in places where a default
  // constructor is required, such as std::map::operator[].
  BoxedValue() : value_(NULL), deleter_(NULL) {}

  // Creates a BoxedValue for the passed pointer |value|. The BoxedValue keeps
  // the ownership of this pointer and can't be released.
  template<typename T>
  explicit BoxedValue(const T* value)
      : value_(static_cast<const void*>(value)), deleter_(ValueDeleter<T>) {}

  // The move constructor takes ownership of the pointer since the semantics of
  // it allows to render the passed BoxedValue undefined. You need to use the
  // move constructor explictly preventing it from accidental references,
  // like in:
  //   BoxedValue new_box(std::move(other_box));
  BoxedValue(BoxedValue&& other)
      : value_(other.value_), deleter_(other.deleter_) {
    other.value_ = NULL;
    other.deleter_ = NULL;
  }

  // Deletes the |value| passed on construction using the delete for the passed
  // type.
  ~BoxedValue() {
    if (deleter_)
      deleter_(value_);
  }

  const void* value() const { return value_; }

  // Static method to call the destructor of the right type.
  template<typename T>
  static void ValueDeleter(const void* value) {
    delete reinterpret_cast<const T*>(value);
  }

 private:
  // A pointer to the cached value.
  const void* value_;

  // A function that calls delete for the right type of value_.
  void (*deleter_)(const void*);

  DISALLOW_COPY_AND_ASSIGN(BoxedValue);
};

}  // namespace chromeos_policy_manager

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POLICY_MANAGER_BOXED_VALUE_H
