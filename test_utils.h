// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_TEST_UTILS_H_
#define UPDATE_ENGINE_TEST_UTILS_H_

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include <base/callback.h>
#include <glib-object.h>
#include <gtest/gtest.h>

#include "update_engine/action.h"
#include "update_engine/subprocess.h"
#include "update_engine/utils.h"

// These are some handy functions for unittests.

namespace chromeos_update_engine {
namespace test_utils {

// 300 byte pseudo-random string. Not null terminated.
// This does not gzip compress well.
extern const unsigned char kRandomString[300];

// Writes the data passed to path. The file at path will be overwritten if it
// exists. Returns true on success, false otherwise.
bool WriteFileVector(const std::string& path, const std::vector<char>& data);
bool WriteFileString(const std::string& path, const std::string& data);

bool BindToUnusedLoopDevice(const std::string &filename,
                            std::string* lo_dev_name_ptr);

// Returns true iff a == b
bool ExpectVectorsEq(const std::vector<char>& a, const std::vector<char>& b);

inline int System(const std::string& cmd) {
  return system(cmd.c_str());
}

inline int Symlink(const std::string& oldpath, const std::string& newpath) {
  return symlink(oldpath.c_str(), newpath.c_str());
}

inline int Chmod(const std::string& path, mode_t mode) {
  return chmod(path.c_str(), mode);
}

inline int Mkdir(const std::string& path, mode_t mode) {
  return mkdir(path.c_str(), mode);
}

inline int Chdir(const std::string& path) {
  return chdir(path.c_str());
}

// Checks if xattr is supported in the directory specified by
// |dir_path| which must be writable. Returns true if the feature is
// supported, false if not or if an error occured.
bool IsXAttrSupported(const base::FilePath& dir_path);

void FillWithData(std::vector<char>* buffer);

// Creates an empty ext image.
void CreateEmptyExtImageAtPath(const std::string& path,
                               size_t size,
                               int block_size);

// Creates an ext image with some files in it. The paths creates are
// returned in out_paths.
void CreateExtImageAtPath(const std::string& path,
                          std::vector<std::string>* out_paths);

// Verifies that for each path in paths, it exists in the filesystem under
// parent. Also, verifies that no additional paths are present under parent.
// Also tests properties of various files created by CreateExtImageAtPath().
// Intentionally copies expected_paths.
void VerifyAllPaths(const std::string& parent,
                    std::set<std::string> expected_paths);

// Class to unmount FS when object goes out of scope
class ScopedFilesystemUnmounter {
 public:
  explicit ScopedFilesystemUnmounter(const std::string& mountpoint)
      : mountpoint_(mountpoint),
        should_unmount_(true) {}
  ~ScopedFilesystemUnmounter() {
    if (should_unmount_) {
      utils::UnmountFilesystem(mountpoint_);
    }
  }
  void set_should_unmount(bool unmount) { should_unmount_ = unmount; }
 private:
  const std::string mountpoint_;
  bool should_unmount_;
  DISALLOW_COPY_AND_ASSIGN(ScopedFilesystemUnmounter);
};

class ScopedLoopbackDeviceBinder {
 public:
  ScopedLoopbackDeviceBinder(const std::string& file, std::string* dev) {
    is_bound_ = BindToUnusedLoopDevice(file, &dev_);
    EXPECT_TRUE(is_bound_);

    if (is_bound_ && dev)
      *dev = dev_;
  }

  ~ScopedLoopbackDeviceBinder() {
    if (!is_bound_)
      return;

    for (int retry = 0; retry < 5; retry++) {
      std::vector<std::string> args;
      args.push_back("/sbin/losetup");
      args.push_back("-d");
      args.push_back(dev_);
      int return_code = 0;
      EXPECT_TRUE(Subprocess::SynchronousExec(args, &return_code, nullptr));
      if (return_code == 0) {
        return;
      }
      sleep(1);
    }
    ADD_FAILURE();
  }

  const std::string &dev() {
    EXPECT_TRUE(is_bound_);
    return dev_;
  }

  bool is_bound() const { return is_bound_; }

 private:
  std::string dev_;
  bool is_bound_;
  DISALLOW_COPY_AND_ASSIGN(ScopedLoopbackDeviceBinder);
};

class ScopedTempFile {
 public:
  ScopedTempFile() {
    EXPECT_TRUE(utils::MakeTempFile("/tmp/update_engine_test_temp_file.XXXXXX",
                                    &path_,
                                    nullptr));
    unlinker_.reset(new ScopedPathUnlinker(path_));
  }
  const std::string& GetPath() { return path_; }
 private:
  std::string path_;
  std::unique_ptr<ScopedPathUnlinker> unlinker_;
};

class ScopedLoopMounter {
 public:
  explicit ScopedLoopMounter(const std::string& file_path,
                             std::string* mnt_path,
                             unsigned long flags);  // NOLINT(runtime/int)

 private:
  // These objects must be destructed in the following order:
  //   ScopedFilesystemUnmounter (the file system must be unmounted first)
  //   ScopedLoopbackDeviceBinder (then the loop device can be deleted)
  //   ScopedDirRemover (then the mount point can be deleted)
  std::unique_ptr<ScopedDirRemover> dir_remover_;
  std::unique_ptr<ScopedLoopbackDeviceBinder> loop_binder_;
  std::unique_ptr<ScopedFilesystemUnmounter> unmounter_;
};

// Deletes a directory and all its contents synchronously. Returns true
// on success. This may be called with a regular file--it will just unlink it.
// This WILL cross filesystem boundaries.
bool RecursiveUnlinkDir(const std::string& path);

// Runs the default GLib main loop for at most |timeout_msec| or until the
// function |terminate| returns true, whichever happens first. The function
// |terminate| is called before every GLib main loop iteration and its value is
// checked.
void RunGMainLoopUntil(int timeout_msec, base::Callback<bool()> terminate);

// Runs the default GLib main loop at most |iterations| times. This
// dispatches all the events that are already waiting in the main loop and
// those that get scheduled as a result of these events being attended.
// Returns the number of iterations the main loop was ran. If there are more
// than |iterations| events to attend, then this function returns |iterations|
// and the remaining events are not dispatched.
int RunGMainLoopMaxIterations(int iterations);

// Allocates, initializes and returns a string GValue object.
GValue* GValueNewString(const char* str);

// Frees a GValue object and its allocated resources.
void GValueFree(gpointer arg);

}  // namespace test_utils


// Useful actions for test. These need to be defined in the
// chromeos_update_engine namespace.

class NoneType;

template<typename T>
class ObjectFeederAction;

template<typename T>
class ActionTraits<ObjectFeederAction<T>> {
 public:
  typedef T OutputObjectType;
  typedef NoneType InputObjectType;
};

// This is a simple Action class for testing. It feeds an object into
// another action.
template<typename T>
class ObjectFeederAction : public Action<ObjectFeederAction<T>> {
 public:
  typedef NoneType InputObjectType;
  typedef T OutputObjectType;
  void PerformAction() {
    LOG(INFO) << "feeder running!";
    CHECK(this->processor_);
    if (this->HasOutputPipe()) {
      this->SetOutputObject(out_obj_);
    }
    this->processor_->ActionComplete(this, ErrorCode::kSuccess);
  }
  static std::string StaticType() { return "ObjectFeederAction"; }
  std::string Type() const { return StaticType(); }
  void set_obj(const T& out_obj) {
    out_obj_ = out_obj;
  }
 private:
  T out_obj_;
};

template<typename T>
class ObjectCollectorAction;

template<typename T>
class ActionTraits<ObjectCollectorAction<T>> {
 public:
  typedef NoneType OutputObjectType;
  typedef T InputObjectType;
};

// This is a simple Action class for testing. It receives an object from
// another action.
template<typename T>
class ObjectCollectorAction : public Action<ObjectCollectorAction<T>> {
 public:
  typedef T InputObjectType;
  typedef NoneType OutputObjectType;
  void PerformAction() {
    LOG(INFO) << "collector running!";
    ASSERT_TRUE(this->processor_);
    if (this->HasInputObject()) {
      object_ = this->GetInputObject();
    }
    this->processor_->ActionComplete(this, ErrorCode::kSuccess);
  }
  static std::string StaticType() { return "ObjectCollectorAction"; }
  std::string Type() const { return StaticType(); }
  const T& object() const { return object_; }
 private:
  T object_;
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_TEST_UTILS_H_
