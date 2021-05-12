//
// Copyright (C) 2012 The Android Open Source Project
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

#include "update_engine/payload_consumer/filesystem_verifier_action.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include <base/bind.h>
#include <base/posix/eintr_wrapper.h>
#include <brillo/message_loops/fake_message_loop.h>
#include <brillo/message_loops/message_loop_utils.h>
#include <brillo/secure_blob.h>
#include <fec/ecc.h>
#include <gtest/gtest.h>
#include <libsnapshot/snapshot_writer.h>

#include "update_engine/common/dynamic_partition_control_stub.h"
#include "update_engine/common/hash_calculator.h"
#include "update_engine/common/mock_dynamic_partition_control.h"
#include "update_engine/common/test_utils.h"
#include "update_engine/common/utils.h"
#include "update_engine/payload_consumer/fake_file_descriptor.h"
#include "update_engine/payload_consumer/install_plan.h"
#include "update_engine/payload_consumer/verity_writer_android.h"

using brillo::MessageLoop;
using std::string;
using testing::_;
using testing::AtLeast;
using testing::DoAll;
using testing::NiceMock;
using testing::Return;
using testing::SetArgPointee;

namespace chromeos_update_engine {

class FilesystemVerifierActionTest : public ::testing::Test {
 public:
  static constexpr size_t BLOCK_SIZE = 4096;
  // We use SHA256 for testing, so hash size is 256bits / 8
  static constexpr size_t HASH_SIZE = 256 / 8;
  static constexpr size_t PARTITION_SIZE = BLOCK_SIZE * 1024;
  static constexpr size_t HASH_TREE_START_OFFSET = 800 * BLOCK_SIZE;
  size_t hash_tree_size = 0;
  size_t fec_start_offset = 0;
  size_t fec_data_size = 0;
  static constexpr size_t FEC_ROOTS = 2;
  size_t fec_rounds = 0;
  size_t fec_size = 0;

 protected:
  void SetUp() override {
    hash_tree_size = HashTreeBuilder::CalculateSize(
        HASH_TREE_START_OFFSET, BLOCK_SIZE, HASH_SIZE);
    fec_start_offset = HASH_TREE_START_OFFSET + hash_tree_size;
    fec_data_size = fec_start_offset;
    static constexpr size_t FEC_ROOTS = 2;
    fec_rounds =
        utils::DivRoundUp(fec_data_size / BLOCK_SIZE, FEC_RSM - FEC_ROOTS);
    fec_size = fec_rounds * FEC_ROOTS * BLOCK_SIZE;

    fec_data_.resize(fec_size);
    hash_tree_data_.resize(hash_tree_size);
    brillo::Blob part_data(PARTITION_SIZE);
    test_utils::FillWithData(&part_data);
    ASSERT_TRUE(utils::WriteFile(
        source_part_.path().c_str(), part_data.data(), part_data.size()));
    // FillWithData() will fill with different data next call. We want
    // source/target partitions to contain different data for testing.
    test_utils::FillWithData(&part_data);
    ASSERT_TRUE(utils::WriteFile(
        target_part_.path().c_str(), part_data.data(), part_data.size()));
    loop_.SetAsCurrent();
  }

  void TearDown() override {
    EXPECT_EQ(0, brillo::MessageLoopRunMaxIterations(&loop_, 1));
  }

  void DoTestVABC(bool clear_target_hash, bool enable_verity);

  // Returns true iff test has completed successfully.
  bool DoTest(bool terminate_early, bool hash_fail);

  void BuildActions(const InstallPlan& install_plan);
  void BuildActions(const InstallPlan& install_plan,
                    DynamicPartitionControlInterface* dynamic_control);

  InstallPlan::Partition* AddFakePartition(InstallPlan* install_plan,
                                           std::string name = "fake_part") {
    InstallPlan::Partition& part = install_plan->partitions.emplace_back();
    part.name = name;
    part.target_path = target_part_.path();
    part.readonly_target_path = part.target_path;
    part.target_size = PARTITION_SIZE;
    part.block_size = BLOCK_SIZE;
    part.source_path = source_part_.path();
    part.source_size = PARTITION_SIZE;
    EXPECT_TRUE(
        HashCalculator::RawHashOfFile(source_part_.path(), &part.source_hash));
    EXPECT_TRUE(
        HashCalculator::RawHashOfFile(target_part_.path(), &part.target_hash));
    return &part;
  }
  static void ZeroRange(FileDescriptorPtr fd,
                        size_t start_block,
                        size_t num_blocks) {
    std::vector<unsigned char> buffer(BLOCK_SIZE);
    ASSERT_EQ((ssize_t)(start_block * BLOCK_SIZE),
              fd->Seek(start_block * BLOCK_SIZE, SEEK_SET));
    for (size_t i = 0; i < num_blocks; i++) {
      ASSERT_TRUE(utils::WriteAll(fd, buffer.data(), buffer.size()));
    }
  }

  void SetHashWithVerity(InstallPlan::Partition* partition) {
    partition->hash_tree_algorithm = "sha256";
    partition->hash_tree_size = hash_tree_size;
    partition->hash_tree_offset = HASH_TREE_START_OFFSET;
    partition->hash_tree_data_offset = 0;
    partition->hash_tree_data_size = HASH_TREE_START_OFFSET;
    partition->fec_size = fec_size;
    partition->fec_offset = fec_start_offset;
    partition->fec_data_offset = 0;
    partition->fec_data_size = fec_data_size;
    partition->fec_roots = FEC_ROOTS;
    VerityWriterAndroid verity_writer;
    ASSERT_TRUE(verity_writer.Init(*partition));
    LOG(INFO) << "Opening " << partition->readonly_target_path;
    auto fd = std::make_shared<EintrSafeFileDescriptor>();
    ASSERT_TRUE(fd->Open(partition->readonly_target_path.c_str(), O_RDWR))
        << "Failed to open " << partition->target_path.c_str() << " "
        << strerror(errno);
    std::vector<unsigned char> buffer(BLOCK_SIZE);
    // Only need to read up to hash tree
    auto bytes_to_read = HASH_TREE_START_OFFSET;
    auto offset = 0;
    while (bytes_to_read > 0) {
      const auto bytes_read = fd->Read(
          buffer.data(), std::min<size_t>(buffer.size(), bytes_to_read));
      ASSERT_GT(bytes_read, 0)
          << "offset: " << offset << " bytes to read: " << bytes_to_read
          << " error: " << strerror(errno);
      ASSERT_TRUE(verity_writer.Update(offset, buffer.data(), bytes_read));
      bytes_to_read -= bytes_read;
      offset += bytes_read;
    }
    ASSERT_TRUE(verity_writer.Finalize(fd, fd));
    ASSERT_TRUE(fd->IsOpen());
    ASSERT_TRUE(HashCalculator::RawHashOfFile(target_part_.path(),
                                              &partition->target_hash));

    ASSERT_TRUE(fd->Seek(HASH_TREE_START_OFFSET, SEEK_SET));
    ASSERT_EQ(fd->Read(hash_tree_data_.data(), hash_tree_data_.size()),
              static_cast<ssize_t>(hash_tree_data_.size()))
        << "Failed to read hashtree " << strerror(errno);
    ASSERT_TRUE(fd->Seek(fec_start_offset, SEEK_SET));
    ASSERT_EQ(fd->Read(fec_data_.data(), fec_data_.size()),
              static_cast<ssize_t>(fec_data_.size()))
        << "Failed to read FEC " << strerror(errno);
    // Fs verification action is expected to write them, so clear verity data to
    // ensure that they are re-created correctly.
    ZeroRange(
        fd, HASH_TREE_START_OFFSET / BLOCK_SIZE, hash_tree_size / BLOCK_SIZE);
    ZeroRange(fd, fec_start_offset / BLOCK_SIZE, fec_size / BLOCK_SIZE);
  }

  brillo::FakeMessageLoop loop_{nullptr};
  ActionProcessor processor_;
  DynamicPartitionControlStub dynamic_control_stub_;
  std::vector<unsigned char> fec_data_;
  std::vector<unsigned char> hash_tree_data_;
  static ScopedTempFile source_part_;
  static ScopedTempFile target_part_;
  InstallPlan install_plan_;
};

ScopedTempFile FilesystemVerifierActionTest::source_part_{
    "source_part.XXXXXX", false, PARTITION_SIZE};
ScopedTempFile FilesystemVerifierActionTest::target_part_{
    "target_part.XXXXXX", false, PARTITION_SIZE};

static void EnableVABC(MockDynamicPartitionControl* dynamic_control,
                       const std::string& part_name) {
  ON_CALL(*dynamic_control, GetDynamicPartitionsFeatureFlag())
      .WillByDefault(Return(FeatureFlag(FeatureFlag::Value::LAUNCH)));
  ON_CALL(*dynamic_control, UpdateUsesSnapshotCompression())
      .WillByDefault(Return(true));
  ON_CALL(*dynamic_control, IsDynamicPartition(part_name, _))
      .WillByDefault(Return(true));
}

class FilesystemVerifierActionTestDelegate : public ActionProcessorDelegate {
 public:
  FilesystemVerifierActionTestDelegate()
      : ran_(false), code_(ErrorCode::kError) {}

  void ProcessingDone(const ActionProcessor* processor, ErrorCode code) {
    MessageLoop::current()->BreakLoop();
  }
  void ProcessingStopped(const ActionProcessor* processor) {
    MessageLoop::current()->BreakLoop();
  }
  void ActionCompleted(ActionProcessor* processor,
                       AbstractAction* action,
                       ErrorCode code) {
    if (action->Type() == FilesystemVerifierAction::StaticType()) {
      ran_ = true;
      code_ = code;
      EXPECT_FALSE(
          static_cast<FilesystemVerifierAction*>(action)->partition_fd_);
    } else if (action->Type() ==
               ObjectCollectorAction<InstallPlan>::StaticType()) {
      auto collector_action =
          static_cast<ObjectCollectorAction<InstallPlan>*>(action);
      install_plan_.reset(new InstallPlan(collector_action->object()));
    }
  }
  bool ran() const { return ran_; }
  ErrorCode code() const { return code_; }

  std::unique_ptr<InstallPlan> install_plan_;

 private:
  bool ran_;
  ErrorCode code_;
};

bool FilesystemVerifierActionTest::DoTest(bool terminate_early,
                                          bool hash_fail) {
  ScopedTempFile a_loop_file("a_loop_file.XXXXXX");

  // Make random data for a.
  const size_t kLoopFileSize = 10 * 1024 * 1024 + 512;
  brillo::Blob a_loop_data(kLoopFileSize);
  test_utils::FillWithData(&a_loop_data);

  // Write data to disk
  if (!(test_utils::WriteFileVector(a_loop_file.path(), a_loop_data))) {
    ADD_FAILURE();
    return false;
  }

  // Attach loop devices to the files
  string a_dev;
  test_utils::ScopedLoopbackDeviceBinder a_dev_releaser(
      a_loop_file.path(), false, &a_dev);
  if (!(a_dev_releaser.is_bound())) {
    ADD_FAILURE();
    return false;
  }

  LOG(INFO) << "verifying: " << a_loop_file.path() << " (" << a_dev << ")";

  bool success = true;

  // Set up the action objects
  install_plan_.source_slot = 0;
  install_plan_.target_slot = 1;
  InstallPlan::Partition part;
  part.name = "part";
  part.target_size = kLoopFileSize - (hash_fail ? 1 : 0);
  part.target_path = a_dev;
  if (!HashCalculator::RawHashOfData(a_loop_data, &part.target_hash)) {
    ADD_FAILURE();
    success = false;
  }
  part.source_size = kLoopFileSize;
  part.source_path = a_dev;
  if (!HashCalculator::RawHashOfData(a_loop_data, &part.source_hash)) {
    ADD_FAILURE();
    success = false;
  }
  install_plan_.partitions = {part};

  BuildActions(install_plan_);

  FilesystemVerifierActionTestDelegate delegate;
  processor_.set_delegate(&delegate);

  loop_.PostTask(FROM_HERE,
                 base::Bind(
                     [](ActionProcessor* processor, bool terminate_early) {
                       processor->StartProcessing();
                       if (terminate_early) {
                         processor->StopProcessing();
                       }
                     },
                     base::Unretained(&processor_),
                     terminate_early));
  loop_.Run();

  if (!terminate_early) {
    bool is_delegate_ran = delegate.ran();
    EXPECT_TRUE(is_delegate_ran);
    success = success && is_delegate_ran;
  } else {
    EXPECT_EQ(ErrorCode::kError, delegate.code());
    return (ErrorCode::kError == delegate.code());
  }
  if (hash_fail) {
    ErrorCode expected_exit_code = ErrorCode::kNewRootfsVerificationError;
    EXPECT_EQ(expected_exit_code, delegate.code());
    return (expected_exit_code == delegate.code());
  }
  EXPECT_EQ(ErrorCode::kSuccess, delegate.code());

  // Make sure everything in the out_image is there
  brillo::Blob a_out;
  if (!utils::ReadFile(a_dev, &a_out)) {
    ADD_FAILURE();
    return false;
  }
  const bool is_a_file_reading_eq =
      test_utils::ExpectVectorsEq(a_loop_data, a_out);
  EXPECT_TRUE(is_a_file_reading_eq);
  success = success && is_a_file_reading_eq;

  bool is_install_plan_eq = (*delegate.install_plan_ == install_plan_);
  EXPECT_TRUE(is_install_plan_eq);
  success = success && is_install_plan_eq;
  return success;
}

void FilesystemVerifierActionTest::BuildActions(
    const InstallPlan& install_plan,
    DynamicPartitionControlInterface* dynamic_control) {
  auto feeder_action = std::make_unique<ObjectFeederAction<InstallPlan>>();
  auto verifier_action =
      std::make_unique<FilesystemVerifierAction>(dynamic_control);
  auto collector_action =
      std::make_unique<ObjectCollectorAction<InstallPlan>>();

  feeder_action->set_obj(install_plan);

  BondActions(feeder_action.get(), verifier_action.get());
  BondActions(verifier_action.get(), collector_action.get());

  processor_.EnqueueAction(std::move(feeder_action));
  processor_.EnqueueAction(std::move(verifier_action));
  processor_.EnqueueAction(std::move(collector_action));
}

void FilesystemVerifierActionTest::BuildActions(
    const InstallPlan& install_plan) {
  BuildActions(install_plan, &dynamic_control_stub_);
}

class FilesystemVerifierActionTest2Delegate : public ActionProcessorDelegate {
 public:
  void ActionCompleted(ActionProcessor* processor,
                       AbstractAction* action,
                       ErrorCode code) {
    if (action->Type() == FilesystemVerifierAction::StaticType()) {
      ran_ = true;
      code_ = code;
    }
  }
  bool ran_;
  ErrorCode code_;
};

TEST_F(FilesystemVerifierActionTest, MissingInputObjectTest) {
  auto copier_action =
      std::make_unique<FilesystemVerifierAction>(&dynamic_control_stub_);
  auto collector_action =
      std::make_unique<ObjectCollectorAction<InstallPlan>>();

  BondActions(copier_action.get(), collector_action.get());

  processor_.EnqueueAction(std::move(copier_action));
  processor_.EnqueueAction(std::move(collector_action));

  FilesystemVerifierActionTest2Delegate delegate;
  processor_.set_delegate(&delegate);

  processor_.StartProcessing();
  ASSERT_FALSE(processor_.IsRunning());
  ASSERT_TRUE(delegate.ran_);
  EXPECT_EQ(ErrorCode::kError, delegate.code_);
}

TEST_F(FilesystemVerifierActionTest, NonExistentDriveTest) {
  InstallPlan::Partition part;
  part.name = "nope";
  part.source_path = "/no/such/file";
  part.target_path = "/no/such/file";
  install_plan_.partitions = {part};

  BuildActions(install_plan_);

  FilesystemVerifierActionTest2Delegate delegate;
  processor_.set_delegate(&delegate);

  processor_.StartProcessing();
  EXPECT_FALSE(processor_.IsRunning());
  EXPECT_TRUE(delegate.ran_);
  EXPECT_EQ(ErrorCode::kFilesystemVerifierError, delegate.code_);
}

TEST_F(FilesystemVerifierActionTest, RunAsRootVerifyHashTest) {
  ASSERT_EQ(0U, getuid());
  EXPECT_TRUE(DoTest(false, false));
}

TEST_F(FilesystemVerifierActionTest, RunAsRootVerifyHashFailTest) {
  ASSERT_EQ(0U, getuid());
  EXPECT_TRUE(DoTest(false, true));
}

TEST_F(FilesystemVerifierActionTest, RunAsRootTerminateEarlyTest) {
  ASSERT_EQ(0U, getuid());
  EXPECT_TRUE(DoTest(true, false));
  // TerminateEarlyTest may leak some null callbacks from the Stream class.
  while (loop_.RunOnce(false)) {
  }
}

#ifdef __ANDROID__
TEST_F(FilesystemVerifierActionTest, RunAsRootWriteVerityTest) {
  ScopedTempFile part_file("part_file.XXXXXX");
  constexpr size_t filesystem_size = 200 * 4096;
  constexpr size_t part_size = 256 * 4096;
  brillo::Blob part_data(filesystem_size, 0x1);
  part_data.resize(part_size);
  ASSERT_TRUE(test_utils::WriteFileVector(part_file.path(), part_data));
  string target_path;
  test_utils::ScopedLoopbackDeviceBinder target_device(
      part_file.path(), true, &target_path);

  InstallPlan::Partition part;
  part.name = "part";
  part.target_path = target_path;
  part.target_size = part_size;
  part.block_size = 4096;
  part.hash_tree_algorithm = "sha1";
  part.hash_tree_data_offset = 0;
  part.hash_tree_data_size = filesystem_size;
  part.hash_tree_offset = filesystem_size;
  part.hash_tree_size = 3 * 4096;
  part.fec_data_offset = 0;
  part.fec_data_size = filesystem_size + part.hash_tree_size;
  part.fec_offset = part.fec_data_size;
  part.fec_size = 2 * 4096;
  part.fec_roots = 2;
  // for i in {1..$((200 * 4096))}; do echo -n -e '\x1' >> part; done
  // avbtool add_hashtree_footer --image part --partition_size $((256 * 4096))
  //     --partition_name part --do_not_append_vbmeta_image
  //     --output_vbmeta_image vbmeta
  // truncate -s $((256 * 4096)) part
  // sha256sum part | xxd -r -p | hexdump -v -e '/1 "0x%02x, "'
  part.target_hash = {0x28, 0xd4, 0x96, 0x75, 0x4c, 0xf5, 0x8a, 0x3e,
                      0x31, 0x85, 0x08, 0x92, 0x85, 0x62, 0xf0, 0x37,
                      0xbc, 0x8d, 0x7e, 0xa4, 0xcb, 0x24, 0x18, 0x7b,
                      0xf3, 0xeb, 0xb5, 0x8d, 0x6f, 0xc8, 0xd8, 0x1a};
  // avbtool info_image --image vbmeta | grep Salt | cut -d':' -f 2 |
  //     xxd -r -p | hexdump -v -e '/1 "0x%02x, "'
  part.hash_tree_salt = {0x9e, 0xcb, 0xf8, 0xd5, 0x0b, 0xb4, 0x43,
                         0x0a, 0x7a, 0x10, 0xad, 0x96, 0xd7, 0x15,
                         0x70, 0xba, 0xed, 0x27, 0xe2, 0xae};
  install_plan_.partitions = {part};

  BuildActions(install_plan_);

  FilesystemVerifierActionTestDelegate delegate;
  processor_.set_delegate(&delegate);

  loop_.PostTask(
      FROM_HERE,
      base::Bind(
          [](ActionProcessor* processor) { processor->StartProcessing(); },
          base::Unretained(&processor_)));
  loop_.Run();

  EXPECT_FALSE(processor_.IsRunning());
  EXPECT_TRUE(delegate.ran());
  EXPECT_EQ(ErrorCode::kSuccess, delegate.code());
}
#endif  // __ANDROID__

TEST_F(FilesystemVerifierActionTest, RunAsRootSkipWriteVerityTest) {
  ScopedTempFile part_file("part_file.XXXXXX");
  constexpr size_t filesystem_size = 200 * 4096;
  constexpr size_t part_size = 256 * 4096;
  brillo::Blob part_data(part_size);
  test_utils::FillWithData(&part_data);
  ASSERT_TRUE(test_utils::WriteFileVector(part_file.path(), part_data));
  string target_path;
  test_utils::ScopedLoopbackDeviceBinder target_device(
      part_file.path(), true, &target_path);

  install_plan_.write_verity = false;
  InstallPlan::Partition part;
  part.name = "part";
  part.target_path = target_path;
  part.target_size = part_size;
  part.block_size = 4096;
  part.hash_tree_data_offset = 0;
  part.hash_tree_data_size = filesystem_size;
  part.hash_tree_offset = filesystem_size;
  part.hash_tree_size = 3 * 4096;
  part.fec_data_offset = 0;
  part.fec_data_size = filesystem_size + part.hash_tree_size;
  part.fec_offset = part.fec_data_size;
  part.fec_size = 2 * 4096;
  EXPECT_TRUE(HashCalculator::RawHashOfData(part_data, &part.target_hash));
  install_plan_.partitions = {part};

  BuildActions(install_plan_);

  FilesystemVerifierActionTestDelegate delegate;
  processor_.set_delegate(&delegate);

  loop_.PostTask(
      FROM_HERE,
      base::Bind(
          [](ActionProcessor* processor) { processor->StartProcessing(); },
          base::Unretained(&processor_)));
  loop_.Run();

  ASSERT_FALSE(processor_.IsRunning());
  ASSERT_TRUE(delegate.ran());
  ASSERT_EQ(ErrorCode::kSuccess, delegate.code());
}

void FilesystemVerifierActionTest::DoTestVABC(bool clear_target_hash,
                                              bool enable_verity) {
  auto part_ptr = AddFakePartition(&install_plan_);
  if (::testing::Test::HasFailure()) {
    return;
  }
  ASSERT_NE(part_ptr, nullptr);
  InstallPlan::Partition& part = *part_ptr;
  part.target_path = "Shouldn't attempt to open this path";
  if (enable_verity) {
    install_plan_.write_verity = true;
    ASSERT_NO_FATAL_FAILURE(SetHashWithVerity(&part));
  }
  if (clear_target_hash) {
    part.target_hash.clear();
  }

  NiceMock<MockDynamicPartitionControl> dynamic_control;

  EnableVABC(&dynamic_control, part.name);

  EXPECT_CALL(dynamic_control, UpdateUsesSnapshotCompression())
      .Times(AtLeast(1));
  auto cow_fd = std::make_shared<EintrSafeFileDescriptor>();
  ASSERT_TRUE(cow_fd->Open(part.readonly_target_path.c_str(), O_RDWR));
  if (enable_verity) {
    ON_CALL(dynamic_control, OpenCowFd(part.name, {part.source_path}, _))
        .WillByDefault(Return(cow_fd));
    EXPECT_CALL(dynamic_control, OpenCowFd(part.name, {part.source_path}, _))
        .Times(AtLeast(1));
    // If we are writing verity, fs verification shouldn't try to open fd
    // against |postinstall_mount_device| or |target_path| at all. It should
    // just use whatever file descriptor returned by OpenCowFd(). Therefore set
    // a fake path to prevent fs verification from trying to open it.
    part.readonly_target_path = "/dev/fake_postinstall_mount_device";
  } else {
    // Since we are not writing verity, we should not attempt to OpenCowFd()
    // reads should go through regular file descriptors on mapped partitions.
    EXPECT_CALL(dynamic_control, OpenCowFd(part.name, {part.source_path}, _))
        .Times(0);
    EXPECT_CALL(dynamic_control, MapAllPartitions()).Times(AtLeast(1));
  }
  EXPECT_CALL(dynamic_control, ListDynamicPartitionsForSlot(_, _, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<2, std::vector<std::string>>({part.name}),
                Return(true)));

  BuildActions(install_plan_, &dynamic_control);

  FilesystemVerifierActionTestDelegate delegate;
  processor_.set_delegate(&delegate);

  loop_.PostTask(
      FROM_HERE,
      base::Bind(
          [](ActionProcessor* processor) { processor->StartProcessing(); },
          base::Unretained(&processor_)));
  loop_.Run();

  ASSERT_FALSE(processor_.IsRunning());
  ASSERT_TRUE(delegate.ran());
  if (enable_verity) {
    std::vector<unsigned char> actual_fec(fec_size);
    ssize_t bytes_read = 0;
    ASSERT_TRUE(utils::PReadAll(cow_fd,
                                actual_fec.data(),
                                actual_fec.size(),
                                fec_start_offset,
                                &bytes_read));
    ASSERT_EQ(actual_fec, fec_data_);
    std::vector<unsigned char> actual_hash_tree(hash_tree_size);
    ASSERT_TRUE(utils::PReadAll(cow_fd,
                                actual_hash_tree.data(),
                                actual_hash_tree.size(),
                                HASH_TREE_START_OFFSET,
                                &bytes_read));
    ASSERT_EQ(actual_hash_tree, hash_tree_data_);
  }
  if (clear_target_hash) {
    ASSERT_EQ(ErrorCode::kNewRootfsVerificationError, delegate.code());
  } else {
    ASSERT_EQ(ErrorCode::kSuccess, delegate.code());
  }
}

TEST_F(FilesystemVerifierActionTest, VABC_NoVerity_Success) {
  DoTestVABC(false, false);
}

TEST_F(FilesystemVerifierActionTest, VABC_NoVerity_Target_Mismatch) {
  DoTestVABC(true, false);
}

TEST_F(FilesystemVerifierActionTest, VABC_Verity_Success) {
  DoTestVABC(false, true);
}

TEST_F(FilesystemVerifierActionTest, VABC_Verity_ReadAfterWrite) {
  ASSERT_NO_FATAL_FAILURE(DoTestVABC(false, true));
  // Run FS verification again, w/o writing verity. We have seen a bug where
  // attempting to run fs again will cause previously written verity data to be
  // dropped, so cover this scenario.
  ASSERT_GE(install_plan_.partitions.size(), 1UL);
  auto& part = install_plan_.partitions[0];
  install_plan_.write_verity = false;
  part.readonly_target_path = target_part_.path();
  NiceMock<MockDynamicPartitionControl> dynamic_control;
  EnableVABC(&dynamic_control, part.name);

  // b/186196758 is only visible if we repeatedely run FS verification w/o
  // writing verity
  for (int i = 0; i < 3; i++) {
    BuildActions(install_plan_, &dynamic_control);

    FilesystemVerifierActionTestDelegate delegate;
    processor_.set_delegate(&delegate);
    loop_.PostTask(
        FROM_HERE,
        base::Bind(
            [](ActionProcessor* processor) { processor->StartProcessing(); },
            base::Unretained(&processor_)));
    loop_.Run();
    ASSERT_FALSE(processor_.IsRunning());
    ASSERT_TRUE(delegate.ran());
    ASSERT_EQ(ErrorCode::kSuccess, delegate.code());
  }
}

TEST_F(FilesystemVerifierActionTest, VABC_Verity_Target_Mismatch) {
  DoTestVABC(true, true);
}

}  // namespace chromeos_update_engine
