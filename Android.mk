#
# Copyright (C) 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

LOCAL_PATH := $(my-dir)

# Definitions applying to all targets. $(eval) this last.
define update_engine_common
    LOCAL_CPP_EXTENSION := .cc
    LOCAL_RTTI_FLAG := -frtti
    LOCAL_CLANG := true

    LOCAL_CFLAGS += \
        -DUSE_HWID_OVERRIDE=0 \
        -DUSE_MTD=0 \
        -DUSE_POWER_MANAGEMENT=0 \
        -D_FILE_OFFSET_BITS=64 \
        -D_POSIX_C_SOURCE=199309L \
        -Wa,--noexecstack \
        -Wall \
        -Werror \
        -Wextra \
        -Wformat=2 \
        -Wno-psabi \
        -Wno-unused-parameter \
        -ffunction-sections \
        -fstack-protector-strong \
        -fvisibility=hidden
    LOCAL_CPPFLAGS += \
        -Wnon-virtual-dtor \
        -fno-strict-aliasing \
        -std=gnu++11
    LOCAL_LDFLAGS += \
        -Wl,--gc-sections
    LOCAL_C_INCLUDES += \
        $(LOCAL_PATH)/client_library/include \
        external/gtest/include \
        system
    LOCAL_SHARED_LIBRARIES += \
        libchrome \
        libchrome-dbus \
        libchromeos \
        libchromeos-dbus \
        libchromeos-http \
        libchromeos-stream
endef

# update_metadata-protos (type: static_library)
# ========================================================
# Protobufs.
include $(CLEAR_VARS)
LOCAL_MODULE := update_metadata-protos
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
generated_sources_dir := $(call local-generated-sources-dir)
LOCAL_EXPORT_C_INCLUDE_DIRS += \
    $(generated_sources_dir)/proto/system
LOCAL_SHARED_LIBRARIES += \
    libprotobuf-cpp-lite-rtti
LOCAL_SRC_FILES := \
    update_metadata.proto
$(eval $(update_engine_common))
include $(BUILD_STATIC_LIBRARY)

# update_engine-dbus-adaptor (from generate-dbus-adaptors.gypi)
# ========================================================
include $(CLEAR_VARS)
LOCAL_MODULE := update_engine-dbus-adaptor
LOCAL_SRC_FILES := \
    dbus_bindings/org.chromium.UpdateEngineInterface.dbus-xml
include $(BUILD_STATIC_LIBRARY)

# update_engine-dbus-libcros-client (from generate-dbus-proxies.gypi)
# ========================================================
include $(CLEAR_VARS)
LOCAL_MODULE := update_engine-dbus-libcros-client
LOCAL_SRC_FILES := \
    dbus_bindings/org.chromium.LibCrosService.dbus-xml
LOCAL_DBUS_PROXY_PREFIX := libcros
include $(BUILD_STATIC_LIBRARY)

# libupdate_engine (type: static_library)
# ========================================================
# The main static_library with all the code.
include $(CLEAR_VARS)
LOCAL_MODULE := libupdate_engine
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_C_INCLUDES += \
    external/e2fsprogs/lib \
    $(LOCAL_PATH)/include
LOCAL_EXPORT_C_INCLUDE_DIRS += \
    $(LOCAL_PATH)/include
LOCAL_STATIC_LIBRARIES += \
    update_metadata-protos \
    update_engine-dbus-adaptor \
    update_engine-dbus-libcros-client \
    update_engine_client-dbus-proxies \
    libbz \
    libfs_mgr \
    libxz
LOCAL_SHARED_LIBRARIES += \
    libprotobuf-cpp-lite-rtti \
    libdbus \
    libcrypto \
    libcurl \
    libcutils \
    libhardware \
    libmetrics \
    libssl \
    libexpat \
    libchromeos-policy
LOCAL_SRC_FILES := \
    action_processor.cc \
    boot_control_android.cc \
    bzip.cc \
    bzip_extent_writer.cc \
    certificate_checker.cc \
    chrome_browser_proxy_resolver.cc \
    clock.cc \
    connection_manager.cc \
    constants.cc \
    daemon.cc \
    dbus_service.cc \
    delta_performer.cc \
    download_action.cc \
    extent_writer.cc \
    file_descriptor.cc \
    file_writer.cc \
    filesystem_verifier_action.cc \
    hardware_android.cc \
    http_common.cc \
    http_fetcher.cc \
    hwid_override.cc \
    install_plan.cc \
    libcros_proxy.cc \
    libcurl_http_fetcher.cc \
    metrics.cc \
    multi_range_http_fetcher.cc \
    omaha_hash_calculator.cc \
    omaha_request_action.cc \
    omaha_request_params.cc \
    omaha_response_handler_action.cc \
    p2p_manager.cc \
    payload_constants.cc \
    payload_state.cc \
    payload_verifier.cc \
    platform_constants_android.cc \
    postinstall_runner_action.cc \
    prefs.cc \
    proxy_resolver.cc \
    real_system_state.cc \
    shill_proxy.cc \
    subprocess.cc \
    terminator.cc \
    update_attempter.cc \
    update_manager/boxed_value.cc \
    update_manager/chromeos_policy.cc \
    update_manager/default_policy.cc \
    update_manager/evaluation_context.cc \
    update_manager/policy.cc \
    update_manager/real_config_provider.cc \
    update_manager/real_device_policy_provider.cc \
    update_manager/real_random_provider.cc \
    update_manager/real_shill_provider.cc \
    update_manager/real_system_provider.cc \
    update_manager/real_time_provider.cc \
    update_manager/real_updater_provider.cc \
    update_manager/state_factory.cc \
    update_manager/update_manager.cc \
    update_status_utils.cc \
    utils.cc \
    xz_extent_writer.cc
$(eval $(update_engine_common))
include $(BUILD_STATIC_LIBRARY)

# update_engine (type: executable)
# ========================================================
# update_engine daemon.
include $(CLEAR_VARS)
LOCAL_MODULE := update_engine
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_STATIC_LIBRARIES += \
    libupdate_engine \
    libbz \
    libfs_mgr \
    libxz \
    update_metadata-protos \
    update_engine-dbus-adaptor \
    update_engine-dbus-libcros-client \
    update_engine_client-dbus-proxies
LOCAL_SHARED_LIBRARIES += \
    libprotobuf-cpp-lite-rtti \
    libdbus \
    libcrypto \
    libcurl \
    libcutils \
    libhardware \
    libmetrics \
    libssl \
    libexpat \
    libchromeos-policy
LOCAL_SRC_FILES := \
    main.cc
LOCAL_INIT_RC := update_engine.rc
$(eval $(update_engine_common))
include $(BUILD_EXECUTABLE)

# update_engine_client (type: executable)
# ========================================================
# update_engine console client.
include $(CLEAR_VARS)
LOCAL_MODULE := update_engine_client
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/include
LOCAL_STATIC_LIBRARIES += \
    update_engine_client-dbus-proxies
LOCAL_SRC_FILES := \
    update_engine_client.cc
$(eval $(update_engine_common))
include $(BUILD_EXECUTABLE)

# libpayload_generator (type: static_library)
# ========================================================
# server-side code. This is used for delta_generator and unittests but not
# for any client code.
include $(CLEAR_VARS)
LOCAL_MODULE := libpayload_generator
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_STATIC_LIBRARIES += \
    libupdate_engine \
    libbz \
    libfs_mgr \
    libxz \
    update_metadata-protos \
    update_engine-dbus-adaptor \
    update_engine-dbus-libcros-client \
    update_engine_client-dbus-proxies \
    update_metadata-protos
LOCAL_SHARED_LIBRARIES += \
    libdbus \
    libcrypto \
    libcurl \
    libmetrics \
    libssl \
    libexpat \
    libchromeos-policy \
    libprotobuf-cpp-lite-rtti \
    libext2fs
LOCAL_SRC_FILES := \
    payload_generator/ab_generator.cc \
    payload_generator/annotated_operation.cc \
    payload_generator/blob_file_writer.cc \
    payload_generator/block_mapping.cc \
    payload_generator/cycle_breaker.cc \
    payload_generator/delta_diff_generator.cc \
    payload_generator/delta_diff_utils.cc \
    payload_generator/ext2_filesystem.cc \
    payload_generator/extent_ranges.cc \
    payload_generator/extent_utils.cc \
    payload_generator/full_update_generator.cc \
    payload_generator/graph_types.cc \
    payload_generator/graph_utils.cc \
    payload_generator/inplace_generator.cc \
    payload_generator/payload_file.cc \
    payload_generator/payload_generation_config.cc \
    payload_generator/payload_signer.cc \
    payload_generator/raw_filesystem.cc \
    payload_generator/tarjan.cc \
    payload_generator/topological_sort.cc
$(eval $(update_engine_common))
include $(BUILD_STATIC_LIBRARY)

# delta_generator (type: executable)
# ========================================================
# server-side delta generator.
include $(CLEAR_VARS)
LOCAL_MODULE := delta_generator
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_STATIC_LIBRARIES += \
    libpayload_generator \
    libupdate_engine \
    libbz \
    libfs_mgr \
    libxz \
    update_metadata-protos \
    update_engine-dbus-adaptor \
    update_engine-dbus-libcros-client \
    update_engine_client-dbus-proxies
LOCAL_SHARED_LIBRARIES += \
    libdbus \
    libcrypto \
    libcurl \
    libmetrics \
    libssl \
    libexpat \
    libchromeos-policy \
    libprotobuf-cpp-lite-rtti \
    libext2fs
LOCAL_SRC_FILES := \
    payload_generator/generate_delta_main.cc
$(eval $(update_engine_common))
include $(BUILD_EXECUTABLE)

# update_engine_client-dbus-proxies (from generate-dbus-proxies.gypi)
# ========================================================
include $(CLEAR_VARS)
LOCAL_MODULE := update_engine_client-dbus-proxies
LOCAL_SRC_FILES := \
    dbus_bindings/dbus-service-config.json \
    dbus_bindings/org.chromium.UpdateEngineInterface.dbus-xml
LOCAL_DBUS_PROXY_PREFIX := update_engine
include $(BUILD_STATIC_LIBRARY)


# Update payload signing public key.
# ========================================================
include $(CLEAR_VARS)
LOCAL_MODULE := brillo-update-payload-key
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/update_engine
LOCAL_MODULE_STEM := update-payload-key.pub.pem
LOCAL_SRC_FILES := update_payload_key/brillo-update-payload-key.pub.pem
LOCAL_BUILT_MODULE_STEM := update_payload_key/brillo-update-payload-key.pub.pem
include $(BUILD_PREBUILT)
